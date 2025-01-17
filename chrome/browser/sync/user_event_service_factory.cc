// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/user_event_service_factory.h"

#include <memory>
#include <utility>

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/user_events/no_op_user_event_service.h"
#include "components/sync/user_events/user_event_service_impl.h"
#include "components/sync/user_events/user_event_sync_bridge.h"

namespace browser_sync {

// static
UserEventServiceFactory* UserEventServiceFactory::GetInstance() {
  return base::Singleton<UserEventServiceFactory>::get();
}

// static
syncer::UserEventService* UserEventServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<syncer::UserEventService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

UserEventServiceFactory::UserEventServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "UserEventService",
          BrowserContextDependencyManager::GetInstance()) {
  // TODO(vitaliii): This is missing
  // DependsOn(ProfileSyncServiceFactory::GetInstance()), which we can't
  // simply add because ProfileSyncServiceFactory itself depends on this
  // factory. This won't be relevant anymore once the separate consents datatype
  // is fully launched.
}

UserEventServiceFactory::~UserEventServiceFactory() {}

KeyedService* UserEventServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  if (!syncer::UserEventServiceImpl::MightRecordEvents(
          context->IsOffTheRecord(), sync_service)) {
    return new syncer::NoOpUserEventService();
  }

  syncer::OnceModelTypeStoreFactory store_factory =
      browser_sync::ProfileSyncService::GetModelTypeStoreFactory(
          profile->GetPath());
  auto change_processor =
      std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
          syncer::USER_EVENTS,
          base::BindRepeating(&syncer::ReportUnrecoverableError,
                              chrome::GetChannel()));
  auto bridge = std::make_unique<syncer::UserEventSyncBridge>(
      std::move(store_factory), std::move(change_processor),
      sync_service->GetGlobalIdMapper());
  return new syncer::UserEventServiceImpl(sync_service, std::move(bridge));
}

content::BrowserContext* UserEventServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

}  // namespace browser_sync
