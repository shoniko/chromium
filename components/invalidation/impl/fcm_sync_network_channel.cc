// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fcm_sync_network_channel.h"

namespace syncer {

FCMSyncNetworkChannel::FCMSyncNetworkChannel() : received_messages_count_(0) {}

FCMSyncNetworkChannel::~FCMSyncNetworkChannel() {}

void FCMSyncNetworkChannel::SetMessageReceiver(
    MessageCallback incoming_receiver) {
  incoming_receiver_ = std::move(incoming_receiver);
}

void FCMSyncNetworkChannel::SetTokenReceiver(TokenCallback token_receiver) {
  token_receiver_ = token_receiver;
}

void FCMSyncNetworkChannel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FCMSyncNetworkChannel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FCMSyncNetworkChannel::NotifyChannelStateChange(
    InvalidatorState invalidator_state) {
  for (auto& observer : observers_)
    observer.OnFCMSyncNetworkChannelStateChanged(invalidator_state);
}

bool FCMSyncNetworkChannel::DeliverIncomingMessage(const std::string& message) {
  if (!incoming_receiver_) {
    DLOG(ERROR) << "No receiver for incoming notification";
    return false;
  }
  received_messages_count_++;
  incoming_receiver_.Run(message);
  return true;
}

bool FCMSyncNetworkChannel::DeliverToken(const std::string& token) {
  if (!token_receiver_) {
    DLOG(ERROR) << "No receiver for token";
    return false;
  }
  token_receiver_.Run(token);
  return true;
}

int FCMSyncNetworkChannel::GetReceivedMessagesCount() const {
  return received_messages_count_;
}

}  // namespace syncer
