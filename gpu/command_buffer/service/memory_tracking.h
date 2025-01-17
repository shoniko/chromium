// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_MEMORY_TRACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_MEMORY_TRACKING_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/trace_event/trace_event.h"

namespace gpu {
namespace gles2 {

// A MemoryTracker is used to propagate per-ContextGroup memory usage
// statistics to the global GpuMemoryManager.
class MemoryTracker : public base::RefCounted<MemoryTracker> {
 public:
  virtual void TrackMemoryAllocatedChange(uint64_t delta) = 0;
  virtual uint64_t GetSize() const = 0;

  // Tracing id which identifies the GPU client for whom memory is being
  // allocated.
  virtual uint64_t ClientTracingId() const = 0;

  // Identifies the share group within which memory is being allocated.
  virtual uint64_t ShareGroupTracingGUID() const = 0;

  // Raw ID identifying the GPU client for whom memory is being allocated.
  virtual int ClientId() const = 0;

 protected:
  friend class base::RefCounted<MemoryTracker>;
  MemoryTracker() = default;
  virtual ~MemoryTracker() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(MemoryTracker);
};

// A MemoryTypeTracker tracks the use of a particular type of memory (buffer,
// texture, or renderbuffer) and forward the result to a specified
// MemoryTracker.
class MemoryTypeTracker {
 public:
  explicit MemoryTypeTracker(MemoryTracker* memory_tracker)
      : memory_tracker_(memory_tracker) {}

  ~MemoryTypeTracker() = default;

  void TrackMemAlloc(size_t bytes) {
    mem_represented_ += bytes;
    if (memory_tracker_ && bytes)
      memory_tracker_->TrackMemoryAllocatedChange(bytes);
  }

  void TrackMemFree(size_t bytes) {
    DCHECK(bytes <= mem_represented_);
    mem_represented_ -= bytes;
    if (memory_tracker_ && bytes) {
      memory_tracker_->TrackMemoryAllocatedChange(
          -static_cast<uint64_t>(bytes));
    }
  }

  size_t GetMemRepresented() const { return mem_represented_; }

 private:
  MemoryTracker* memory_tracker_;
  size_t mem_represented_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MemoryTypeTracker);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_MEMORY_TRACKING_H_
