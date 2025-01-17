// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These are functions to access various profile-management flags but with
// possible overrides from Experiements.  This is done inside chrome/common
// because it is accessed by files through the chrome/ directory tree.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_PROFILE_MANAGEMENT_SWITCHES_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_PROFILE_MANAGEMENT_SWITCHES_H_

#include "base/feature_list.h"

namespace signin {

// Improved and unified consent for privacy-related features.
extern const base::Feature kUnifiedConsent;
extern const char kUnifiedConsentShowBumpParameter[];

// State of the "Unified Consent" feature.
enum class UnifiedConsentFeatureState {
  // Unified consent is disabled.
  kDisabled,
  // Unified consent is enabled, but the bump is not shown.
  kEnabledNoBump,
  // Unified consent is enabled and the bump is shown.
  kEnabledWithBump
};

// TODO(https://crbug.com/777774): Cleanup this enum and remove related
// functions once Dice is fully rolled out, and/or Mirror code is removed on
// desktop.
enum class AccountConsistencyMethod : int {
  // No account consistency.
  kDisabled,

  // Account management UI in the avatar bubble.
  kMirror,

  // No account consistency, but Dice fixes authentication errors.
  kDiceFixAuthErrors,

  // Chrome uses the Dice signin flow and silently collects tokens associated
  // with Gaia cookies to prepare for the migration. Uses the Chrome sync Gaia
  // endpoint to enable sync.
  kDicePrepareMigration,

  // Account management UI on Gaia webpages is enabled once the accounts become
  // consistent.
  kDiceMigration,

  // Account management UI on Gaia webpages is enabled. If accounts are not
  // consistent when this is enabled, the account reconcilor enforces the
  // consistency.
  kDice
};

// Returns true if the |a| comes after |b| in the AccountConsistencyMethod enum.
// Should not be used for Mirror.
bool DiceMethodGreaterOrEqual(AccountConsistencyMethod a,
                              AccountConsistencyMethod b);

////////////////////////////////////////////////////////////////////////////////
// Other functions:

// Whether the chrome.identity API should be multi-account.
bool IsExtensionsMultiAccount();

// Returns the state of the "Unified Consent" feature.
UnifiedConsentFeatureState GetUnifiedConsentFeatureState();

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_PROFILE_MANAGEMENT_SWITCHES_H_
