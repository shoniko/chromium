// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_feature_pod_controller.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/network/network_feature_pod_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "ui/base/l10n/l10n_util.h"

using chromeos::NetworkHandler;
using chromeos::NetworkTypePattern;
using chromeos::NetworkState;

namespace ash {

namespace {

void SetNetworkEnabled(bool enabled) {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->ConnectedNetworkByType(
          NetworkTypePattern::NonVirtual());

  // For cellular and tether, users are only allowed to disable them from
  // feature pod toggle.
  if (!enabled && network && network->Matches(NetworkTypePattern::Cellular())) {
    NetworkHandler::Get()->network_state_handler()->SetTechnologyEnabled(
        NetworkTypePattern::Cellular(), false,
        chromeos::network_handler::ErrorCallback());
    return;
  }

  if (!enabled && network && network->Matches(NetworkTypePattern::Tether())) {
    NetworkHandler::Get()->network_state_handler()->SetTechnologyEnabled(
        NetworkTypePattern::Tether(), false,
        chromeos::network_handler::ErrorCallback());
    return;
  }

  if (network && !network->Matches(NetworkTypePattern::WiFi()))
    return;

  NetworkHandler::Get()->network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), enabled,
      chromeos::network_handler::ErrorCallback());
}

}  // namespace

NetworkFeaturePodController::NetworkFeaturePodController(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {}

NetworkFeaturePodController::~NetworkFeaturePodController() = default;

FeaturePodButton* NetworkFeaturePodController::CreateButton() {
  DCHECK(!button_);
  button_ = new NetworkFeaturePodButton(this);
  return button_;
}

void NetworkFeaturePodController::OnIconPressed() {
  bool was_enabled = button_->IsToggled();
  SetNetworkEnabled(!was_enabled);

  // If network was disabled, show network list as well as enabling network.
  if (!was_enabled)
    tray_controller_->ShowNetworkDetailedView();
}

void NetworkFeaturePodController::OnLabelPressed() {
  SetNetworkEnabled(true);
  tray_controller_->ShowNetworkDetailedView();
}

SystemTrayItemUmaType NetworkFeaturePodController::GetUmaType() const {
  return SystemTrayItemUmaType::UMA_NETWORK;
}

}  // namespace ash
