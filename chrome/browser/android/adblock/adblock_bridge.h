// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_ADBLOCK_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_ADBLOCK_BRIDGE_H_

#include <jni.h>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"


class AdblockBridge {
 public:
   static bool RegisterJNI(JNIEnv* env);
   static jlong filterEnginePtr; 
};

#endif  // CHROME_BROWSER_ANDROID_ADBLOCK_BRIDGE_H_
