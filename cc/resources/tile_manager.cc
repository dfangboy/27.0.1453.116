// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/tile_manager.h"

#include <algorithm>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/resources/platform_color.h"
#include "cc/resources/raster_worker_pool.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/tile.h"
#include "third_party/skia/include/core/SkDevice.h"

namespace cc {

namespace {

// If we raster too fast we become upload bound, and pending
// uploads consume memory. For maximum upload throughput, we would
// want to allow for upload_throughput * pipeline_time of pending
// uploads, after which we are just wasting memory. Since we don't
// know our upload throughput yet, this just caps our memory usage.
#if defined(OS_ANDROID)
// For reference, the Nexus10 can upload 1MB in about 2.5ms.
// Assuming a three frame deep pipeline this implies ~20MB.
const int kMaxPendingUploadBytes = 20 * 1024 * 1024;
// TODO(epenner): We should remove this upload limit (crbug.com/176197)
const int kMaxPendingUploads = 72;
#else
const int kMaxPendingUploadBytes = 100 * 1024 * 1024;
const int kMaxPendingUploads = 1000;
#endif

#if defined(OS_ANDROID)
const int kMaxNumPendingTasksPerThread = 8;
#else
const int kMaxNumPendingTasksPerThread = 40;
#endif

// Limit for time spent running cheap tasks during a single frame.
// TODO(skyostil): Determine this limit more dynamically.
const int kRunCheapTasksTimeMs = 6;

// Determine bin based on three categories of tiles: things we need now,
// things we need soon, and eventually.
inline TileManagerBin BinFromTilePriority(
  const TilePriority& prio,
  float max_distance_in_content_space) {
  if (!prio.is_live)
    return NEVER_BIN;

  // The amount of time for which we want to have prepainting coverage.
  const float kPrepaintingWindowTimeSeconds = 1.0f;
  const float kBackflingGuardDistancePixels = 314.0f;

  // Explicitly limit how far ahead we will prepaint to limit memory usage.
  if (prio.distance_to_visible_in_pixels > max_distance_in_content_space)
    return NEVER_BIN;

  if (prio.time_to_visible_in_seconds == 0 ||
      prio.distance_to_visible_in_pixels < kBackflingGuardDistancePixels)
    return NOW_BIN;

  if (prio.resolution == NON_IDEAL_RESOLUTION)
    return EVENTUALLY_BIN;

  if (prio.time_to_visible_in_seconds < kPrepaintingWindowTimeSeconds)
    return SOON_BIN;

  return EVENTUALLY_BIN;
}

std::string ValueToString(scoped_ptr<base::Value> value) {
  std::string str;
  base::JSONWriter::Write(value.get(), &str);
  return str;
}

}  // namespace

scoped_ptr<base::Value> TileManagerBinAsValue(TileManagerBin bin) {
  switch (bin) {
  case NOW_BIN:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "NOW_BIN"));
  case SOON_BIN:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "SOON_BIN"));
  case EVENTUALLY_BIN:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "EVENTUALLY_BIN"));
  case NEVER_BIN:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "NEVER_BIN"));
  default:
      DCHECK(false) << "Unrecognized TileManagerBin value " << bin;
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "<unknown TileManagerBin value>"));
  }
}

scoped_ptr<base::Value> TileManagerBinPriorityAsValue(
    TileManagerBinPriority bin_priority) {
  switch (bin_priority) {
  case HIGH_PRIORITY_BIN:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "HIGH_PRIORITY_BIN"));
  case LOW_PRIORITY_BIN:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "LOW_PRIORITY_BIN"));
  default:
      DCHECK(false) << "Unrecognized TileManagerBinPriority value";
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "<unknown TileManagerBinPriority value>"));
  }
}

scoped_ptr<base::Value> TileRasterStateAsValue(
    TileRasterState raster_state) {
  switch (raster_state) {
  case IDLE_STATE:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "IDLE_STATE"));
  case WAITING_FOR_RASTER_STATE:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "WAITING_FOR_RASTER_STATE"));
  case RASTER_STATE:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "RASTER_STATE"));
  case UPLOAD_STATE:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "UPLOAD_STATE"));
  case FORCED_UPLOAD_COMPLETION_STATE:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "FORCED_UPLOAD_COMPLETION_STATE"));
  default:
      DCHECK(false) << "Unrecognized TileRasterState value";
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "<unknown TileRasterState value>"));
  }
}

TileManager::TileManager(
    TileManagerClient* client,
    ResourceProvider* resource_provider,
    size_t num_raster_threads,
    size_t max_prepaint_tile_distance,
    bool use_cheapness_estimator,
    bool use_color_estimator,
    bool prediction_benchmarking,
    RenderingStatsInstrumentation* rendering_stats_instrumentation)
    : client_(client),
      resource_pool_(ResourcePool::Create(resource_provider)),
      raster_worker_pool_(RasterWorkerPool::Create(this, num_raster_threads)),
      manage_tiles_pending_(false),
      manage_tiles_call_count_(0),
      bytes_pending_upload_(0),
      has_performed_uploads_since_last_flush_(false),
      ever_exceeded_memory_budget_(false),
      max_prepaint_tile_distance_(max_prepaint_tile_distance),
      use_cheapness_estimator_(use_cheapness_estimator),
      use_color_estimator_(use_color_estimator),
      prediction_benchmarking_(prediction_benchmarking),
      pending_tasks_(0),
      max_pending_tasks_(kMaxNumPendingTasksPerThread * num_raster_threads),
      rendering_stats_instrumentation_(rendering_stats_instrumentation) {
  for (int i = 0; i < NUM_STATES; ++i) {
    for (int j = 0; j < NUM_TREES; ++j) {
      for (int k = 0; k < NUM_BINS; ++k)
        raster_state_count_[i][j][k] = 0;
    }
  }
}

