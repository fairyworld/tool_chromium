// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link ActorMetrics}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowSystemClock.class})
public class ActorMetricsTest {
    private ActorMetrics mActorMetrics;

    @Before
    public void setUp() {
        ActorMetrics.resetForTesting();
        mActorMetrics = ActorMetrics.getInstance();
    }

    @After
    public void tearDown() {
        ActorMetrics.resetForTesting();
    }

    @Test
    public void testTaskStateTransitionsInForeground() {
        int taskId = 123;
        String histogramForegroundActing = "Actor.Tools.ExecutionDuration.Foreground.Acting";
        String histogramForegroundReflecting =
                "Actor.Tools.ExecutionDuration.Foreground.Reflecting";

        mActorMetrics.onTaskStateChangedForTesting(taskId, ActorTaskState.ACTING);
        ShadowSystemClock.advanceBy(500, TimeUnit.MILLISECONDS);

        mActorMetrics.onTaskStateChangedForTesting(taskId, ActorTaskState.REFLECTING);
        ShadowSystemClock.advanceBy(300, TimeUnit.MILLISECONDS);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(histogramForegroundActing, 500)
                        .expectIntRecord(histogramForegroundReflecting, 300)
                        .build();

        mActorMetrics.onTaskStateChangedForTesting(taskId, ActorTaskState.FINISHED);
        watcher.assertExpected();
    }

    @Test
    public void testPiPTransitions() {
        int taskId = 456;
        String histogramForegroundActing = "Actor.Tools.ExecutionDuration.Foreground.Acting";
        String histogramPipActing = "Actor.Tools.ExecutionDuration.Pip.Acting";

        // Start in Foreground
        mActorMetrics.onTaskStateChangedForTesting(taskId, ActorTaskState.ACTING);
        ShadowSystemClock.advanceBy(400, TimeUnit.MILLISECONDS);

        // Transition to PiP while still ACTING
        mActorMetrics.setIsInPip(true);
        ShadowSystemClock.advanceBy(600, TimeUnit.MILLISECONDS);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(histogramForegroundActing, 400)
                        .expectIntRecord(histogramPipActing, 600)
                        .build();

        mActorMetrics.onTaskStateChangedForTesting(taskId, ActorTaskState.FINISHED);
        watcher.assertExpected();
    }

    @Test
    public void testMultipleTasks() {
        int taskId1 = 1;
        int taskId2 = 2;
        String histogramForegroundActing = "Actor.Tools.ExecutionDuration.Foreground.Acting";

        mActorMetrics.onTaskStateChangedForTesting(taskId1, ActorTaskState.ACTING);
        ShadowSystemClock.advanceBy(100, TimeUnit.MILLISECONDS);

        mActorMetrics.onTaskStateChangedForTesting(taskId2, ActorTaskState.ACTING);
        ShadowSystemClock.advanceBy(200, TimeUnit.MILLISECONDS);

        // taskId1: 100ms (alone) + 200ms (while taskId2 is also ACTING) = 300ms
        // taskId2: 200ms

        var watcher1 = HistogramWatcher.newSingleRecordWatcher(histogramForegroundActing, 300);
        mActorMetrics.onTaskStateChangedForTesting(taskId1, ActorTaskState.FINISHED);
        watcher1.assertExpected();

        var watcher2 = HistogramWatcher.newSingleRecordWatcher(histogramForegroundActing, 200);
        mActorMetrics.onTaskStateChangedForTesting(taskId2, ActorTaskState.FINISHED);
        watcher2.assertExpected();
    }

    @Test
    public void testAccumulateMultipleTimesSameState() {
        int taskId = 789;
        String histogramForegroundActing = "Actor.Tools.ExecutionDuration.Foreground.Acting";

        mActorMetrics.onTaskStateChangedForTesting(taskId, ActorTaskState.ACTING);
        ShadowSystemClock.advanceBy(100, TimeUnit.MILLISECONDS);

        mActorMetrics.onTaskStateChangedForTesting(taskId, ActorTaskState.PAUSED_BY_USER);
        ShadowSystemClock.advanceBy(500, TimeUnit.MILLISECONDS);

        mActorMetrics.onTaskStateChangedForTesting(taskId, ActorTaskState.ACTING);
        ShadowSystemClock.advanceBy(200, TimeUnit.MILLISECONDS);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(histogramForegroundActing, 300)
                        .expectIntRecord(
                                "Actor.Tools.ExecutionDuration.Foreground.PausedByUser", 500)
                        .build();

        mActorMetrics.onTaskStateChangedForTesting(taskId, ActorTaskState.FINISHED);
        watcher.assertExpected();
    }
}
