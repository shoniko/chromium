// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/transceiver_state_surfacer.h"

#include "content/renderer/media/webrtc/webrtc_util.h"
#include "third_party/webrtc/api/rtptransceiverinterface.h"

namespace content {

TransceiverStateSurfacer::TransceiverStateSurfacer(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> signaling_task_runner)
    : main_task_runner_(std::move(main_task_runner)),
      signaling_task_runner_(std::move(signaling_task_runner)),
      is_initialized_(false),
      states_obtained_(false) {
  DCHECK(main_task_runner_);
  DCHECK(signaling_task_runner_);
}

TransceiverStateSurfacer::TransceiverStateSurfacer(
    TransceiverStateSurfacer&& other)
    : main_task_runner_(other.main_task_runner_),
      signaling_task_runner_(other.signaling_task_runner_),
      is_initialized_(other.is_initialized_),
      states_obtained_(other.states_obtained_) {
  // Explicitly null |other|'s task runners for use in destructor.
  other.main_task_runner_ = nullptr;
  other.signaling_task_runner_ = nullptr;
}

TransceiverStateSurfacer::~TransceiverStateSurfacer() {
  // It's OK to not be on the main thread if this object has been moved, in
  // which case |main_task_runner_| is null.
  DCHECK(!main_task_runner_ || main_task_runner_->BelongsToCurrentThread());
}

TransceiverStateSurfacer& TransceiverStateSurfacer::operator=(
    TransceiverStateSurfacer&& other) {
  main_task_runner_ = other.main_task_runner_;
  signaling_task_runner_ = other.signaling_task_runner_;
  states_obtained_ = other.states_obtained_;
  // Explicitly null |other|'s task runners for use in destructor.
  other.main_task_runner_ = nullptr;
  other.signaling_task_runner_ = nullptr;
  return *this;
}

void TransceiverStateSurfacer::Initialize(
    scoped_refptr<WebRtcMediaStreamTrackAdapterMap> track_adapter_map,
    std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>
        webrtc_transceivers) {
  DCHECK(signaling_task_runner_->BelongsToCurrentThread());
  DCHECK(!is_initialized_);
  for (auto& webrtc_transceiver : webrtc_transceivers) {
    // Create the sender state.
    base::Optional<RtpSenderState> sender_state;
    auto webrtc_sender = webrtc_transceiver->sender();
    if (webrtc_sender) {
      std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef>
          sender_track_ref;
      if (webrtc_sender->track()) {
        sender_track_ref =
            track_adapter_map->GetLocalTrackAdapter(webrtc_sender->track());
        DCHECK(sender_track_ref);
      }
      sender_state = RtpSenderState(
          main_task_runner_, signaling_task_runner_, webrtc_sender.get(),
          std::move(sender_track_ref), webrtc_sender->stream_ids());
    }
    // Create the receiver state.
    base::Optional<RtpReceiverState> receiver_state;
    auto webrtc_receiver = webrtc_transceiver->receiver();
    if (webrtc_receiver) {
      DCHECK(webrtc_receiver->track());
      auto receiver_track_ref =
          track_adapter_map->GetOrCreateRemoteTrackAdapter(
              webrtc_receiver->track().get());
      DCHECK(receiver_track_ref);
      std::vector<std::string> receiver_stream_ids;
      for (auto& stream : webrtc_receiver->streams()) {
        receiver_stream_ids.push_back(stream->id());
      }
      receiver_state = RtpReceiverState(
          main_task_runner_, signaling_task_runner_, webrtc_receiver.get(),
          std::move(receiver_track_ref), std::move(receiver_stream_ids));
    }
    // Create the transceiver state.
    transceiver_states_.push_back(RtpTransceiverState(
        main_task_runner_, signaling_task_runner_, webrtc_transceiver.get(),
        std::move(sender_state), std::move(receiver_state),
        ToBaseOptional(webrtc_transceiver->mid()),
        webrtc_transceiver->stopped(), webrtc_transceiver->direction(),
        ToBaseOptional(webrtc_transceiver->current_direction())));
  }
  is_initialized_ = true;
}

std::vector<RtpTransceiverState> TransceiverStateSurfacer::ObtainStates() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(is_initialized_);
  for (auto& transceiver_state : transceiver_states_)
    transceiver_state.Initialize();
  states_obtained_ = true;
  return std::move(transceiver_states_);
}

SurfaceSenderStateOnly::SurfaceSenderStateOnly(
    rtc::scoped_refptr<webrtc::RtpSenderInterface> sender)
    : sender_(std::move(sender)) {
  DCHECK(sender_);
}

SurfaceSenderStateOnly::~SurfaceSenderStateOnly() {}

cricket::MediaType SurfaceSenderStateOnly::media_type() const {
  return sender_->media_type();
}

absl::optional<std::string> SurfaceSenderStateOnly::mid() const {
  return absl::nullopt;
}

rtc::scoped_refptr<webrtc::RtpSenderInterface> SurfaceSenderStateOnly::sender()
    const {
  return sender_;
}

rtc::scoped_refptr<webrtc::RtpReceiverInterface>
SurfaceSenderStateOnly::receiver() const {
  return nullptr;
}

bool SurfaceSenderStateOnly::stopped() const {
  return false;
}

webrtc::RtpTransceiverDirection SurfaceSenderStateOnly::direction() const {
  return webrtc::RtpTransceiverDirection::kSendOnly;
}

void SurfaceSenderStateOnly::SetDirection(
    webrtc::RtpTransceiverDirection new_direction) {
  NOTIMPLEMENTED();
}

absl::optional<webrtc::RtpTransceiverDirection>
SurfaceSenderStateOnly::current_direction() const {
  return absl::nullopt;
}

void SurfaceSenderStateOnly::Stop() {
  NOTIMPLEMENTED();
}

void SurfaceSenderStateOnly::SetCodecPreferences(
    rtc::ArrayView<webrtc::RtpCodecCapability> codecs) {
  NOTIMPLEMENTED();
}

SurfaceReceiverStateOnly::SurfaceReceiverStateOnly(
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver)
    : receiver_(std::move(receiver)) {
  DCHECK(receiver_);
}

SurfaceReceiverStateOnly::~SurfaceReceiverStateOnly() {}

cricket::MediaType SurfaceReceiverStateOnly::media_type() const {
  return receiver_->media_type();
}

absl::optional<std::string> SurfaceReceiverStateOnly::mid() const {
  return absl::nullopt;
}

rtc::scoped_refptr<webrtc::RtpSenderInterface>
SurfaceReceiverStateOnly::sender() const {
  return nullptr;
}

rtc::scoped_refptr<webrtc::RtpReceiverInterface>
SurfaceReceiverStateOnly::receiver() const {
  return receiver_;
}

bool SurfaceReceiverStateOnly::stopped() const {
  return false;
}

webrtc::RtpTransceiverDirection SurfaceReceiverStateOnly::direction() const {
  return webrtc::RtpTransceiverDirection::kRecvOnly;
}

void SurfaceReceiverStateOnly::SetDirection(
    webrtc::RtpTransceiverDirection new_direction) {
  NOTIMPLEMENTED();
}

absl::optional<webrtc::RtpTransceiverDirection>
SurfaceReceiverStateOnly::current_direction() const {
  return absl::nullopt;
}

void SurfaceReceiverStateOnly::Stop() {
  NOTIMPLEMENTED();
}

void SurfaceReceiverStateOnly::SetCodecPreferences(
    rtc::ArrayView<webrtc::RtpCodecCapability> codecs) {
  NOTIMPLEMENTED();
}

}  // namespace content
