// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_IMPL_MODEL_TYPE_STORE_IMPL_H_
#define COMPONENTS_SYNC_MODEL_IMPL_MODEL_TYPE_STORE_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/model_type_store.h"

namespace syncer {

class BlockingModelTypeStoreImpl;

// ModelTypeStoreImpl handles details of store initialization and threading.
// Actual leveldb IO calls are performed in BlockingModelTypeStoreImpl (in the
// underlying ModelTypeStoreBackend).
class ModelTypeStoreImpl : public ModelTypeStore {
 public:
  ~ModelTypeStoreImpl() override;

  static void CreateStore(ModelType type,
                          const std::string& path,
                          InitCallback callback);
  static void CreateInMemoryStoreForTest(ModelType type, InitCallback callback);

  // ModelTypeStore implementation.
  void ReadData(const IdList& id_list, ReadDataCallback callback) override;
  void ReadAllData(ReadAllDataCallback callback) override;
  void ReadAllMetadata(ReadMetadataCallback callback) override;
  std::unique_ptr<WriteBatch> CreateWriteBatch() override;
  void CommitWriteBatch(std::unique_ptr<WriteBatch> write_batch,
                        CallbackWithResult callback) override;
  void DeleteAllDataAndMetadata(CallbackWithResult callback) override;

 private:
  static void BackendInitDone(
      ModelType type,
      std::unique_ptr<base::Optional<ModelError>> result,
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
      InitCallback callback,
      std::unique_ptr<BlockingModelTypeStoreImpl> backend_store);

  ModelTypeStoreImpl(
      ModelType type,
      std::unique_ptr<BlockingModelTypeStoreImpl> backend_store,
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner);

  // Callbacks for different calls to ModelTypeStoreBackend.
  void ReadDataDone(ReadDataCallback callback,
                    std::unique_ptr<RecordList> record_list,
                    std::unique_ptr<IdList> missing_id_list,
                    const base::Optional<ModelError>& error);
  void ReadAllDataDone(ReadAllDataCallback callback,
                       std::unique_ptr<RecordList> record_list,
                       const base::Optional<ModelError>& error);
  void ReadAllMetadataDone(ReadMetadataCallback callback,
                           std::unique_ptr<MetadataBatch> metadata_batch,
                           const base::Optional<ModelError>& error);
  void WriteModificationsDone(CallbackWithResult callback,
                              const base::Optional<ModelError>& error);

  const ModelType type_;
  // |backend_store_| should be deleted on backend thread.
  // To accomplish this store's dtor posts task to backend thread passing
  // backend ownership to task parameter.
  std::unique_ptr<BlockingModelTypeStoreImpl> backend_store_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ModelTypeStoreImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ModelTypeStoreImpl);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_IMPL_MODEL_TYPE_STORE_IMPL_H_