TileManager::~TileManager() {
  // Reset global state and manage. This should cause
  // our memory usage to drop to zero.
  global_state_ = GlobalStateThatImpactsTilePriority();
  AssignGpuMemoryToTiles();
  // This should finish all pending tasks and release any uninitialized
  // resources.
  raster_worker_pool_.reset();
  AbortPendingTileUploads();
  DCHECK_EQ(tiles_with_pending_upload_.size(), 0);
  DCHECK_EQ(all_tiles_.size(), 0);
  DCHECK_EQ(live_or_allocated_tiles_.size(), 0);
}

void TileManager::SetGlobalState(
    const GlobalStateThatImpactsTilePriority& global_state) {
  global_state_ = global_state;
  resource_pool_->SetMaxMemoryUsageBytes(
      global_state_.memory_limit_in_bytes,
      global_state_.unused_memory_limit_in_bytes);
  ScheduleManageTiles();
  UpdateCheapTasksTimeLimit();
}

void TileManager::RegisterTile(Tile* tile) {
  all_tiles_.insert(tile);

  const ManagedTileState& mts = tile->managed_state();
  for (int i = 0; i < NUM_TREES; ++i)
    ++raster_state_count_[mts.raster_state][i][mts.tree_bin[i]];

  ScheduleManageTiles();
}

void TileManager::UnregisterTile(Tile* tile) {
  for (TileList::iterator it = tiles_with_image_decoding_tasks_.begin();
       it != tiles_with_image_decoding_tasks_.end(); it++) {
    if (*it == tile) {
      tiles_with_image_decoding_tasks_.erase(it);
      break;
    }
  }
  for (TileVector::iterator it = tiles_that_need_to_be_rasterized_.begin();
       it != tiles_that_need_to_be_rasterized_.end(); it++) {
    if (*it == tile) {
      tiles_that_need_to_be_rasterized_.erase(it);
      break;
    }
  }
  for (TileVector::iterator it = live_or_allocated_tiles_.begin();
       it != live_or_allocated_tiles_.end(); it++) {
    if (*it == tile) {
      live_or_allocated_tiles_.erase(it);
      break;
    }
  }
  TileSet::iterator it = all_tiles_.find(tile);
  DCHECK(it != all_tiles_.end());
  const ManagedTileState& mts = tile->managed_state();
  for (int i = 0; i < NUM_TREES; ++i)
    --raster_state_count_[mts.raster_state][i][mts.tree_bin[i]];
  FreeResourcesForTile(tile);
  all_tiles_.erase(it);
}

class BinComparator {
 public:
  bool operator() (const Tile* a, const Tile* b) const {
    const ManagedTileState& ams = a->managed_state();
    const ManagedTileState& bms = b->managed_state();
    if (ams.bin[HIGH_PRIORITY_BIN] != bms.bin[HIGH_PRIORITY_BIN])
      return ams.bin[HIGH_PRIORITY_BIN] < bms.bin[HIGH_PRIORITY_BIN];

    if (ams.bin[LOW_PRIORITY_BIN] != bms.bin[LOW_PRIORITY_BIN])
      return ams.bin[LOW_PRIORITY_BIN] < bms.bin[LOW_PRIORITY_BIN];

    if (ams.resolution != bms.resolution)
      return ams.resolution < bms.resolution;

    if (ams.time_to_needed_in_seconds !=  bms.time_to_needed_in_seconds)
      return ams.time_to_needed_in_seconds < bms.time_to_needed_in_seconds;

    if (ams.distance_to_visible_in_pixels !=
        bms.distance_to_visible_in_pixels) {
      return ams.distance_to_visible_in_pixels <
             bms.distance_to_visible_in_pixels;
    }

    gfx::Rect a_rect = a->content_rect();
    gfx::Rect b_rect = b->content_rect();
    if (a_rect.y() != b_rect.y())
      return a_rect.y() < b_rect.y();
    return a_rect.x() < b_rect.x();
  }
};

void TileManager::SortTiles() {
  TRACE_EVENT0("cc", "TileManager::SortTiles");
  TRACE_COUNTER_ID1(
      "cc", "LiveTileCount", this, live_or_allocated_tiles_.size());

  // Sort by bin, resolution and time until needed.
  std::sort(live_or_allocated_tiles_.begin(),
            live_or_allocated_tiles_.end(), BinComparator());
}

