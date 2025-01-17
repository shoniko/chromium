// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_BLOCKING_MODEL_TYPE_STORE_H_
#define COMPONENTS_SYNC_MODEL_BLOCKING_MODEL_TYPE_STORE_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_store_base.h"

namespace syncer {

class MetadataBatch;

// BlockingModelTypeStore represents a synchronous API for a leveldb-based
// persistence layer, with support for metadata.
class BlockingModelTypeStore : public ModelTypeStoreBase {
 public:
  virtual base::Optional<ModelError> ReadData(const IdList& id_list,
                                              RecordList* data_records,
                                              IdList* missing_id_list) = 0;
  virtual base::Optional<ModelError> ReadAllData(RecordList* data_records) = 0;
  virtual base::Optional<ModelError> ReadAllMetadata(
      MetadataBatch* metadata_batch) = 0;

  virtual std::unique_ptr<WriteBatch> CreateWriteBatch() = 0;
  virtual base::Optional<ModelError> CommitWriteBatch(
      std::unique_ptr<WriteBatch> write_batch) = 0;
  virtual base::Optional<ModelError> DeleteAllDataAndMetadata() = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_BLOCKING_MODEL_TYPE_STORE_H_
