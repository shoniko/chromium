// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/texture_pool.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/service/abstract_texture.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "media/gpu/fake_command_buffer_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

using gpu::gles2::AbstractTexture;
using testing::_;
using testing::NiceMock;
using testing::Return;

// SupportsWeakPtr so it's easy to tell when it has been destroyed.
class MockAbstractTexture : public NiceMock<AbstractTexture>,
                            public base::SupportsWeakPtr<MockAbstractTexture> {
 public:
  MockAbstractTexture() {}
  ~MockAbstractTexture() override {}

  MOCK_METHOD0(ForceContextLost, void());
  MOCK_CONST_METHOD0(GetTextureBase, gpu::TextureBase*());
  MOCK_METHOD2(SetParameteri, void(GLenum pname, GLint param));
  MOCK_METHOD2(BindStreamTextureImage,
               void(gpu::gles2::GLStreamTextureImage* image,
                    GLuint service_id));
  MOCK_METHOD2(BindImage, void(gl::GLImage* image, bool client_managed));
  MOCK_METHOD0(ReleaseImage, void());
  MOCK_METHOD0(SetCleared, void());
};

class TexturePoolTest : public testing::Test {
 public:
  void SetUp() override {
    task_runner_ = base::ThreadTaskRunnerHandle::Get();
    helper_ = base::MakeRefCounted<FakeCommandBufferHelper>(task_runner_);
    texture_pool_ = new TexturePool(helper_);
    // Random sync token that HasData().
    sync_token_ = gpu::SyncToken(gpu::CommandBufferNamespace::GPU_IO,
                                 gpu::CommandBufferId::FromUnsafeValue(1), 1);
    ASSERT_TRUE(sync_token_.HasData());
  }

  ~TexturePoolTest() override {
    helper_->StubLost();
    base::RunLoop().RunUntilIdle();
  }

  using WeakTexture = base::WeakPtr<MockAbstractTexture>;

  WeakTexture CreateAndAddTexture() {
    std::unique_ptr<MockAbstractTexture> texture =
        std::make_unique<MockAbstractTexture>();
    WeakTexture texture_weak = texture->AsWeakPtr();

    texture_pool_->AddTexture(std::move(texture));

    return texture_weak;
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  gpu::SyncToken sync_token_;

  scoped_refptr<FakeCommandBufferHelper> helper_;
  scoped_refptr<TexturePool> texture_pool_;
};

TEST_F(TexturePoolTest, AddAndReleaseTexturesWithContext) {
  // Test that adding then deleting a texture destroys it.
  WeakTexture texture = CreateAndAddTexture();
  bool release_flag = false;
  texture_pool_->ReleaseTexture(
      texture.get(), sync_token_,
      base::BindOnce([](bool* flag) { *flag = true; }, &release_flag));

  // The texture should still exist until the sync token is cleared.
  EXPECT_TRUE(texture);

  // Once the sync token is released, then the context should be made current
  // and the texture should be destroyed.
  helper_->ReleaseSyncToken(sync_token_);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(texture);
  // The release cb should have been called.
  EXPECT_TRUE(release_flag);
}

TEST_F(TexturePoolTest, AddAndReleaseTexturesWithoutContext) {
  // Test that adding then deleting a texture destroys it, even if the context
  // was lost.
  WeakTexture texture = CreateAndAddTexture();
  helper_->ContextLost();
  texture_pool_->ReleaseTexture(texture.get(), sync_token_);
  ASSERT_TRUE(texture);

  helper_->ReleaseSyncToken(sync_token_);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(texture);
}

TEST_F(TexturePoolTest, NonEmptyPoolAfterStubDestructionDoesntCrash) {
  // Make sure that we can delete the stub, and verify that pool teardown still
  // works (doesn't crash) even though the pool is not empty.
  CreateAndAddTexture();

  helper_->StubLost();
}

TEST_F(TexturePoolTest,
       NonEmptyPoolAfterStubWithoutContextDestructionDoesntCrash) {
  // Make sure that we can delete the stub, and verify that pool teardown still
  // works (doesn't crash) even though the pool is not empty.
  CreateAndAddTexture();

  helper_->ContextLost();
  helper_->StubLost();
}

TEST_F(TexturePoolTest, TexturePoolRetainsReferenceWhileWaiting) {
  // Dropping our reference to |texture_pool_| while it's waiting for a sync
  // token shouldn't prevent the wait from completing.
  WeakTexture texture = CreateAndAddTexture();
  texture_pool_->ReleaseTexture(texture.get(), sync_token_);

  // The texture should still exist until the sync token is cleared.
  ASSERT_TRUE(texture);

  // Drop the texture pool while it's waiting.  Nothing should happen.
  texture_pool_ = nullptr;
  ASSERT_TRUE(texture);

  // The texture should be destroyed after the sync token completes.
  helper_->ReleaseSyncToken(sync_token_);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(texture);
}

TEST_F(TexturePoolTest, TexturePoolReleasesImmediatelyWithoutSyncToken) {
  // If we don't provide a sync token, then it should release the texture.
  WeakTexture texture = CreateAndAddTexture();
  texture_pool_->ReleaseTexture(texture.get(), gpu::SyncToken());
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(texture);
}

}  // namespace media
