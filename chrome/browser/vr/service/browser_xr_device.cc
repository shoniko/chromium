// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/service/browser_xr_device.h"

#include "chrome/browser/vr/service/vr_display_host.h"
#include "device/vr/vr_device.h"

namespace vr {

BrowserXrDevice::BrowserXrDevice(device::mojom::XRRuntimePtr device,
                                 device::mojom::VRDisplayInfoPtr display_info,
                                 bool is_fallback)
    : device_(std::move(device)),
      display_info_(std::move(display_info)),
      is_fallback_(is_fallback),
      binding_(this),
      weak_ptr_factory_(this) {
  device::mojom::XRRuntimeEventListenerPtr listener;
  binding_.Bind(mojo::MakeRequest(&listener));

  // Unretained is safe because we are calling through an InterfacePtr we own,
  // so we won't be called after device_ is destroyed.
  device_->ListenToDeviceChanges(
      std::move(listener),
      base::BindOnce(&BrowserXrDevice::OnInitialDevicePropertiesReceived,
                     base::Unretained(this)));
}

void BrowserXrDevice::OnInitialDevicePropertiesReceived(
    device::mojom::VRDisplayInfoPtr display_info) {
  OnDisplayInfoChanged(std::move(display_info));
}

BrowserXrDevice::~BrowserXrDevice() = default;

void BrowserXrDevice::OnDisplayInfoChanged(
    device::mojom::VRDisplayInfoPtr vr_device_info) {
  display_info_ = vr_device_info.Clone();
  for (VRDisplayHost* display : displays_) {
    display->OnChanged(vr_device_info.Clone());
  }
}

void BrowserXrDevice::StopExclusiveSession() {
  if (exclusive_session_controller_) {
    exclusive_session_controller_ = nullptr;
    presenting_display_host_ = nullptr;
  }
}

void BrowserXrDevice::OnExitPresent() {
  if (presenting_display_host_) {
    presenting_display_host_->OnExitPresent();
    presenting_display_host_ = nullptr;
  }
}

void BrowserXrDevice::OnDeviceActivated(
    device::mojom::VRDisplayEventReason reason,
    base::OnceCallback<void(bool)> on_handled) {
  if (listening_for_activation_display_host_) {
    listening_for_activation_display_host_->OnActivate(reason,
                                                       std::move(on_handled));
  } else {
    std::move(on_handled).Run(true /* will_not_present */);
  }
}

void BrowserXrDevice::OnDeviceIdle(device::mojom::VRDisplayEventReason reason) {
  for (VRDisplayHost* display : displays_) {
    display->OnDeactivate(reason);
  }
}

void BrowserXrDevice::OnDisplayHostAdded(VRDisplayHost* display) {
  displays_.insert(display);
}

void BrowserXrDevice::OnDisplayHostRemoved(VRDisplayHost* display) {
  DCHECK(display);
  displays_.erase(display);
  if (display == presenting_display_host_) {
    ExitPresent(display);
    DCHECK(presenting_display_host_ == nullptr);
  }
  if (display == listening_for_activation_display_host_) {
    // Not listening for activation.
    listening_for_activation_display_host_ = nullptr;
    device_->SetListeningForActivate(false);
  }
}

void BrowserXrDevice::ExitPresent(VRDisplayHost* display) {
  if (display == presenting_display_host_) {
    StopExclusiveSession();
  }
}

void BrowserXrDevice::RequestSession(
    VRDisplayHost* display,
    device::mojom::XRDeviceRuntimeSessionOptionsPtr options,
    device::mojom::VRDisplayHost::RequestSessionCallback callback) {
  device_->RequestSession(
      options->Clone(), base::BindOnce(&BrowserXrDevice::OnRequestSessionResult,
                                       weak_ptr_factory_.GetWeakPtr(), display,
                                       options->Clone(), std::move(callback)));
}

void BrowserXrDevice::OnRequestSessionResult(
    VRDisplayHost* display,
    device::mojom::XRDeviceRuntimeSessionOptionsPtr options,
    device::mojom::VRDisplayHost::RequestSessionCallback callback,
    device::mojom::XRPresentationConnectionPtr connection,
    device::mojom::XRSessionControllerPtr exclusive_session_controller) {
  if (connection && (displays_.find(display) != displays_.end())) {
    if (options->exclusive) {
      presenting_display_host_ = display;
      exclusive_session_controller_ = std::move(exclusive_session_controller);
    }
    std::move(callback).Run(std::move(connection));
  } else {
    std::move(callback).Run(nullptr);
    if (connection) {
      // The device has been removed, but we still got a connection, so make
      // sure to clean up this weird state.
      exclusive_session_controller_ = std::move(exclusive_session_controller);
      StopExclusiveSession();
    }
  }
}

void BrowserXrDevice::UpdateListeningForActivate(VRDisplayHost* display) {
  if (display->ListeningForActivate() && display->InFocusedFrame()) {
    bool was_listening = !!listening_for_activation_display_host_;
    listening_for_activation_display_host_ = display;
    if (!was_listening)
      OnListeningForActivate(true);
  } else if (listening_for_activation_display_host_ == display) {
    listening_for_activation_display_host_ = nullptr;
    OnListeningForActivate(false);
  }
}

void BrowserXrDevice::OnListeningForActivate(bool is_listening) {
  device_->SetListeningForActivate(is_listening);
}

}  // namespace vr
