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
#include "components/prefs/pref_member.h"
#include "base/lazy_instance.h"

class PrefService;

class AdblockBridge {
 public:
   static bool prefs_moved_to_thread;
   static BooleanPrefMember* enable_adblock;
   static StringListPrefMember* adblock_whitelisted_domains;
   
   static void InitializePrefsOnUIThread(PrefService* pref_service);
   static void ReleasePrefs();

   static jlong getFilterEnginePtr(); // thread-safe
   static void setFilterEnginePtr(jlong ptr); // thread-safe

  private:
    static base::LazyInstance<base::Lock>::DestructorAtExit filterEnginePtrLock;
    static jlong filterEnginePtr;
};

#endif  // CHROME_BROWSER_ANDROID_ADBLOCK_BRIDGE_H_
