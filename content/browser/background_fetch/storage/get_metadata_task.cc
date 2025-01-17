// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/get_metadata_task.h"

#include <utility>

#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "url/gurl.h"

namespace content {

namespace background_fetch {

GetMetadataTask::GetMetadataTask(DatabaseTaskHost* host,
                                 int64_t service_worker_registration_id,
                                 const url::Origin& origin,
                                 const std::string& developer_id,
                                 GetMetadataCallback callback)
    : DatabaseTask(host),
      service_worker_registration_id_(service_worker_registration_id),
      origin_(origin),
      developer_id_(developer_id),
      callback_(std::move(callback)),
      weak_factory_(this) {}

GetMetadataTask::~GetMetadataTask() = default;

void GetMetadataTask::Start() {
  service_worker_context()->GetRegistrationUserData(
      service_worker_registration_id_,
      {ActiveRegistrationUniqueIdKey(developer_id_)},
      base::BindOnce(&GetMetadataTask::DidGetUniqueId,
                     weak_factory_.GetWeakPtr()));
}

void GetMetadataTask::DidGetUniqueId(const std::vector<std::string>& data,
                                     blink::ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kNotFound:
      std::move(callback_).Run(blink::mojom::BackgroundFetchError::INVALID_ID,
                               nullptr /* metadata */);
      Finished();  // Destroys |this|.
      return;
    case DatabaseStatus::kOk:
      DCHECK_EQ(1u, data.size());
      service_worker_context()->GetRegistrationUserData(
          service_worker_registration_id_, {RegistrationKey(data[0])},
          base::BindOnce(&GetMetadataTask::DidGetMetadata,
                         weak_factory_.GetWeakPtr()));
      return;
    case DatabaseStatus::kFailed:
      std::move(callback_).Run(
          blink::mojom::BackgroundFetchError::STORAGE_ERROR,
          nullptr /* metadata */);
      Finished();  // Destroys |this|.
      return;
  }
}

void GetMetadataTask::DidGetMetadata(const std::vector<std::string>& data,
                                     blink::ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kNotFound:
      // The database is corrupt as there's no registration data despite there
      // being an active developer_id pointing to it.
      std::move(callback_).Run(
          blink::mojom::BackgroundFetchError::STORAGE_ERROR,
          nullptr /* metadata */);
      Finished();  // Destroys |this|.
      return;
    case DatabaseStatus::kOk:
      DCHECK_EQ(1u, data.size());
      ProcessMetadata(data[0]);
      return;
    case DatabaseStatus::kFailed:
      std::move(callback_).Run(
          blink::mojom::BackgroundFetchError::STORAGE_ERROR,
          nullptr /* metadata */);
      Finished();  // Destroys |this|.
      return;
  }
}

void GetMetadataTask::ProcessMetadata(const std::string& metadata) {
  metadata_proto_ = std::make_unique<proto::BackgroundFetchMetadata>();
  if (!metadata_proto_->ParseFromString(metadata)) {
    std::move(callback_).Run(blink::mojom::BackgroundFetchError::STORAGE_ERROR,
                             nullptr /* metadata */);
    Finished();
    return;
  }

  const auto& registration_proto = metadata_proto_->registration();
  if (registration_proto.developer_id() != developer_id_ ||
      !origin_.IsSameOriginWith(
          url::Origin::Create(GURL(metadata_proto_->origin())))) {
    std::move(callback_).Run(blink::mojom::BackgroundFetchError::STORAGE_ERROR,
                             nullptr /* metadata */);
    Finished();
    return;
  }

  std::move(callback_).Run(blink::mojom::BackgroundFetchError::NONE,
                           std::move(metadata_proto_));
  Finished();
}

}  // namespace background_fetch

}  // namespace content
