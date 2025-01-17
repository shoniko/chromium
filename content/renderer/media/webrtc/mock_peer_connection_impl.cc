// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/mock_peer_connection_impl.h"

#include <stddef.h>

#include <vector>

#include "base/logging.h"
#include "base/stl_util.h"
#include "content/renderer/media/webrtc/mock_data_channel_impl.h"
#include "content/renderer/media/webrtc/mock_peer_connection_dependency_factory.h"
#include "third_party/webrtc/api/rtpreceiverinterface.h"
#include "third_party/webrtc/rtc_base/refcountedobject.h"

using testing::_;
using webrtc::AudioTrackInterface;
using webrtc::CreateSessionDescriptionObserver;
using webrtc::DtmfSenderInterface;
using webrtc::DtmfSenderObserverInterface;
using webrtc::IceCandidateInterface;
using webrtc::MediaStreamInterface;
using webrtc::PeerConnectionInterface;
using webrtc::SessionDescriptionInterface;
using webrtc::SetSessionDescriptionObserver;

namespace content {

class MockStreamCollection : public webrtc::StreamCollectionInterface {
 public:
  size_t count() override { return streams_.size(); }
  MediaStreamInterface* at(size_t index) override { return streams_[index]; }
  MediaStreamInterface* find(const std::string& id) override {
    for (size_t i = 0; i < streams_.size(); ++i) {
      if (streams_[i]->id() == id)
        return streams_[i];
    }
    return nullptr;
  }
  webrtc::MediaStreamTrackInterface* FindAudioTrack(
      const std::string& id) override {
    for (size_t i = 0; i < streams_.size(); ++i) {
      webrtc::MediaStreamTrackInterface* track =
          streams_.at(i)->FindAudioTrack(id);
      if (track)
        return track;
    }
    return nullptr;
  }
  webrtc::MediaStreamTrackInterface* FindVideoTrack(
      const std::string& id) override {
    for (size_t i = 0; i < streams_.size(); ++i) {
      webrtc::MediaStreamTrackInterface* track =
          streams_.at(i)->FindVideoTrack(id);
      if (track)
        return track;
    }
    return nullptr;
  }
  void AddStream(MediaStreamInterface* stream) {
    streams_.push_back(stream);
  }
  void RemoveStream(MediaStreamInterface* stream) {
    StreamVector::iterator it = streams_.begin();
    for (; it != streams_.end(); ++it) {
      if (it->get() == stream) {
        streams_.erase(it);
        break;
      }
    }
  }

 protected:
  ~MockStreamCollection() override {}

 private:
  typedef std::vector<rtc::scoped_refptr<MediaStreamInterface> >
      StreamVector;
  StreamVector streams_;
};

class MockDtmfSender : public DtmfSenderInterface {
 public:
  void RegisterObserver(DtmfSenderObserverInterface* observer) override {
    observer_ = observer;
  }
  void UnregisterObserver() override { observer_ = nullptr; }
  bool CanInsertDtmf() override { return true; }
  bool InsertDtmf(const std::string& tones,
                  int duration,
                  int inter_tone_gap) override {
    tones_ = tones;
    duration_ = duration;
    inter_tone_gap_ = inter_tone_gap;
    return true;
  }
  std::string tones() const override { return tones_; }
  int duration() const override { return duration_; }
  int inter_tone_gap() const override { return inter_tone_gap_; }