void TileManager::ManageTiles() {
  TRACE_EVENT0("cc", "TileManager::ManageTiles");
  manage_tiles_pending_ = false;
  ++manage_tiles_call_count_;

  const TreePriority tree_priority = global_state_.tree_priority;
  TRACE_COUNTER_ID1("cc", "TileCount", this, all_tiles_.size());

  // Memory limit policy works by mapping some bin states to the NEVER bin.
  TileManagerBin bin_map[NUM_BINS];
  if (global_state_.memory_limit_policy == ALLOW_NOTHING) {
    bin_map[NOW_BIN] = NEVER_BIN;
    bin_map[SOON_BIN] = NEVER_BIN;
    bin_map[EVENTUALLY_BIN] = NEVER_BIN;
    bin_map[NEVER_BIN] = NEVER_BIN;
  } else if (global_state_.memory_limit_policy == ALLOW_ABSOLUTE_MINIMUM) {
    bin_map[NOW_BIN] = NOW_BIN;
    bin_map[SOON_BIN] = NEVER_BIN;
    bin_map[EVENTUALLY_BIN] = NEVER_BIN;
    bin_map[NEVER_BIN] = NEVER_BIN;
  } else if (global_state_.memory_limit_policy == ALLOW_PREPAINT_ONLY) {
    bin_map[NOW_BIN] = NOW_BIN;
    bin_map[SOON_BIN] = SOON_BIN;
    bin_map[EVENTUALLY_BIN] = NEVER_BIN;
    bin_map[NEVER_BIN] = NEVER_BIN;
  } else {
    bin_map[NOW_BIN] = NOW_BIN;
    bin_map[SOON_BIN] = SOON_BIN;
    bin_map[EVENTUALLY_BIN] = EVENTUALLY_BIN;
    bin_map[NEVER_BIN] = NEVER_BIN;
  }

  live_or_allocated_tiles_.clear();
  // For each tree, bin into different categories of tiles.
  for (TileSet::iterator it = all_tiles_.begin();
       it != all_tiles_.end(); ++it) {
    Tile* tile = *it;
    ManagedTileState& mts = tile->managed_state();

    TilePriority prio[NUM_BIN_PRIORITIES];
    switch (tree_priority) {
      case SAME_PRIORITY_FOR_BOTH_TREES:
        prio[HIGH_PRIORITY_BIN] = prio[LOW_PRIORITY_BIN] =
            tile->combined_priority();
        break;
      case SMOOTHNESS_TAKES_PRIORITY:
        prio[HIGH_PRIORITY_BIN] = tile->priority(ACTIVE_TREE);
        prio[LOW_PRIORITY_BIN] = tile->priority(PENDING_TREE);
        break;
      case NEW_CONTENT_TAKES_PRIORITY:
        prio[HIGH_PRIORITY_BIN] = tile->priority(PENDING_TREE);
        prio[LOW_PRIORITY_BIN] = tile->priority(ACTIVE_TREE);
        break;
    }

    mts.resolution = prio[HIGH_PRIORITY_BIN].resolution;
    mts.time_to_needed_in_seconds =
        prio[HIGH_PRIORITY_BIN].time_to_visible_in_seconds;
    mts.distance_to_visible_in_pixels =
        prio[HIGH_PRIORITY_BIN].distance_to_visible_in_pixels;
    mts.bin[HIGH_PRIORITY_BIN] = BinFromTilePriority(
        prio[HIGH_PRIORITY_BIN],
        max_prepaint_tile_distance_);
    mts.bin[LOW_PRIORITY_BIN] = BinFromTilePriority(
        prio[LOW_PRIORITY_BIN],
        max_prepaint_tile_distance_);
    mts.gpu_memmgr_stats_bin = BinFromTilePriority(
        tile->combined_priority(),
        max_prepaint_tile_distance_);

    DidTileTreeBinChange(tile,
        bin_map[BinFromTilePriority(tile->priority(ACTIVE_TREE),
                                    max_prepaint_tile_distance_)],
        ACTIVE_TREE);
    DidTileTreeBinChange(tile,
        bin_map[BinFromTilePriority(tile->priority(PENDING_TREE),
                                    max_prepaint_tile_distance_)],
        PENDING_TREE);

    for (int i = 0; i < NUM_BIN_PRIORITIES; ++i)
      mts.bin[i] = bin_map[mts.bin[i]];

    if (!mts.drawing_info.resource_ &&
        !mts.drawing_info.resource_is_being_initialized_ &&
        !tile->priority(ACTIVE_TREE).is_live &&
        !tile->priority(PENDING_TREE).is_live)
      continue;

    live_or_allocated_tiles_.push_back(tile);
  }
  TRACE_COUNTER_ID1("cc", "LiveOrAllocatedTileCount", this,
                    live_or_allocated_tiles_.size());

  SortTiles();

  // Assign gpu memory and determine what tiles need to be rasterized.
  AssignGpuMemoryToTiles();

  TRACE_EVENT_INSTANT1("cc", "DidManage", "state",
                       ValueToString(BasicStateAsValue()));

  // Finally, kick the rasterizer.
  DispatchMoreTasks();
}

void TileManager::CheckForCompletedTileUploads() {
  while (!tiles_with_pending_upload_.empty()) {
    Tile* tile = tiles_with_pending_upload_.front();
    ManagedTileState& managed_tile_state = tile->managed_state();
    DCHECK(managed_tile_state.drawing_info.resource_);

    // Set pixel tasks complete in the order they are posted.
    if (!resource_pool_->resource_provider()->DidSetPixelsComplete(
          managed_tile_state.drawing_info.resource_->id())) {
      break;
    }

    // It's now safe to release the pixel buffer.
    resource_pool_->resource_provider()->ReleasePixelBuffer(
        managed_tile_state.drawing_info.resource_->id());

    managed_tile_state.drawing_info.can_be_freed_ = true;

    bytes_pending_upload_ -= tile->bytes_consumed_if_allocated();
    DidTileRasterStateChange(tile, IDLE_STATE);
    DidFinishTileInitialization(tile);

    tiles_with_pending_upload_.pop();
  }

  DispatchMoreTasks();
}

void TileManager::AbortPendingTileUploads() {
  while (!tiles_with_pending_upload_.empty()) {
    Tile* tile = tiles_with_pending_upload_.front();
    ManagedTileState& managed_tile_state = tile->managed_state();
    DCHECK(managed_tile_state.drawing_info.resource_);

    resource_pool_->resource_provider()->AbortSetPixels(
        managed_tile_state.drawing_info.resource_->id());
    resource_pool_->resource_provider()->ReleasePixelBuffer(
        managed_tile_state.drawing_info.resource_->id());

    managed_tile_state.drawing_info.resource_is_being_initialized_ = false;
    managed_tile_state.drawing_info.can_be_freed_ = true;
    managed_tile_state.can_use_gpu_memory = false;
    FreeResourcesForTile(tile);

    bytes_pending_upload_ -= tile->bytes_consumed_if_allocated();
    DidTileRasterStateChange(tile, IDLE_STATE);
    tiles_with_pending_upload_.pop();
  }
}

