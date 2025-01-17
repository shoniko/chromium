// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_service_network_delegate.h"

#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/url_loader.h"

namespace network {

NetworkServiceNetworkDelegate::NetworkServiceNetworkDelegate(
    NetworkContext* network_context)
    : network_context_(network_context) {}

bool NetworkServiceNetworkDelegate::OnCanGetCookies(
    const net::URLRequest& request,
    const net::CookieList& cookie_list,
    bool allowed_from_caller) {
  URLLoader* url_loader = URLLoader::ForRequest(request);
  if (url_loader) {
    network_context_->network_service()->client()->OnCookiesRead(
        url_loader->GetProcessId(), url_loader->GetRenderFrameId(),
        request.url(), request.site_for_cookies(), cookie_list,
        !allowed_from_caller);
  }
  return allowed_from_caller;
}

bool NetworkServiceNetworkDelegate::OnCanSetCookie(
    const net::URLRequest& request,
    const net::CanonicalCookie& cookie,
    net::CookieOptions* options,
    bool allowed_from_caller) {
  URLLoader* url_loader = URLLoader::ForRequest(request);
  if (url_loader) {
    network_context_->network_service()->client()->OnCookieChange(
        url_loader->GetProcessId(), url_loader->GetRenderFrameId(),
        request.url(), request.site_for_cookies(), cookie,
        !allowed_from_caller);
  }
  return allowed_from_caller;
}

bool NetworkServiceNetworkDelegate::OnCanAccessFile(
    const net::URLRequest& request,
    const base::FilePath& original_path,
    const base::FilePath& absolute_path) const {
  // Match the default implementation (BasicNetworkDelegate)'s behavior for
  // now.
  return true;
}

}  // namespace network
