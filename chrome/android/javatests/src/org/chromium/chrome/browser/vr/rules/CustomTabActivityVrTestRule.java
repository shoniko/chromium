// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.rules;

import android.support.test.InstrumentationRegistry;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.vr.TestVrShellDelegate;
import org.chromium.chrome.browser.vr.rules.VrActivityRestriction.SupportedActivity;
import org.chromium.chrome.browser.vr.util.HeadTrackingUtils;
import org.chromium.chrome.browser.vr.util.VrTestRuleUtils;

/**
 * VR extension of CustomTabActivityTestRule. Applies CustomTabActivityTestRule then
 * opens up a CustomTabActivity to a blank page.
 */
public class CustomTabActivityVrTestRule extends CustomTabActivityTestRule implements VrTestRule {
    private boolean mTrackerDirty;

    @Override
    public Statement apply(final Statement base, final Description desc) {
        return super.apply(new Statement() {
            @Override
            public void evaluate() throws Throwable {
                VrTestRuleUtils.ensureNoVrActivitiesDisplayed();
                HeadTrackingUtils.checkForAndApplyHeadTrackingModeAnnotation(
                        CustomTabActivityVrTestRule.this, desc);
                startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), "about:blank"));
                TestVrShellDelegate.createTestVrShellDelegate(getActivity());
                try {
                    base.evaluate();
                } finally {
                    if (isTrackerDirty()) HeadTrackingUtils.revertTracker();
                }
            }
        }, desc);
    }

    @Override
    public SupportedActivity getRestriction() {
        return SupportedActivity.CCT;
    }

    @Override
    public boolean isTrackerDirty() {
        return mTrackerDirty;
    }

    @Override
    public void setTrackerDirty() {
        mTrackerDirty = true;
    }
}
