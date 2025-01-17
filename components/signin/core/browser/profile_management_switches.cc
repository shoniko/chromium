// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/profile_management_switches.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "components/signin/core/browser/signin_switches.h"

namespace signin {

namespace {

bool AccountConsistencyMethodGreaterOrEqual(AccountConsistencyMethod a,
                                            AccountConsistencyMethod b) {
  return static_cast<int>(a) >= static_cast<int>(b);
}

}  // namespace

// base::Feature definitions.

const base::Feature kUnifiedConsent{"UnifiedConsent",
                                    base::FEATURE_DISABLED_BY_DEFAULT};
const char kUnifiedConsentShowBumpParameter[] = "show_consent_bump";

bool DiceMethodGreaterOrEqual(AccountConsistencyMethod a,
                              AccountConsistencyMethod b) {
  DCHECK_NE(AccountConsistencyMethod::kMirror, a);
  DCHECK_NE(AccountConsistencyMethod::kMirror, b);
  return AccountConsistencyMethodGreaterOrEqual(a, b);
}

bool IsExtensionsMultiAccount() {
#if defined(OS_ANDROID) || defined(OS_IOS)
  NOTREACHED() << "Extensions are not enabled on Android or iOS";
  // Account consistency is enabled on Android and iOS.
  return false;
#endif

  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kExtensionsMultiAccount);
}

UnifiedConsentFeatureState GetUnifiedConsentFeatureState() {
  if (!base::FeatureList::IsEnabled(signin::kUnifiedConsent))
    return UnifiedConsentFeatureState::kDisabled;

  std::string show_bump = base::GetFieldTrialParamValueByFeature(
      kUnifiedConsent, kUnifiedConsentShowBumpParameter);
  return show_bump.empty() ? UnifiedConsentFeatureState::kEnabledNoBump
                           : UnifiedConsentFeatureState::kEnabledWithBump;
}

}  // namespace signin