void TileManager::ForceTileUploadToComplete(Tile* tile) {
  DCHECK(tile);
  ManagedTileState& managed_tile_state = tile->managed_state();
  if (managed_tile_state.raster_state == UPLOAD_STATE) {
    Resource* resource = tile->drawing_info().resource_.get();
    DCHECK(resource);
    resource_pool_->resource_provider()->
        ForceSetPixelsToComplete(resource->id());
    DidTileRasterStateChange(tile, FORCED_UPLOAD_COMPLETION_STATE);
    DidFinishTileInitialization(tile);
  }
}

void TileManager::GetMemoryStats(
    size_t* memory_required_bytes,
    size_t* memory_nice_to_have_bytes,
    size_t* memory_used_bytes) const {
  *memory_required_bytes = 0;
  *memory_nice_to_have_bytes = 0;
  *memory_used_bytes = 0;
  for (size_t i = 0; i < live_or_allocated_tiles_.size(); i++) {
    const Tile* tile = live_or_allocated_tiles_[i];
    const ManagedTileState& mts = tile->managed_state();
    if (!tile->drawing_info().requires_resource())
      continue;

    size_t tile_bytes = tile->bytes_consumed_if_allocated();
    if (mts.gpu_memmgr_stats_bin == NOW_BIN)
      *memory_required_bytes += tile_bytes;
    if (mts.gpu_memmgr_stats_bin != NEVER_BIN)
      *memory_nice_to_have_bytes += tile_bytes;
    if (mts.can_use_gpu_memory)
      *memory_used_bytes += tile_bytes;
  }
}

scoped_ptr<base::Value> TileManager::BasicStateAsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());
  state->SetInteger("tile_count", all_tiles_.size());
  state->Set("global_state", global_state_.AsValue().release());
  state->Set("memory_requirements", GetMemoryRequirementsAsValue().release());
  return state.PassAs<base::Value>();
}

scoped_ptr<base::Value> TileManager::AllTilesAsValue() const {
  scoped_ptr<base::ListValue> state(new base::ListValue());
  for (TileSet::const_iterator it = all_tiles_.begin();
       it != all_tiles_.end();
       it++) {
    state->Append((*it)->AsValue().release());
  }
  return state.PassAs<base::Value>();
}

scoped_ptr<base::Value> TileManager::GetMemoryRequirementsAsValue() const {
  scoped_ptr<base::DictionaryValue> requirements(
      new base::DictionaryValue());

  size_t memory_required_bytes;
  size_t memory_nice_to_have_bytes;
  size_t memory_used_bytes;
  GetMemoryStats(&memory_required_bytes,
                 &memory_nice_to_have_bytes,
                 &memory_used_bytes);
  requirements->SetInteger("memory_required_bytes", memory_required_bytes);
  requirements->SetInteger("memory_nice_to_have_bytes",
                           memory_nice_to_have_bytes);
  requirements->SetInteger("memory_used_bytes", memory_used_bytes);
  return requirements.PassAs<base::Value>();
}

bool TileManager::HasPendingWorkScheduled(WhichTree tree) const {
  // Always true when ManageTiles() call is pending.
  if (manage_tiles_pending_)
    return true;

  for (int i = 0; i < NUM_STATES; ++i) {
    switch (i) {
      case WAITING_FOR_RASTER_STATE:
      case RASTER_STATE:
      case UPLOAD_STATE:
      case FORCED_UPLOAD_COMPLETION_STATE:
        for (int j = 0; j < NEVER_BIN; ++j) {
          if (raster_state_count_[i][tree][j])
            return true;
        }
        break;
      case IDLE_STATE:
        break;
      default:
        NOTREACHED();
    }
  }

  return false;
}

void TileManager::DidFinishDispatchingWorkerPoolCompletionCallbacks() {
  // If a flush is needed, do it now before starting to dispatch more tasks.
  if (has_performed_uploads_since_last_flush_) {
    resource_pool_->resource_provider()->ShallowFlushIfSupported();
    has_performed_uploads_since_last_flush_ = false;
  }

  DispatchMoreTasks();
}

