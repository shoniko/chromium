// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.adblock;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.Set;

/**
 * AdblockBridge is a singleton which provides access to some native adblocking methods.
 */
public final class AdblockBridge {

    private AdblockBridge() {
    }

    private static AdblockBridge sInstance;
    
    /**
     * @return The singleton object.
     */
    public static AdblockBridge getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) sInstance = new AdblockBridge();
        return sInstance;
    }

    /**
     * @return Whether the singleton have been initialized.
     */
    public static boolean isInitialized() {
        return sInstance != null;
    }

    public void setFilterEnginePointer(long ptr) {
        nativeSetFilterEnginePointer(ptr);
    }

    public long getIsolatePointer() {
        return nativeGetIsolatePointer();
    }

    private native void nativeSetFilterEnginePointer(long ptr);
    private native long nativeGetIsolatePointer();
}
