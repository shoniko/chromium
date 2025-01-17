// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/recyclable_compositor_mac.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/common/features.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/compositor_switches.h"

namespace ui {

namespace {

// Returns a task runner for creating a ui::Compositor. This allows compositor
// tasks to be funneled through ui::WindowResizeHelper's task runner to allow
// resize operations to coordinate with frames provided by the GPU process.
scoped_refptr<base::SingleThreadTaskRunner> GetCompositorTaskRunner() {
  // If the WindowResizeHelper's pumpable task runner is set, it means the GPU
  // process is directing messages there, and the compositor can synchronize
  // with it. Otherwise, just use the UI thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      ui::WindowResizeHelperMac::Get()->task_runner();
  return task_runner ? task_runner : base::ThreadTaskRunnerHandle::Get();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// RecyclableCompositorMac

RecyclableCompositorMac::RecyclableCompositorMac(
    ui::ContextFactory* context_factory,
    ui::ContextFactoryPrivate* context_factory_private)
    : accelerated_widget_mac_(new ui::AcceleratedWidgetMac()),
      compositor_(context_factory_private->AllocateFrameSinkId(),
                  context_factory,
                  context_factory_private,
                  GetCompositorTaskRunner(),
                  features::IsSurfaceSynchronizationEnabled(),
                  ui::IsPixelCanvasRecordingEnabled()) {
  compositor_.SetAcceleratedWidget(
      accelerated_widget_mac_->accelerated_widget());
  Suspend();
  compositor_.AddObserver(this);
}

RecyclableCompositorMac::~RecyclableCompositorMac() {
  compositor_.RemoveObserver(this);
}

void RecyclableCompositorMac::Suspend() {
  // Requests a compositor lock without a timeout.
  compositor_suspended_lock_ =
      compositor_.GetCompositorLock(nullptr, base::TimeDelta());
}

void RecyclableCompositorMac::Unsuspend() {
  compositor_suspended_lock_ = nullptr;
}

void RecyclableCompositorMac::UpdateSurface(const gfx::Size& size_pixels,
                                            float scale_factor) {
  if (size_pixels != size_pixels_ || scale_factor != scale_factor_) {
    size_pixels_ = size_pixels;
    scale_factor_ = scale_factor;
    compositor()->SetScaleAndSize(scale_factor_, size_pixels_,
                                  local_surface_id_allocator_.GenerateId());
  }
}

void RecyclableCompositorMac::InvalidateSurface() {
  size_pixels_ = gfx::Size();
  scale_factor_ = 1.f;
  local_surface_id_allocator_.Invalidate();
  compositor()->SetScaleAndSize(
      scale_factor_, size_pixels_,
      local_surface_id_allocator_.GetCurrentLocalSurfaceId());
}

void RecyclableCompositorMac::OnCompositingDidCommit(
    ui::Compositor* compositor_that_did_commit) {
  DCHECK_EQ(compositor_that_did_commit, compositor());
  accelerated_widget_mac_->SetSuspended(false);
}

////////////////////////////////////////////////////////////////////////////////
// RecyclableCompositorMacFactory

// static
RecyclableCompositorMacFactory* RecyclableCompositorMacFactory::Get() {
  static base::NoDestructor<RecyclableCompositorMacFactory> factory;
  return factory.get();
}

std::unique_ptr<RecyclableCompositorMac>
RecyclableCompositorMacFactory::CreateCompositor(
    ui::ContextFactory* context_factory,
    ui::ContextFactoryPrivate* context_factory_private) {
  active_compositor_count_ += 1;
  if (!compositors_.empty()) {
    std::unique_ptr<RecyclableCompositorMac> result;
    result = std::move(compositors_.back());
    compositors_.pop_back();
    return result;
  }
  return std::make_unique<RecyclableCompositorMac>(context_factory,
                                                   context_factory_private);
}

void RecyclableCompositorMacFactory::RecycleCompositor(
    std::unique_ptr<RecyclableCompositorMac> compositor) {
  // When we get to zero compositors in use, destroy all spare compositors.
  // This is done to appease tests that rely on compositors being destroyed
  // immediately (if the compositor is recycled and continues to exist, its
  // subsequent initialization will crash).
  active_compositor_count_ -= 1;
  if (!active_compositor_count_) {
    compositors_.clear();
    return;
  }

  if (recycling_disabled_)
    return;

  compositor->accelerated_widget_mac_->SetSuspended(true);

  // Make this RecyclableCompositorMac recyclable for future instances.
  compositors_.push_back(std::move(compositor));

  // Post a task to free up the spare ui::Compositors when needed. Post this
  // to the browser main thread so that we won't free any compositors while
  // in a nested loop waiting to put up a new frame.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&RecyclableCompositorMacFactory::ReduceSpareCompositors,
                     weak_factory_.GetWeakPtr()));
}

RecyclableCompositorMacFactory::RecyclableCompositorMacFactory()
    : weak_factory_(this) {}

RecyclableCompositorMacFactory::~RecyclableCompositorMacFactory() = default;

void RecyclableCompositorMacFactory::ReduceSpareCompositors() {
  // Allow at most one spare recyclable compositor.
  while (compositors_.size() > 1)
    compositors_.pop_front();
}

void RecyclableCompositorMacFactory::DisableRecyclingForShutdown() {
  recycling_disabled_ = true;
  compositors_.clear();
}

}  // namespace ui
