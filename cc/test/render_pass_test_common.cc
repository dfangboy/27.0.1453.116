// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/render_pass_test_common.h"

#include "cc/quads/checkerboard_draw_quad.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/io_surface_draw_quad.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/quads/yuv_video_draw_quad.h"
#include "cc/resources/resource_provider.h"
#include "ui/gfx/transform.h"

namespace cc {

void TestRenderPass::AppendQuad(scoped_ptr<cc::DrawQuad> quad) {
  quad_list.push_back(quad.Pass());
}

void TestRenderPass::AppendSharedQuadState(
    scoped_ptr<cc::SharedQuadState> state) {
  shared_quad_state_list.push_back(state.Pass());
}

void TestRenderPass::AppendOneOfEveryQuadType(
    cc::ResourceProvider* resource_provider, RenderPass::Id child_pass) {
  gfx::Rect rect(0, 0, 100, 100);
  gfx::Rect opaque_rect(10, 10, 80, 80);
  const float vertex_opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
  cc::ResourceProvider::ResourceId texture_resource =
      resource_provider->CreateResource(
          gfx::Size(20, 12),
          resource_provider->best_texture_format(),
          ResourceProvider::TextureUsageAny);
  resource_provider->AllocateForTesting(texture_resource);
  unsigned texture_id = ResourceProvider::ScopedReadLockGL(
      resource_provider, texture_resource).texture_id();
  cc::ResourceProvider::ResourceId resource1 =
      resource_provider->CreateResource(
          gfx::Size(45, 5),
          resource_provider->best_texture_format(),
          ResourceProvider::TextureUsageAny);
  resource_provider->AllocateForTesting(resource1);
  cc::ResourceProvider::ResourceId resource2 =
      resource_provider->CreateResource(
          gfx::Size(346, 61),
          resource_provider->best_texture_format(),
          ResourceProvider::TextureUsageAny);
  resource_provider->AllocateForTesting(resource2);
  cc::ResourceProvider::ResourceId resource3 =
      resource_provider->CreateResource(
          gfx::Size(12, 134),
          resource_provider->best_texture_format(),
          ResourceProvider::TextureUsageAny);
  resource_provider->AllocateForTesting(resource3);
  cc::ResourceProvider::ResourceId resource4 =
      resource_provider->CreateResource(
          gfx::Size(56, 12),
          resource_provider->best_texture_format(),
          ResourceProvider::TextureUsageAny);
  resource_provider->AllocateForTesting(resource4);
  cc::ResourceProvider::ResourceId resource5 =
      resource_provider->CreateResource(
          gfx::Size(73, 26),
          resource_provider->best_texture_format(),
          ResourceProvider::TextureUsageAny);
  resource_provider->AllocateForTesting(resource5);

  scoped_ptr<cc::SharedQuadState> shared_state = cc::SharedQuadState::Create();
  shared_state->SetAll(gfx::Transform(),
                       rect.size(),
                       rect,
                       rect,
                       false,
                       1);

  scoped_ptr<cc::CheckerboardDrawQuad> checkerboard_quad =
      cc::CheckerboardDrawQuad::Create();
  checkerboard_quad->SetNew(shared_state.get(),
                            rect,
                            SK_ColorRED);
  AppendQuad(checkerboard_quad.PassAs<DrawQuad>());

  scoped_ptr<cc::DebugBorderDrawQuad> debug_border_quad =
      cc::DebugBorderDrawQuad::Create();
  debug_border_quad->SetNew(shared_state.get(),
                            rect,
                            SK_ColorRED,
                            1);
  AppendQuad(debug_border_quad.PassAs<DrawQuad>());

  scoped_ptr<cc::IOSurfaceDrawQuad> io_surface_quad =
      cc::IOSurfaceDrawQuad::Create();
  io_surface_quad->SetNew(shared_state.get(),
                          rect,
                          opaque_rect,
                          gfx::Size(50, 50),
                          texture_id,
                          cc::IOSurfaceDrawQuad::FLIPPED);
  AppendQuad(io_surface_quad.PassAs<DrawQuad>());

  if (child_pass.layer_id) {
    scoped_ptr<cc::RenderPassDrawQuad> render_pass_quad =
        cc::RenderPassDrawQuad::Create();
    render_pass_quad->SetNew(shared_state.get(),
                             rect,
                             child_pass,
                             false,
                             resource5,
                             rect,
                             gfx::RectF(),
                             WebKit::WebFilterOperations(),
                             skia::RefPtr<SkImageFilter>(),
                             WebKit::WebFilterOperations());
    AppendQuad(render_pass_quad.PassAs<DrawQuad>());

    scoped_ptr<cc::RenderPassDrawQuad> render_pass_replica_quad =
        cc::RenderPassDrawQuad::Create();
    render_pass_replica_quad->SetNew(shared_state.get(),
                                     rect,
                                     child_pass,
                                     true,
                                     resource5,
                                     rect,
                                     gfx::RectF(),
                                     WebKit::WebFilterOperations(),
                                     skia::RefPtr<SkImageFilter>(),
                                     WebKit::WebFilterOperations());
    AppendQuad(render_pass_replica_quad.PassAs<DrawQuad>());
  }

  scoped_ptr<cc::SolidColorDrawQuad> solid_color_quad =
      cc::SolidColorDrawQuad::Create();
  solid_color_quad->SetNew(shared_state.get(),
                           rect,
                           SK_ColorRED);
  AppendQuad(solid_color_quad.PassAs<DrawQuad>());

  scoped_ptr<cc::StreamVideoDrawQuad> stream_video_quad =
      cc::StreamVideoDrawQuad::Create();
  stream_video_quad->SetNew(shared_state.get(),
                            rect,
                            opaque_rect,
                            texture_id,
                            gfx::Transform());
  AppendQuad(stream_video_quad.PassAs<DrawQuad>());

  scoped_ptr<cc::TextureDrawQuad> texture_quad =
      cc::TextureDrawQuad::Create();
  texture_quad->SetNew(shared_state.get(),
                       rect,
                       opaque_rect,
                       resource1,
                       false,
                       gfx::PointF(0.f, 0.f),
                       gfx::PointF(1.f, 1.f),
                       vertex_opacity,
                       false);
  AppendQuad(texture_quad.PassAs<DrawQuad>());

  scoped_ptr<cc::TileDrawQuad> scaled_tile_quad =
      cc::TileDrawQuad::Create();
  scaled_tile_quad->SetNew(shared_state.get(),
                           rect,
                           opaque_rect,
                           resource2,
                           gfx::RectF(0, 0, 50, 50),
                           gfx::Size(50, 50),
                           false);
  AppendQuad(scaled_tile_quad.PassAs<DrawQuad>());

  scoped_ptr<cc::SharedQuadState> transformed_state = shared_state->Copy();
  gfx::Transform rotation;
  rotation.Rotate(45);
  transformed_state->content_to_target_transform =
      transformed_state->content_to_target_transform * rotation;
  scoped_ptr<cc::TileDrawQuad> transformed_tile_quad =
      cc::TileDrawQuad::Create();
  transformed_tile_quad->SetNew(transformed_state.get(),
                                rect,
                                opaque_rect,
                                resource3,
                                gfx::RectF(0, 0, 100, 100),
                                gfx::Size(100, 100),
                                false);
  AppendQuad(transformed_tile_quad.PassAs<DrawQuad>());

  scoped_ptr<cc::TileDrawQuad> tile_quad =
      cc::TileDrawQuad::Create();
  tile_quad->SetNew(shared_state.get(),
                    rect,
                    opaque_rect,
                    resource4,
                    gfx::RectF(0, 0, 100, 100),
                    gfx::Size(100, 100),
                    false);
  AppendQuad(tile_quad.PassAs<DrawQuad>());

  cc::VideoLayerImpl::FramePlane planes[3];
  for (int i = 0; i < 3; ++i) {
    planes[i].resource_id =
        resource_provider->CreateResource(
            gfx::Size(20, 12),
            resource_provider->best_texture_format(),
            ResourceProvider::TextureUsageAny);
    resource_provider->AllocateForTesting(planes[i].resource_id);
    planes[i].size = gfx::Size(100, 100);
    planes[i].format = GL_LUMINANCE;
  }
  scoped_ptr<cc::YUVVideoDrawQuad> yuv_quad =
      cc::YUVVideoDrawQuad::Create();
  yuv_quad->SetNew(shared_state.get(),
                   rect,
                   opaque_rect,
                   gfx::Size(100, 100),
                   planes[0],
                   planes[1],
                   planes[2]);
  AppendQuad(yuv_quad.PassAs<DrawQuad>());

  AppendSharedQuadState(transformed_state.Pass());
  AppendSharedQuadState(shared_state.Pass());
}

}  // namespace cc
