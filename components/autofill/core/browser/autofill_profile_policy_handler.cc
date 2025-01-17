// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_profile_policy_handler.h"

#include "base/values.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace autofill {

AutofillProfilePolicyHandler::AutofillProfilePolicyHandler()
    : TypeCheckingPolicyHandler(policy::key::kAutofillProfileEnabled,
                                base::Value::Type::BOOLEAN) {}

AutofillProfilePolicyHandler::~AutofillProfilePolicyHandler() {}

void AutofillProfilePolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value = policies.GetValue(policy_name());
  bool autofill_profile_enabled;
  if (value && value->GetAsBoolean(&autofill_profile_enabled) &&
      !autofill_profile_enabled) {
    prefs->SetBoolean(autofill::prefs::kAutofillProfileEnabled, false);
  }
}

}  // namespace autofill
