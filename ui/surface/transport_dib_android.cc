// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/surface/transport_dib.h"

#include <sys/stat.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "base/shared_memory.h"
#include "skia/ext/platform_canvas.h"

TransportDIB::TransportDIB()
    : size_(0) {
}

TransportDIB::TransportDIB(TransportDIB::Handle dib)
    : shared_memory_(dib, false /* read write */),
      size_(0) {
}

TransportDIB::~TransportDIB() {
}

// static
TransportDIB* TransportDIB::Create(size_t size, uint32 sequence_num) {
  TransportDIB* dib = new TransportDIB;
  // We will use ashmem_get_size_region() to figure out the size in Map(size).
  if (!dib->shared_memory_.CreateAndMapAnonymous(size)) {
    delete dib;
    return NULL;
  }

  dib->size_ = size;
  return dib;
}

// static
TransportDIB* TransportDIB::Map(Handle handle) {
  scoped_ptr<TransportDIB> dib(CreateWithHandle(handle));
  if (!dib->Map())
    return NULL;
  return dib.release();
}

// static
TransportDIB* TransportDIB::CreateWithHandle(Handle handle) {
  return new TransportDIB(handle);
}

// static
bool TransportDIB::is_valid_handle(Handle dib) {
  return dib.fd >= 0;
}

// static
bool TransportDIB::is_valid_id(Id id) {
  // Same as is_valid_handle().
  return id.fd >= 0;
}

skia::PlatformCanvas* TransportDIB::GetPlatformCanvas(int w, int h) {
  if ((!memory() && !Map()) || !VerifyCanvasSize(w, h))
    return NULL;
  return skia::CreatePlatformCanvas(w, h, true,
                                    reinterpret_cast<uint8_t*>(memory()),
                                    skia::RETURN_NULL_ON_FAILURE);
}

bool TransportDIB::Map() {
  if (!is_valid_handle(handle()) || !shared_memory_.Map(0))
    return false;

  size_ = shared_memory_.mapped_size();
  return true;
}

void* TransportDIB::memory() const {
  return shared_memory_.memory();
}

TransportDIB::Id TransportDIB::id() const {
  // Use FileDescriptor as id.
  return shared_memory_.handle();
}

TransportDIB::Handle TransportDIB::handle() const {
  return shared_memory_.handle();
}