void TileManager::AssignGpuMemoryToTiles() {
  TRACE_EVENT0("cc", "TileManager::AssignGpuMemoryToTiles");
  size_t unreleasable_bytes = 0;

  // Now give memory out to the tiles until we're out, and build
  // the needs-to-be-rasterized queue.
  tiles_that_need_to_be_rasterized_.clear();

  // Reset the image decoding list so that we don't mess up with tile
  // priorities. Tiles will be added to the image decoding list again
  // when DispatchMoreTasks() is called.
  tiles_with_image_decoding_tasks_.clear();

  // By clearing the tiles_that_need_to_be_rasterized_ vector and
  // tiles_with_image_decoding_tasks_ list above we move all tiles
  // currently waiting for raster to idle state.
  // Call DidTileRasterStateChange() for each of these tiles to
  // have this state change take effect.
  // Some memory cannot be released. We figure out how much in this
  // loop as well.
  for (TileVector::iterator it = live_or_allocated_tiles_.begin();
       it != live_or_allocated_tiles_.end(); ++it) {
    Tile* tile = *it;
    ManagedTileState& mts = tile->managed_state();
    if (!tile->drawing_info().requires_resource())
      continue;

    if (!mts.drawing_info.can_be_freed_)
      unreleasable_bytes += tile->bytes_consumed_if_allocated();
    if (mts.raster_state == WAITING_FOR_RASTER_STATE)
      DidTileRasterStateChange(tile, IDLE_STATE);
  }

  size_t bytes_allocatable =
      global_state_.memory_limit_in_bytes - unreleasable_bytes;
  size_t bytes_that_exceeded_memory_budget_in_now_bin = 0;
  size_t bytes_left = bytes_allocatable;
  for (TileVector::iterator it = live_or_allocated_tiles_.begin();
       it != live_or_allocated_tiles_.end();
       ++it) {
    Tile* tile = *it;
    ManagedTileState& mts = tile->managed_state();
    if (!tile->drawing_info().requires_resource())
      continue;

    size_t tile_bytes = tile->bytes_consumed_if_allocated();
    if (!mts.drawing_info.can_be_freed_)
      continue;
    if (mts.bin[HIGH_PRIORITY_BIN] == NEVER_BIN &&
        mts.bin[LOW_PRIORITY_BIN] == NEVER_BIN) {
      mts.can_use_gpu_memory = false;
      FreeResourcesForTile(tile);
      continue;
    }
    if (tile_bytes > bytes_left) {
      mts.can_use_gpu_memory = false;
      if (mts.bin[HIGH_PRIORITY_BIN] == NOW_BIN ||
          mts.bin[LOW_PRIORITY_BIN] == NOW_BIN)
          bytes_that_exceeded_memory_budget_in_now_bin += tile_bytes;
      FreeResourcesForTile(tile);
      continue;
    }
    bytes_left -= tile_bytes;
    mts.can_use_gpu_memory = true;
    if (!mts.drawing_info.resource_ &&
        !mts.drawing_info.resource_is_being_initialized_) {
      tiles_that_need_to_be_rasterized_.push_back(tile);
      DidTileRasterStateChange(tile, WAITING_FOR_RASTER_STATE);
    }
  }

  ever_exceeded_memory_budget_ |=
      bytes_that_exceeded_memory_budget_in_now_bin > 0;
  if (ever_exceeded_memory_budget_) {
      TRACE_COUNTER_ID2("cc", "over_memory_budget", this,
                        "budget", global_state_.memory_limit_in_bytes,
                        "over", bytes_that_exceeded_memory_budget_in_now_bin);
  }
  memory_stats_from_last_assign_.total_budget_in_bytes =
      global_state_.memory_limit_in_bytes;
  memory_stats_from_last_assign_.bytes_allocated =
      bytes_allocatable - bytes_left;
  memory_stats_from_last_assign_.bytes_unreleasable = unreleasable_bytes;
  memory_stats_from_last_assign_.bytes_over =
      bytes_that_exceeded_memory_budget_in_now_bin;

  // Reverse two tiles_that_need_* vectors such that pop_back gets
  // the highest priority tile.
  std::reverse(
      tiles_that_need_to_be_rasterized_.begin(),
      tiles_that_need_to_be_rasterized_.end());
}

void TileManager::FreeResourcesForTile(Tile* tile) {
  ManagedTileState& managed_tile_state = tile->managed_state();
  DCHECK(managed_tile_state.drawing_info.can_be_freed_);
  if (managed_tile_state.drawing_info.resource_)
    resource_pool_->ReleaseResource(
        managed_tile_state.drawing_info.resource_.Pass());
}

bool TileManager::CanDispatchRasterTask(Tile* tile) const {
  if (pending_tasks_ >= max_pending_tasks_)
    return false;
  size_t new_bytes_pending = bytes_pending_upload_;
  new_bytes_pending += tile->bytes_consumed_if_allocated();
  return new_bytes_pending <= kMaxPendingUploadBytes &&
         tiles_with_pending_upload_.size() < kMaxPendingUploads;
}

void TileManager::DispatchMoreTasks() {
  // Because tiles in the image decoding list have higher priorities, we
  // need to process those tiles first before we start to handle the tiles
  // in the need_to_be_rasterized queue. Note that solid/transparent tiles
  // will not be put into the decoding list.
  for(TileList::iterator it = tiles_with_image_decoding_tasks_.begin();
      it != tiles_with_image_decoding_tasks_.end(); ) {
    ManagedTileState& managed_tile_state = (*it)->managed_state();
    DispatchImageDecodeTasksForTile(*it);
    if (managed_tile_state.pending_pixel_refs.empty()) {
      if (!CanDispatchRasterTask(*it))
        return;
      DispatchOneRasterTask(*it);
      tiles_with_image_decoding_tasks_.erase(it++);
    } else {
      ++it;
    }
  }

  // Process all tiles in the need_to_be_rasterized queue. If a tile is
  // solid/transparent, then we are done (we don't need to rasterize
  // the tile). If a tile has image decoding tasks, put it to the back
  // of the image decoding list.
  while (!tiles_that_need_to_be_rasterized_.empty()) {
    Tile* tile = tiles_that_need_to_be_rasterized_.back();
    ManagedTileState& mts = tile->managed_state();

    AnalyzeTile(tile);
    if (!tile->drawing_info().requires_resource()) {
      DidTileRasterStateChange(tile, IDLE_STATE);
      tiles_that_need_to_be_rasterized_.pop_back();
      continue;
    }

    DispatchImageDecodeTasksForTile(tile);
    if (!mts.pending_pixel_refs.empty()) {
      tiles_with_image_decoding_tasks_.push_back(tile);
    } else {
      if (!CanDispatchRasterTask(tile))
        return;
      DispatchOneRasterTask(tile);
    }
    tiles_that_need_to_be_rasterized_.pop_back();
  }
}

