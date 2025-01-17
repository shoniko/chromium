// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/ax_tree_source_mus.h"

#include <vector>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace {

class AXTreeSourceMusTest : public ViewsTestBase {
 public:
  AXTreeSourceMusTest() = default;
  ~AXTreeSourceMusTest() override = default;

  // testing::Test:
  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_ = std::make_unique<Widget>();
    Widget::InitParams params(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(11, 22, 333, 444);
    params.context = GetContext();
    widget_->Init(params);
    widget_->SetContentsView(new View());
    label_ = new Label(base::ASCIIToUTF16("Label"));
    label_->SetBounds(1, 1, 111, 111);
    widget_->GetContentsView()->AddChildView(label_);
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  std::unique_ptr<Widget> widget_;
  Label* label_ = nullptr;  // Owned by views hierarchy.

 private:
  DISALLOW_COPY_AND_ASSIGN(AXTreeSourceMusTest);
};

TEST_F(AXTreeSourceMusTest, Serialize) {
  AXAuraObjCache* cache = AXAuraObjCache::GetInstance();
  AXAuraObjWrapper* root = cache->GetOrCreate(widget_->GetContentsView());

  AXTreeSourceMus tree(root);
  EXPECT_EQ(root, tree.GetRoot());

  // Serialize the root.
  ui::AXNodeData node_data;
  tree.SerializeNode(root, &node_data);

  // Root is at the origin and has no parent container.
  EXPECT_EQ(gfx::RectF(0, 0, 333, 444), node_data.location);
  EXPECT_EQ(-1, node_data.offset_container_id);

  // Serialize a child.
  tree.SerializeNode(cache->GetOrCreate(label_), &node_data);

  // Child has relative position with the root as the container.
  EXPECT_EQ(gfx::RectF(1, 1, 111, 111), node_data.location);
  EXPECT_EQ(root->GetUniqueId().Get(), node_data.offset_container_id);
}

}  // namespace
}  // namespace views
