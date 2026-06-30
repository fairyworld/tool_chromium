// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/voice_isolation_handler.h"

#include <utility>

#include "media/base/audio_bus.h"

namespace audio {

// TODO(tomasl): Add a parameter to pass a VoiceIsolation instance.
VoiceIsolationHandler::VoiceIsolationHandler(
    DeliverProcessedAudioCallback deliver_processed_audio_callback)
    : deliver_processed_audio_callback_(
          std::move(deliver_processed_audio_callback)) {}

VoiceIsolationHandler::~VoiceIsolationHandler() = default;

void VoiceIsolationHandler::ProcessCapturedAudio(
    const media::AudioBus& audio_source,
    base::TimeTicks audio_capture_time,
    std::optional<double> volume,
    const media::AudioGlitchInfo& audio_glitch_info) {
  // TODO(tomasl): Do voice isolation denoising here using the new
  // VoiceIsolation component.
  deliver_processed_audio_callback_.Run(audio_source, audio_capture_time,
                                        volume, audio_glitch_info);
}

}  // namespace audio