void TileManager::AnalyzeTile(Tile* tile) {
  ManagedTileState& managed_tile_state = tile->managed_state();
  if ((use_cheapness_estimator_ || use_color_estimator_) &&
      !managed_tile_state.picture_pile_analyzed) {
    tile->picture_pile()->AnalyzeInRect(
        tile->content_rect(),
        tile->contents_scale(),
        &managed_tile_state.picture_pile_analysis);
    managed_tile_state.picture_pile_analysis.is_cheap_to_raster &=
        use_cheapness_estimator_;
    managed_tile_state.picture_pile_analysis.is_solid_color &=
        use_color_estimator_;
    managed_tile_state.picture_pile_analysis.is_transparent &=
        use_color_estimator_;
    managed_tile_state.picture_pile_analyzed = true;
    managed_tile_state.need_to_gather_pixel_refs = false;
    managed_tile_state.pending_pixel_refs.swap(
        managed_tile_state.picture_pile_analysis.lazy_pixel_refs);

    if (managed_tile_state.picture_pile_analysis.is_solid_color) {
      tile->drawing_info().set_solid_color(
        managed_tile_state.picture_pile_analysis.solid_color);
      DidFinishTileInitialization(tile);
    } else if (managed_tile_state.picture_pile_analysis.is_transparent) {
      tile->drawing_info().set_transparent();
      DidFinishTileInitialization(tile);
    }
  }
}

void TileManager::GatherPixelRefsForTile(Tile* tile) {
  TRACE_EVENT0("cc", "TileManager::GatherPixelRefsForTile");
  ManagedTileState& managed_tile_state = tile->managed_state();
  if (managed_tile_state.need_to_gather_pixel_refs) {
    base::TimeTicks start_time =
        rendering_stats_instrumentation_->StartRecording();
    tile->picture_pile()->GatherPixelRefs(
        tile->content_rect_,
        tile->contents_scale_,
        managed_tile_state.pending_pixel_refs);
    managed_tile_state.need_to_gather_pixel_refs = false;
    base::TimeDelta duration =
        rendering_stats_instrumentation_->EndRecording(start_time);
    rendering_stats_instrumentation_->AddImageGathering(duration);
  }
}

void TileManager::DispatchImageDecodeTasksForTile(Tile* tile) {
  GatherPixelRefsForTile(tile);
  std::list<skia::LazyPixelRef*>& pending_pixel_refs =
      tile->managed_state().pending_pixel_refs;
  std::list<skia::LazyPixelRef*>::iterator it = pending_pixel_refs.begin();
  while (it != pending_pixel_refs.end()) {
    if (pending_decode_tasks_.end() != pending_decode_tasks_.find(
        (*it)->getGenerationID())) {
      ++it;
      continue;
    }
    // TODO(qinmin): passing correct image size to PrepareToDecode().
    if ((*it)->PrepareToDecode(skia::LazyPixelRef::PrepareParams())) {
      rendering_stats_instrumentation_->IncrementDeferredImageCacheHitCount();
      pending_pixel_refs.erase(it++);
    } else {
      if (pending_tasks_ >= max_pending_tasks_)
        return;
      DispatchOneImageDecodeTask(tile, *it);
      ++it;
    }
  }
}

void TileManager::DispatchOneImageDecodeTask(
    scoped_refptr<Tile> tile, skia::LazyPixelRef* pixel_ref) {
  TRACE_EVENT0("cc", "TileManager::DispatchOneImageDecodeTask");
  uint32_t pixel_ref_id = pixel_ref->getGenerationID();
  DCHECK(pending_decode_tasks_.end() ==
      pending_decode_tasks_.find(pixel_ref_id));
  pending_decode_tasks_[pixel_ref_id] = pixel_ref;

  raster_worker_pool_->PostTaskAndReply(
      base::Bind(&TileManager::RunImageDecodeTask,
                 pixel_ref,
                 rendering_stats_instrumentation_),
      base::Bind(&TileManager::OnImageDecodeTaskCompleted,
                 base::Unretained(this),
                 tile,
                 pixel_ref_id));
  pending_tasks_++;
}

void TileManager::OnImageDecodeTaskCompleted(
    scoped_refptr<Tile> tile, uint32_t pixel_ref_id) {
  TRACE_EVENT0("cc", "TileManager::OnImageDecodeTaskCompleted");
  pending_decode_tasks_.erase(pixel_ref_id);
  pending_tasks_--;

  for (TileList::iterator it = tiles_with_image_decoding_tasks_.begin();
      it != tiles_with_image_decoding_tasks_.end(); ++it) {
    std::list<skia::LazyPixelRef*>& pixel_refs =
        (*it)->managed_state().pending_pixel_refs;
    for (std::list<skia::LazyPixelRef*>::iterator pixel_it =
        pixel_refs.begin(); pixel_it != pixel_refs.end(); ++pixel_it) {
      if (pixel_ref_id == (*pixel_it)->getGenerationID()) {
        pixel_refs.erase(pixel_it);
        break;
      }
    }
  }
}

scoped_ptr<ResourcePool::Resource> TileManager::PrepareTileForRaster(
    Tile* tile) {
  ManagedTileState& managed_tile_state = tile->managed_state();
  DCHECK(managed_tile_state.can_use_gpu_memory);
  scoped_ptr<ResourcePool::Resource> resource =
      resource_pool_->AcquireResource(tile->tile_size_.size(), tile->format_);
  resource_pool_->resource_provider()->AcquirePixelBuffer(resource->id());

  managed_tile_state.drawing_info.resource_is_being_initialized_ = true;
  managed_tile_state.drawing_info.can_be_freed_ = false;

  DidTileRasterStateChange(tile, RASTER_STATE);
  return resource.Pass();
}

void TileManager::SetAnticipatedDrawTime(base::TimeTicks time) {
  anticipated_draw_time_ = time;
  UpdateCheapTasksTimeLimit();
}

