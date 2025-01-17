// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_UTIL_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_UTIL_H_

#include "base/optional.h"

namespace content {

template <typename OptionalT>
base::Optional<typename OptionalT::value_type> ToBaseOptional(
    const OptionalT& optional) {
  return optional ? base::make_optional(*optional) : base::nullopt;
}

template <typename OptionalT1, typename OptionalT2>
bool OptionalEquals(const OptionalT1& lhs, const OptionalT2& rhs) {
  if (!lhs)
    return !rhs;
  if (!rhs)
    return false;
  return *lhs == *rhs;
}

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_UTIL_H_
