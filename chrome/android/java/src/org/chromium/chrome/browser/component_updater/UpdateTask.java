// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.component_updater;

import android.content.Context;

import org.chromium.chrome.browser.background_task_scheduler.NativeBackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTask.TaskFinishedCallback;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskParameters;

/** Task for initiating a component update. */
public class UpdateTask extends NativeBackgroundTask {
    @Override
    @StartBeforeNativeResult
    protected int onStartTaskBeforeNativeLoaded(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        assert taskParameters.getTaskId() == TaskIds.COMPONENT_UPDATE_JOB_ID;
        UpdateScheduler.getInstance().onStartTaskBeforeNativeLoaded(callback);
        return LOAD_NATIVE;
    }

    @Override
    protected void onStartTaskWithNative(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        assert taskParameters.getTaskId() == TaskIds.COMPONENT_UPDATE_JOB_ID;
        UpdateScheduler.getInstance().onStartTaskWithNative();
    }

    @Override
    protected boolean onStopTaskBeforeNativeLoaded(Context context, TaskParameters taskParameters) {
        assert taskParameters.getTaskId() == TaskIds.COMPONENT_UPDATE_JOB_ID;
        UpdateScheduler.getInstance().onStopTask();

        // Don't reschedule task here. We are rescheduling with our parameters.
        return false;
    }

    @Override
    protected boolean onStopTaskWithNative(Context context, TaskParameters taskParameters) {
        assert taskParameters.getTaskId() == TaskIds.COMPONENT_UPDATE_JOB_ID;
        UpdateScheduler.getInstance().onStopTask();

        // Don't reschedule task here. We are rescheduling with our parameters.
        return false;
    }

    @Override
    public void reschedule(Context context) {
        UpdateScheduler.getInstance().reschedule();
    }
}