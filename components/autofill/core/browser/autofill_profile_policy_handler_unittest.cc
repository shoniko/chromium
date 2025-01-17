// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_profile_policy_handler.h"

#include <memory>

#include "base/values.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

// Test cases for the Autofill profiles policy setting.
class AutofillProfilePolicyHandlerTest : public testing::Test {};

TEST_F(AutofillProfilePolicyHandlerTest, Default) {
  policy::PolicyMap policy;
  PrefValueMap prefs;
  AutofillProfilePolicyHandler handler;
  handler.ApplyPolicySettings(policy, &prefs);
  EXPECT_FALSE(
      prefs.GetValue(autofill::prefs::kAutofillProfileEnabled, nullptr));
}

TEST_F(AutofillProfilePolicyHandlerTest, Enabled) {
  policy::PolicyMap policy;
  policy.Set(policy::key::kAutofillProfileEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(true),
             nullptr);
  PrefValueMap prefs;
  AutofillProfilePolicyHandler handler;
  handler.ApplyPolicySettings(policy, &prefs);

  // Enabling Autofill for profiles should not set the prefs.
  EXPECT_FALSE(
      prefs.GetValue(autofill::prefs::kAutofillProfileEnabled, nullptr));
}

TEST_F(AutofillProfilePolicyHandlerTest, Disabled) {
  policy::PolicyMap policy;
  policy.Set(policy::key::kAutofillProfileEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(false),
             nullptr);
  PrefValueMap prefs;
  AutofillProfilePolicyHandler handler;
  handler.ApplyPolicySettings(policy, &prefs);

  // Disabling Autofill for profiles should switch the prefs to managed.
  const base::Value* value = nullptr;
  EXPECT_TRUE(prefs.GetValue(autofill::prefs::kAutofillProfileEnabled, &value));
  ASSERT_TRUE(value);
  bool autofill_profile_enabled = true;
  bool result = value->GetAsBoolean(&autofill_profile_enabled);
  ASSERT_TRUE(result);
  EXPECT_FALSE(autofill_profile_enabled);
}

}  // namespace autofill