void TileManager::UpdateCheapTasksTimeLimit() {
  base::TimeTicks limit;
  if (use_cheapness_estimator_ &&
      global_state_.tree_priority != SMOOTHNESS_TAKES_PRIORITY) {
    limit = std::min(
        base::TimeTicks::Now() +
            base::TimeDelta::FromMilliseconds(kRunCheapTasksTimeMs),
        anticipated_draw_time_);
  }
  raster_worker_pool_->SetRunCheapTasksTimeLimit(limit);
}

void TileManager::DispatchOneRasterTask(scoped_refptr<Tile> tile) {
  TRACE_EVENT0("cc", "TileManager::DispatchOneRasterTask");
  scoped_ptr<ResourcePool::Resource> resource = PrepareTileForRaster(tile);
  ResourceProvider::ResourceId resource_id = resource->id();
  uint8* buffer =
      resource_pool_->resource_provider()->MapPixelBuffer(resource_id);

  ManagedTileState& managed_tile_state = tile->managed_state();
  raster_worker_pool_->PostRasterTaskAndReply(
      tile->picture_pile(),
      managed_tile_state.picture_pile_analysis.is_cheap_to_raster,
      base::Bind(&TileManager::RunRasterTask,
                 buffer,
                 tile->content_rect(),
                 tile->contents_scale(),
                 GetRasterTaskMetadata(*tile),
                 rendering_stats_instrumentation_),
      base::Bind(&TileManager::OnRasterTaskCompleted,
                 base::Unretained(this),
                 tile,
                 base::Passed(&resource),
                 manage_tiles_call_count_));
  pending_tasks_++;
}

TileManager::RasterTaskMetadata TileManager::GetRasterTaskMetadata(
    const Tile& tile) const {
  RasterTaskMetadata metadata;
  const ManagedTileState& mts = tile.managed_state();
  metadata.prediction_benchmarking = prediction_benchmarking_;
  metadata.is_tile_in_pending_tree_now_bin =
      mts.tree_bin[PENDING_TREE] == NOW_BIN;
  metadata.tile_resolution = mts.resolution;
  metadata.layer_id = tile.layer_id();
  return metadata;
}

void TileManager::OnRasterTaskCompleted(
    scoped_refptr<Tile> tile,
    scoped_ptr<ResourcePool::Resource> resource,
    int manage_tiles_call_count_when_dispatched) {
  TRACE_EVENT0("cc", "TileManager::OnRasterTaskCompleted");

  pending_tasks_--;

  // Release raster resources.
  resource_pool_->resource_provider()->UnmapPixelBuffer(resource->id());

  ManagedTileState& managed_tile_state = tile->managed_state();
  managed_tile_state.drawing_info.can_be_freed_ = true;

  // Tile can be freed after the completion of the raster task. Call
  // AssignGpuMemoryToTiles() to re-assign gpu memory to highest priority
  // tiles if ManageTiles() was called since task was dispatched. The result
  // of this could be that this tile is no longer allowed to use gpu
  // memory and in that case we need to abort initialization and free all
  // associated resources before calling DispatchMoreTasks().
  if (manage_tiles_call_count_when_dispatched != manage_tiles_call_count_)
    AssignGpuMemoryToTiles();

  // Finish resource initialization if |can_use_gpu_memory| is true.
  if (managed_tile_state.can_use_gpu_memory) {
    // The component order may be bgra if we're uploading bgra pixels to rgba
    // texture. Mark contents as swizzled if image component order is
    // different than texture format.
    managed_tile_state.drawing_info.contents_swizzled_ =
        !PlatformColor::SameComponentOrder(tile->format_);

    // Tile resources can't be freed until upload has completed.
    managed_tile_state.drawing_info.can_be_freed_ = false;

    resource_pool_->resource_provider()->BeginSetPixels(resource->id());
    has_performed_uploads_since_last_flush_ = true;

    managed_tile_state.drawing_info.resource_ = resource.Pass();

    bytes_pending_upload_ += tile->bytes_consumed_if_allocated();
    DidTileRasterStateChange(tile, UPLOAD_STATE);
    tiles_with_pending_upload_.push(tile);
  } else {
    resource_pool_->resource_provider()->ReleasePixelBuffer(resource->id());
    resource_pool_->ReleaseResource(resource.Pass());
    managed_tile_state.drawing_info.resource_is_being_initialized_ = false;
    DidTileRasterStateChange(tile, IDLE_STATE);
  }
}

void TileManager::DidFinishTileInitialization(Tile* tile) {
  ManagedTileState& managed_state = tile->managed_state();
  managed_state.drawing_info.resource_is_being_initialized_ = false;
  if (tile->priority(ACTIVE_TREE).distance_to_visible_in_pixels == 0)
    client_->DidInitializeVisibleTile();
}

void TileManager::DidTileRasterStateChange(Tile* tile, TileRasterState state) {
  ManagedTileState& mts = tile->managed_state();
  DCHECK_LT(state, NUM_STATES);

  for (int i = 0; i < NUM_TREES; ++i) {
    // Decrement count for current state.
    --raster_state_count_[mts.raster_state][i][mts.tree_bin[i]];
    DCHECK_GE(raster_state_count_[mts.raster_state][i][mts.tree_bin[i]], 0);

    // Increment count for new state.
    ++raster_state_count_[state][i][mts.tree_bin[i]];
  }

  mts.raster_state = state;
}

void TileManager::DidTileTreeBinChange(Tile* tile,
                                       TileManagerBin new_tree_bin,
                                       WhichTree tree) {
  ManagedTileState& mts = tile->managed_state();

  // Decrement count for current bin.
  --raster_state_count_[mts.raster_state][tree][mts.tree_bin[tree]];
  DCHECK_GE(raster_state_count_[mts.raster_state][tree][mts.tree_bin[tree]], 0);

  // Increment count for new bin.
  ++raster_state_count_[mts.raster_state][tree][new_tree_bin];

  mts.tree_bin[tree] = new_tree_bin;
}

