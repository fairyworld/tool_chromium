// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mac/vsync_provider_mac.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/bind_post_task.h"
#include "base/trace_event/trace_event.h"

namespace ui {

// static
VSyncProviderMac* VSyncProviderMac::GetInstance() {
  static base::NoDestructor<VSyncProviderMac> provider;
  return provider.get();
}

VSyncProviderMac::VSyncProviderMac()
    : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {}

VSyncProviderMac::~VSyncProviderMac() = default;

bool VSyncProviderMac::IsDisplayLinkInBrowserValid(int64_t vsync_display_id) {
  CGDirectDisplayID display_id =
      base::checked_cast<CGDirectDisplayID>(vsync_display_id);

  if (!task_runner_->BelongsToCurrentThread()) {
    // `callback_lists_` is updated on the Viz thread. When called on a non-Viz
    // thread (such as `CrGpuMain` or `CompositorGpuThread`), we must acquire
    // `id_lock_` to prevent data races and ensure memory visibility.
    base::AutoLock lock(id_lock_);
    return callback_lists_.contains(display_id);
  } else {
    return callback_lists_.contains(display_id);
  }
}

void VSyncProviderMac::SetSupportedDisplayLinkId(int64_t vsync_display_id,
                                                 bool is_supported) {
  CGDirectDisplayID display_id =
      base::checked_cast<CGDirectDisplayID>(vsync_display_id);

  if (is_supported) {
    AddSupportedDisplayLinkId(display_id);
  } else {
    RemoveSupportedDisplayLinkId(display_id);
  }
}

void VSyncProviderMac::AddSupportedDisplayLinkId(CGDirectDisplayID display_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vsync_sequence_checker_);

  // Initialize request tracking for histograms. This is only accessed on the
  // VizCompositorThread; no lock is required.
  begin_frame_request_times_.emplace(display_id, base::TimeTicks());

  base::AutoLock lock(id_lock_);
  auto found = callback_lists_.find(display_id);
  if (found == callback_lists_.end()) {
    // Insert an empty callback list.
    auto result = callback_lists_.insert(
        std::make_pair(display_id, std::list<VSyncCallbackMac::Callback>()));
    bool inserted = result.second;
    DCHECK(inserted);
  }
}

void VSyncProviderMac::RemoveSupportedDisplayLinkId(
    CGDirectDisplayID display_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vsync_sequence_checker_);

  // Accessed on the VizCompositorThread; no lock is required.
  begin_frame_request_times_.erase(display_id);

  base::AutoLock lock(id_lock_);
  auto found = callback_lists_.find(display_id);
  if (found != callback_lists_.end()) {
    callback_lists_.erase(display_id);
  }
}

void VSyncProviderMac::RegisterCallback(VSyncCallbackMac::Callback callback,
                                        CGDirectDisplayID display_id) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VSyncProviderMac::RegisterCallback,
                                  base::Unretained(this), std::move(callback),
                                  display_id));
    return;
  }

  auto found = callback_lists_.find(display_id);
  if (found == callback_lists_.end()) {
    return;
  }

  std::list<VSyncCallbackMac::Callback>& callbacks = found->second;
  bool should_request_begin_frame = callbacks.empty();

  callbacks.push_back(std::move(callback));

  // Request BeginFrames from the browser via IPC.
  if (should_request_begin_frame) {
    begin_frame_request_times_[display_id] = base::TimeTicks::Now();

    needs_begin_frame_callback_.Run(display_id,
                                    /*needs_begin_frames=*/true);
  }
}

void VSyncProviderMac::UnregisterCallback(VSyncCallbackMac::Callback callback,
                                          CGDirectDisplayID display_id) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VSyncProviderMac::UnregisterCallback,
                                  base::Unretained(this), std::move(callback),
                                  display_id));
    return;
  }

  auto found = callback_lists_.find(display_id);
  if (found == callback_lists_.end()) {
    return;
  }

  // Reset the BeginFrame request timestamp for this display.
  begin_frame_request_times_[display_id] = base::TimeTicks();

  std::list<VSyncCallbackMac::Callback>& callbacks = found->second;
  callbacks.remove(callback);

  // Stop BeginFrame in browser via IPC.
  if (callbacks.empty()) {
    needs_begin_frame_callback_.Run(display_id,
                                    /*needs_begin_frames=*/false);
  }
}

void VSyncProviderMac::OnVSync(const VSyncParamsMac& params,
                               int64_t vsync_display_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vsync_sequence_checker_);
  TRACE_EVENT0("gpu", "VSyncProviderMac::OnVSync");

  CGDirectDisplayID display_id =
      base::checked_cast<CGDirectDisplayID>(vsync_display_id);

  // DisplayLink entry might no longer exist.
  auto found = callback_lists_.find(display_id);
  if (found == callback_lists_.end()) {
    return;
  }

  RecordTimeFromNeedsBeginFramesToVSync(display_id);

  // Unregister() might be called inside the loop and
  // |callback_lists_.[display_id]| size changes while callbacks are called. Get
  // a local copy here.
  std::list<VSyncCallbackMac::Callback> local_callbacks = found->second;

  // Run callbacks.
  for (auto& cb : local_callbacks) {
    cb.Run(params);
  }
}

void VSyncProviderMac::SetCallbackForRemoteNeedsBeginFrame(
    NeedsBeginFrameCB callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vsync_sequence_checker_);

  needs_begin_frame_callback_ = std::move(callback);
}

bool VSyncProviderMac::BelongsToCurrentThread() {
  return task_runner_->BelongsToCurrentThread();
}

bool VSyncProviderMac::IsConnectedToBrowserOnVizThread() {
  if (!task_runner_->BelongsToCurrentThread()) {
    return false;
  }

  return !needs_begin_frame_callback_.is_null();
}

void VSyncProviderMac::RecordTimeFromNeedsBeginFramesToVSync(
    CGDirectDisplayID display_id) {
  // The kMaxKeepAliveCount is 20 frames in ExternalBeginFrameSourceMac. That
  // means the worse case scenario is logging 6 times per seconds on a 120Hz
  // display. This number is far from the real world cases because most webpages
  // don't do frequent rendering start/stop change.
  auto it = begin_frame_request_times_.find(display_id);
  if (it != begin_frame_request_times_.end() && !it->second.is_null()) {
    base::TimeDelta delta = base::TimeTicks::Now() - it->second;
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Viz.DisplayLink.IpcTimeFromNeedsBeginFramesToVSyncReceived", delta,
        base::Milliseconds(1), base::Minutes(30), 50);

    // Reset the timestamp so that the metric is only recorded once per request.
    it->second = base::TimeTicks();
  }
}

}  // namespace ui
