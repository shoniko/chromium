// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/resource_loading_hints/resource_loading_hints_web_contents_observer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/loader/chrome_navigation_data.h"
#include "chrome/browser/profiles/profile.h"
#include "components/previews/content/previews_content_util.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_user_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/loader/previews_resource_loading_hints.mojom.h"
#include "url/gurl.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ResourceLoadingHintsWebContentsObserver);

ResourceLoadingHintsWebContentsObserver::
    ~ResourceLoadingHintsWebContentsObserver() {}

ResourceLoadingHintsWebContentsObserver::
    ResourceLoadingHintsWebContentsObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

void ResourceLoadingHintsWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument() || navigation_handle->IsErrorPage()) {
    return;
  }

  // Store Previews information for this navigation.
  ChromeNavigationData* nav_data = static_cast<ChromeNavigationData*>(
      navigation_handle->GetNavigationData());
  if (!nav_data || !nav_data->previews_user_data())
    return;

  previews::PreviewsUserData* previews_user_data =
      nav_data->previews_user_data();

  if (previews_user_data->committed_previews_type() !=
      previews::PreviewsType::RESOURCE_LOADING_HINTS) {
    return;
  }

  DCHECK(previews::params::IsResourceLoadingHintsEnabled());
  SendResourceLoadingHints(navigation_handle);
}

void ResourceLoadingHintsWebContentsObserver::SendResourceLoadingHints(
    content::NavigationHandle* navigation_handle) const {
  // Hints should be sent only after the renderer frame has committed.
  DCHECK(navigation_handle->HasCommitted());
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(navigation_handle->GetURL().SchemeIsHTTPOrHTTPS());

  blink::mojom::PreviewsResourceLoadingHintsReceiverPtr hints_receiver_ptr;
  web_contents()->GetMainFrame()->GetRemoteInterfaces()->GetInterface(
      &hints_receiver_ptr);
  blink::mojom::PreviewsResourceLoadingHintsPtr hints_ptr =
      blink::mojom::PreviewsResourceLoadingHints::New();

  // TOOD(tbansal): https://crbug.com/856243. Send an actual list of resource
  // URLs to block.
  hints_ptr->subresources_to_block.push_back(std::string());
  hints_receiver_ptr->SetResourceLoadingHints(std::move(hints_ptr));
}