// static
void TileManager::RunRasterTask(
    uint8* buffer,
    const gfx::Rect& rect,
    float contents_scale,
    const RasterTaskMetadata& metadata,
    RenderingStatsInstrumentation* stats_instrumentation,
    PicturePileImpl* picture_pile) {
  TRACE_EVENT2(
      "cc", "TileManager::RunRasterTask",
      "is_on_pending_tree",
      metadata.is_tile_in_pending_tree_now_bin,
      "is_low_res",
      metadata.tile_resolution == LOW_RESOLUTION);
  devtools_instrumentation::ScopedRasterTask raster_task(metadata.layer_id);

  DCHECK(picture_pile);
  DCHECK(buffer);

  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, rect.width(), rect.height());
  bitmap.setPixels(buffer);
  SkDevice device(bitmap);
  SkCanvas canvas(&device);

  base::TimeTicks start_time = stats_instrumentation->StartRecording();

  int64 total_pixels_rasterized = 0;
  picture_pile->Raster(&canvas, rect, contents_scale,
                       &total_pixels_rasterized);

  base::TimeDelta duration = stats_instrumentation->EndRecording(start_time);

  if (stats_instrumentation->record_rendering_stats()) {
    stats_instrumentation->AddRaster(duration,
                                     total_pixels_rasterized,
                                     metadata.is_tile_in_pending_tree_now_bin);

    UMA_HISTOGRAM_CUSTOM_COUNTS("Renderer4.PictureRasterTimeMS",
                                duration.InMilliseconds(),
                                0,
                                10,
                                10);

    if (metadata.prediction_benchmarking) {
      PicturePileImpl::Analysis analysis;
      picture_pile->AnalyzeInRect(rect, contents_scale, &analysis);
      bool is_predicted_cheap = analysis.is_cheap_to_raster;
      bool is_actually_cheap = duration.InMillisecondsF() <= 1.0f;
      RecordCheapnessPredictorResults(is_predicted_cheap, is_actually_cheap);

      DCHECK_EQ(bitmap.rowBytes(),
                bitmap.width() * bitmap.bytesPerPixel());

      RecordSolidColorPredictorResults(
          reinterpret_cast<SkColor*>(bitmap.getPixels()),
          bitmap.getSize() / bitmap.bytesPerPixel(),
          analysis.is_solid_color,
          analysis.solid_color,
          analysis.is_transparent);
    }
  }
}

// static
void TileManager::RecordCheapnessPredictorResults(bool is_predicted_cheap,
                                                  bool is_actually_cheap) {
  if (is_predicted_cheap && !is_actually_cheap)
    UMA_HISTOGRAM_BOOLEAN("Renderer4.CheapPredictorBadlyWrong", true);
  else if (!is_predicted_cheap && is_actually_cheap)
    UMA_HISTOGRAM_BOOLEAN("Renderer4.CheapPredictorSafelyWrong", true);

  UMA_HISTOGRAM_BOOLEAN("Renderer4.CheapPredictorAccuracy",
                        is_predicted_cheap == is_actually_cheap);
}

// static
void TileManager::RecordSolidColorPredictorResults(
    const SkColor* actual_colors,
    size_t color_count,
    bool is_predicted_solid,
    SkColor predicted_color,
    bool is_predicted_transparent) {
  DCHECK_GT(color_count, 0u);

  bool is_actually_solid = true;
  bool is_transparent = true;

  SkColor actual_color = *actual_colors;
  for (unsigned int i = 0; i < color_count; ++i) {
    SkColor current_color = actual_colors[i];
    if (current_color != actual_color ||
        SkColorGetA(current_color) != 255)
      is_actually_solid = false;

    if (SkColorGetA(current_color) != 0)
      is_transparent = false;

    if(!is_actually_solid && !is_transparent)
      break;
  }

  if (is_predicted_solid && !is_actually_solid)
    UMA_HISTOGRAM_BOOLEAN("Renderer4.ColorPredictor.WrongActualNotSolid", true);
  else if (is_predicted_solid &&
           is_actually_solid &&
           predicted_color != actual_color)
    UMA_HISTOGRAM_BOOLEAN("Renderer4.ColorPredictor.WrongColor", true);
  else if (!is_predicted_solid && is_actually_solid)
    UMA_HISTOGRAM_BOOLEAN("Renderer4.ColorPredictor.WrongActualSolid", true);

  bool correct_guess = (is_predicted_solid && is_actually_solid &&
                        predicted_color == actual_color) ||
                       (!is_predicted_solid && !is_actually_solid);
  UMA_HISTOGRAM_BOOLEAN("Renderer4.ColorPredictor.Accuracy", correct_guess);

  if (correct_guess)
    UMA_HISTOGRAM_BOOLEAN("Renderer4.ColorPredictor.IsCorrectSolid",
                          is_predicted_solid);

  if (is_predicted_transparent)
    UMA_HISTOGRAM_BOOLEAN(
        "Renderer4.ColorPredictor.PredictedTransparentIsActually",
        is_transparent);
  UMA_HISTOGRAM_BOOLEAN("Renderer4.ColorPredictor.IsActuallyTransparent",
                        is_transparent);
}

// static
void TileManager::RunImageDecodeTask(
    skia::LazyPixelRef* pixel_ref,
    RenderingStatsInstrumentation* stats_instrumentation) {
  TRACE_EVENT0("cc", "TileManager::RunImageDecodeTask");
  base::TimeTicks start_time = stats_instrumentation->StartRecording();
  pixel_ref->Decode();
  base::TimeDelta duration = stats_instrumentation->EndRecording(start_time);
  stats_instrumentation->AddDeferredImageDecode(duration);
}

}  // namespace cc
