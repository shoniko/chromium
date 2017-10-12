// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/adblock/adblock_bridge.h"

#include <jni.h>
#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "third_party/WebKit/public/web/WebKit.h"

#include "jni/AdblockBridge_jni.h"

#include "gin/public/isolate_holder.h"
#include "gin/v8_initializer.h"
#include "base/threading/thread.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ScopedJavaGlobalRef;

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jlong AdblockBridge::filterEnginePtr = 0;
base::Thread* thread = nullptr;
gin::IsolateHolder* isolate_holder = nullptr;

static void SetFilterEnginePointer(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jlong ptr)
{  
  LOG(WARNING) << "Adblock: set FilterEngine instance " << ptr;
  AdblockBridge::filterEnginePtr = ptr;

  if (!ptr)
  {
    // v8 deinit
    LOG(WARNING) << "Adblock: destroying isolate holder ...";
    delete isolate_holder;
    isolate_holder = nullptr;
    LOG(WARNING) << "Adblock: destroyed isolate holder";

    // thread
    thread->Stop();
    delete thread;
    thread = nullptr;
  }
}

static jlong GetIsolatePointer(
	JNIEnv* env,
	const base::android::JavaParamRef<jobject>& jcaller)
{
  if (!isolate_holder)
  {
    // thread for V8 tasks
    thread = new base::Thread("AdblockThread");
    thread->Start();

    // v8 init
    LOG(WARNING) << "Adblock: creating isolate holder ...";
    
    #ifdef V8_USE_EXTERNAL_STARTUP_DATA
    LOG(WARNING) << "Adblock: loading v8 snapshot & natives ...";
    gin::V8Initializer::LoadV8Snapshot();
    gin::V8Initializer::LoadV8Natives();
    LOG(WARNING) << "Adblock: loaded v8 snapshot & natives";
    #endif

    LOG(WARNING) << "Adblock: initialize isolate holder";
    gin::IsolateHolder::Initialize(gin::IsolateHolder::kStrictMode,
                                   gin::IsolateHolder::kStableV8Extras,
                                   gin::ArrayBufferAllocator::SharedInstance());

    // create isolate using isolate holder (using kUseLocker!)
    thread->WaitUntilThreadStarted();
    scoped_refptr<base::SingleThreadTaskRunner> task_runner = thread->task_runner();
    isolate_holder = new gin::IsolateHolder(task_runner, gin::IsolateHolder::AccessMode::kUseLocker);
  }

  v8::Isolate* isolate = isolate_holder->isolate();

  // return isolate pointer
  LOG(WARNING) << "Adblock: returning (from adblock_bridge.cc) isolate pointer " << isolate;
  return (jlong)isolate;
}