 private:
  DtmfSenderObserverInterface* observer_ = nullptr;
  std::string tones_;
  int duration_ = 0;
  int inter_tone_gap_ = 0;
};

FakeRtpSender::FakeRtpSender(
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
    std::vector<std::string> stream_ids)
    : track_(std::move(track)), stream_ids_(std::move(stream_ids)) {}

FakeRtpSender::~FakeRtpSender() {}

bool FakeRtpSender::SetTrack(webrtc::MediaStreamTrackInterface* track) {
  NOTIMPLEMENTED();
  return false;
}

rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> FakeRtpSender::track()
    const {
  return track_;
}

uint32_t FakeRtpSender::ssrc() const {
  NOTIMPLEMENTED();
  return 0;
}

cricket::MediaType FakeRtpSender::media_type() const {
  NOTIMPLEMENTED();
  return cricket::MEDIA_TYPE_AUDIO;
}

std::string FakeRtpSender::id() const {
  NOTIMPLEMENTED();
  return "";
}

std::vector<std::string> FakeRtpSender::stream_ids() const {
  return stream_ids_;
}

webrtc::RtpParameters FakeRtpSender::GetParameters() {
  NOTIMPLEMENTED();
  return webrtc::RtpParameters();
}

webrtc::RTCError FakeRtpSender::SetParameters(
    const webrtc::RtpParameters& parameters) {
  NOTIMPLEMENTED();
  return webrtc::RTCError::OK();
}

rtc::scoped_refptr<webrtc::DtmfSenderInterface> FakeRtpSender::GetDtmfSender()
    const {
  return new rtc::RefCountedObject<MockDtmfSender>();
}

FakeRtpReceiver::FakeRtpReceiver(
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
    std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>> streams)
    : track_(std::move(track)), streams_(std::move(streams)) {}

FakeRtpReceiver::~FakeRtpReceiver() {}

rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> FakeRtpReceiver::track()
    const {
  return track_;
}

std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>
FakeRtpReceiver::streams() const {
  return streams_;
}

cricket::MediaType FakeRtpReceiver::media_type() const {
  NOTIMPLEMENTED();
  return cricket::MEDIA_TYPE_AUDIO;
}

std::string FakeRtpReceiver::id() const {
  NOTIMPLEMENTED();
  return "";
}

webrtc::RtpParameters FakeRtpReceiver::GetParameters() const {
  NOTIMPLEMENTED();
  return webrtc::RtpParameters();
}

bool FakeRtpReceiver::SetParameters(const webrtc::RtpParameters& parameters) {
  NOTIMPLEMENTED();
  return false;
}

void FakeRtpReceiver::SetObserver(
    webrtc::RtpReceiverObserverInterface* observer) {
  NOTIMPLEMENTED();
}

std::vector<webrtc::RtpSource> FakeRtpReceiver::GetSources() const {
  NOTIMPLEMENTED();
  return std::vector<webrtc::RtpSource>();
}

FakeRtpTransceiver::FakeRtpTransceiver(
    cricket::MediaType media_type,
    rtc::scoped_refptr<webrtc::RtpSenderInterface> sender,
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver)
    : media_type_(media_type),
      sender_(std::move(sender)),
      receiver_(std::move(receiver)) {}

FakeRtpTransceiver::~FakeRtpTransceiver() {}

cricket::MediaType FakeRtpTransceiver::media_type() const {
  return media_type_;
}

absl::optional<std::string> FakeRtpTransceiver::mid() const {
  return absl::nullopt;
}

rtc::scoped_refptr<webrtc::RtpSenderInterface> FakeRtpTransceiver::sender()
    const {
  return sender_;
}

rtc::scoped_refptr<webrtc::RtpReceiverInterface> FakeRtpTransceiver::receiver()
    const {
  return receiver_;
}

bool FakeRtpTransceiver::stopped() const {
  return false;
}

webrtc::RtpTransceiverDirection FakeRtpTransceiver::direction() const {
  return webrtc::RtpTransceiverDirection::kSendRecv;
}

void FakeRtpTransceiver::SetDirection(
    webrtc::RtpTransceiverDirection new_direction) {
  NOTIMPLEMENTED();
}

absl::optional<webrtc::RtpTransceiverDirection>
FakeRtpTransceiver::current_direction() const {
  return absl::nullopt;
}

void FakeRtpTransceiver::Stop() {
  NOTIMPLEMENTED();
}

void FakeRtpTransceiver::SetCodecPreferences(
    rtc::ArrayView<webrtc::RtpCodecCapability> codecs) {
  NOTIMPLEMENTED();
}

const char MockPeerConnectionImpl::kDummyOffer[] = "dummy offer";
const char MockPeerConnectionImpl::kDummyAnswer[] = "dummy answer";

MockPeerConnectionImpl::MockPeerConnectionImpl(
    MockPeerConnectionDependencyFactory* factory,
    webrtc::PeerConnectionObserver* observer)
    : dependency_factory_(factory),
      remote_streams_(new rtc::RefCountedObject<MockStreamCollection>),
      hint_audio_(false),
      hint_video_(false),
      getstats_result_(true),
      sdp_mline_index_(-1),
      observer_(observer) {
  ON_CALL(*this, SetLocalDescription(_, _)).WillByDefault(testing::Invoke(
      this, &MockPeerConnectionImpl::SetLocalDescriptionWorker));
  // TODO(hbos): Remove once no longer mandatory to implement.
  ON_CALL(*this, SetRemoteDescription(_, _)).WillByDefault(testing::Invoke(
      this, &MockPeerConnectionImpl::SetRemoteDescriptionWorker));
  ON_CALL(*this, SetRemoteDescriptionForMock(_, _))
      .WillByDefault(testing::Invoke(
          [this](
              std::unique_ptr<webrtc::SessionDescriptionInterface>* desc,
              rtc::scoped_refptr<webrtc::SetRemoteDescriptionObserverInterface>*
                  observer) {
            SetRemoteDescriptionWorker(nullptr, desc->release());
          }));
}

MockPeerConnectionImpl::~MockPeerConnectionImpl() {}

webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpSenderInterface>>
MockPeerConnectionImpl::AddTrack(
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
    const std::vector<std::string>& stream_ids) {
  DCHECK(track);
  DCHECK_EQ(1u, stream_ids.size());
  for (const auto& sender : senders_) {
    if (sender->track() == track)
      return webrtc::RTCError(webrtc::RTCErrorType::INVALID_PARAMETER);
  }
  for (const auto& stream_id : stream_ids) {
    if (!base::ContainsValue(local_stream_ids_, stream_id)) {
      stream_label_ = stream_id;
      local_stream_ids_.push_back(stream_id);
    }
  }
  auto* sender = new rtc::RefCountedObject<FakeRtpSender>(track, stream_ids);
  senders_.push_back(sender);
  return rtc::scoped_refptr<webrtc::RtpSenderInterface>(sender);
}

bool MockPeerConnectionImpl::RemoveTrack(webrtc::RtpSenderInterface* s) {
  rtc::scoped_refptr<FakeRtpSender> sender = static_cast<FakeRtpSender*>(s);
  auto it = std::find(senders_.begin(), senders_.end(), sender);
  if (it == senders_.end())
    return false;
  senders_.erase(it);
  auto track = sender->track();

  for (const auto& stream_id : sender->stream_ids()) {
    auto local_stream_it = std::find(local_stream_ids_.begin(),
                                     local_stream_ids_.end(), stream_id);
    if (local_stream_it != local_stream_ids_.end())
      local_stream_ids_.erase(local_stream_it);
  }
  return true;
}

std::vector<rtc::scoped_refptr<webrtc::RtpSenderInterface>>
MockPeerConnectionImpl::GetSenders() const {
  std::vector<rtc::scoped_refptr<webrtc::RtpSenderInterface>> senders;
  for (const auto& sender : senders_)
    senders.push_back(sender);
  return senders;
}

std::vector<rtc::scoped_refptr<webrtc::RtpReceiverInterface>>
MockPeerConnectionImpl::GetReceivers() const {
  std::vector<rtc::scoped_refptr<webrtc::RtpReceiverInterface>> receivers;
  for (size_t i = 0; i < remote_streams_->count(); ++i) {
    for (const auto& audio_track : remote_streams_->at(i)->GetAudioTracks()) {
      receivers.push_back(
          new rtc::RefCountedObject<FakeRtpReceiver>(audio_track));
    }
    for (const auto& video_track : remote_streams_->at(i)->GetVideoTracks()) {
      receivers.push_back(
          new rtc::RefCountedObject<FakeRtpReceiver>(video_track));
    }
  }
  return receivers;
}

rtc::scoped_refptr<webrtc::DataChannelInterface>
MockPeerConnectionImpl::CreateDataChannel(const std::string& label,
                      const webrtc::DataChannelInit* config) {
  return new rtc::RefCountedObject<MockDataChannel>(label, config);
}

bool MockPeerConnectionImpl::GetStats(
    webrtc::StatsObserver* observer,
    webrtc::MediaStreamTrackInterface* track,
    StatsOutputLevel level) {
  if (!getstats_result_)
    return false;

  DCHECK_EQ(kStatsOutputLevelStandard, level);
  webrtc::StatsReport report1(webrtc::StatsReport::NewTypedId(
      webrtc::StatsReport::kStatsReportTypeSsrc, "1234"));
  webrtc::StatsReport report2(webrtc::StatsReport::NewTypedId(
      webrtc::StatsReport::kStatsReportTypeSession, "nontrack"));
  report1.set_timestamp(42);
  report1.AddString(webrtc::StatsReport::kStatsValueNameFingerprint,
                    "trackvalue");

  webrtc::StatsReports reports;
  reports.push_back(&report1);

  // If selector is given, we pass back one report.
  // If selector is not given, we pass back two.
  if (!track) {
    report2.set_timestamp(44);
    report2.AddString(webrtc::StatsReport::kStatsValueNameFingerprintAlgorithm,
                      "somevalue");
    reports.push_back(&report2);
  }

  // Note that the callback is synchronous, not asynchronous; it will
  // happen before the request call completes.
  observer->OnComplete(reports);

  return true;
}

void MockPeerConnectionImpl::GetStats(
    webrtc::RTCStatsCollectorCallback* callback) {
  DCHECK(callback);
  DCHECK(stats_report_);
  callback->OnStatsDelivered(stats_report_);
}

void MockPeerConnectionImpl::GetStats(
    rtc::scoped_refptr<webrtc::RtpSenderInterface> selector,
    rtc::scoped_refptr<webrtc::RTCStatsCollectorCallback> callback) {
  callback->OnStatsDelivered(stats_report_);
}

void MockPeerConnectionImpl::GetStats(
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> selector,
    rtc::scoped_refptr<webrtc::RTCStatsCollectorCallback> callback) {
  callback->OnStatsDelivered(stats_report_);
}

void MockPeerConnectionImpl::SetGetStatsReport(webrtc::RTCStatsReport* report) {
  stats_report_ = report;
}

const webrtc::SessionDescriptionInterface*
MockPeerConnectionImpl::local_description() const {
  return local_desc_.get();
}

const webrtc::SessionDescriptionInterface*
MockPeerConnectionImpl::remote_description() const {
  return remote_desc_.get();
}

void MockPeerConnectionImpl::AddRemoteStream(MediaStreamInterface* stream) {
  remote_streams_->AddStream(stream);
}

void MockPeerConnectionImpl::CreateOffer(
    CreateSessionDescriptionObserver* observer,
    const RTCOfferAnswerOptions& options) {
  DCHECK(observer);
  created_sessiondescription_.reset(
      dependency_factory_->CreateSessionDescription("unknown", kDummyOffer,
                                                    nullptr));
}

void MockPeerConnectionImpl::CreateAnswer(
    CreateSessionDescriptionObserver* observer,
    const RTCOfferAnswerOptions& options) {
  DCHECK(observer);
  created_sessiondescription_.reset(
      dependency_factory_->CreateSessionDescription("unknown", kDummyAnswer,
                                                    nullptr));
}

void MockPeerConnectionImpl::SetLocalDescriptionWorker(
    SetSessionDescriptionObserver* observer,
    SessionDescriptionInterface* desc) {
  desc->ToString(&description_sdp_);
  local_desc_.reset(desc);
}

void MockPeerConnectionImpl::SetRemoteDescriptionWorker(
    SetSessionDescriptionObserver* observer,
    SessionDescriptionInterface* desc) {
  desc->ToString(&description_sdp_);
  remote_desc_.reset(desc);
}

bool MockPeerConnectionImpl::SetConfiguration(
    const RTCConfiguration& configuration,
    webrtc::RTCError* error) {
  if (setconfiguration_error_type_ == webrtc::RTCErrorType::NONE) {
    return true;
  }
  error->set_type(setconfiguration_error_type_);
  return false;
}

bool MockPeerConnectionImpl::AddIceCandidate(
    const IceCandidateInterface* candidate) {
  sdp_mid_ = candidate->sdp_mid();
  sdp_mline_index_ = candidate->sdp_mline_index();
  return candidate->ToString(&ice_sdp_);
}

void MockPeerConnectionImpl::RegisterUMAObserver(
    webrtc::UMAObserver* observer) {
  NOTIMPLEMENTED();
}

webrtc::RTCError MockPeerConnectionImpl::SetBitrate(
    const webrtc::BitrateSettings& bitrate) {
  NOTIMPLEMENTED();
  return webrtc::RTCError::OK();
}

}  // namespace content
