// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/raster_decoder_context_state.h"

#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/create_gr_gl_interface.h"

namespace gpu {
namespace raster {

RasterDecoderContextState::RasterDecoderContextState(
    scoped_refptr<gl::GLShareGroup> share_group,
    scoped_refptr<gl::GLSurface> surface,
    scoped_refptr<gl::GLContext> context,
    bool use_virtualized_gl_contexts)
    : share_group(std::move(share_group)),
      surface(std::move(surface)),
      context(std::move(context)),
      use_virtualized_gl_contexts(use_virtualized_gl_contexts) {
  if (base::ThreadTaskRunnerHandle::IsSet()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "RasterDecoderContextState", base::ThreadTaskRunnerHandle::Get());
  }
}

RasterDecoderContextState::~RasterDecoderContextState() {
  if (gr_context)
    gr_context->abandonContext();
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

void RasterDecoderContextState::InitializeGrContext(
    const GpuDriverBugWorkarounds& workarounds) {
  DCHECK(context->IsCurrent(surface.get()));

  sk_sp<const GrGLInterface> interface(
      gl::init::CreateGrGLInterface(*context->GetVersionInfo()));
  if (!interface) {
    LOG(ERROR) << "OOP raster support disabled: GrGLInterface creation "
                  "failed.";
    return;
  }

  // If you make any changes to the GrContext::Options here that could
  // affect text rendering, make sure to match the capabilities initialized
  // in GetCapabilities and ensuring these are also used by the
  // PaintOpBufferSerializer.
  GrContextOptions options;
  options.fDriverBugWorkarounds =
      GrDriverBugWorkarounds(workarounds.ToIntSet());
  size_t max_resource_cache_bytes = 0u;
  raster::DetermineGrCacheLimitsFromAvailableMemory(
      &max_resource_cache_bytes, &glyph_cache_max_texture_bytes);
  options.fGlyphCacheTextureMaximumBytes = glyph_cache_max_texture_bytes;
  gr_context = GrContext::MakeGL(std::move(interface), options);
  if (!gr_context) {
    LOG(ERROR) << "OOP raster support disabled: GrContext creation "
                  "failed.";
  } else {
    constexpr int kMaxGaneshResourceCacheCount = 16384;
    gr_context->setResourceCacheLimits(kMaxGaneshResourceCacheCount,
                                       max_resource_cache_bytes);
  }
}

bool RasterDecoderContextState::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  if (gr_context)
    DumpGrMemoryStatistics(gr_context.get(), pmd, base::nullopt);
  return true;
}

void RasterDecoderContextState::PurgeGrCache() {
  if (!gr_context)
    return;

  context->MakeCurrent(surface.get());
  gr_context->freeGpuResources();
}

}  // namespace raster
}  // namespace gpu
