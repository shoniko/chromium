// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/module_blacklist_cache_updater_win.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/sha1.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "chrome/browser/conflicts/module_blacklist_cache_util_win.h"
#include "chrome/browser/conflicts/module_database_win.h"
#include "chrome/browser/conflicts/module_info_util_win.h"
#include "chrome/browser/conflicts/module_list_filter_win.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/install_static/install_util.h"

#if !defined(OFFICIAL_BUILD)
#include "base/base_paths.h"
#endif

namespace {

// The maximum number of modules allowed in the cache. This keeps the cache
// from growing indefinitely.
// Note: This value is tied to the "ModuleBlacklistCache.ModuleCount" histogram.
//       Rename the histogram if this value is ever changed.
static constexpr size_t kMaxModuleCount = 5000u;

// The maximum amount of time a stale entry is kept in the cache before it is
// deleted.
static constexpr base::TimeDelta kMaxEntryAge = base::TimeDelta::FromDays(180);

// This enum is used for UMA. Therefore, the values should never change.
enum class BlacklistStatus {
  // A module was marked as blacklisted during the current browser execution.
  kNewlyBlacklisted = 0,
  // A module was blocked when it tried to load into the process.
  kBlocked = 1,
  kMaxValue = kBlocked,
};

// Updates the module blacklist cache asynchronously on a background sequence
// and return a CacheUpdateResult value.
ModuleBlacklistCacheUpdater::CacheUpdateResult UpdateModuleBlacklistCache(
    const base::FilePath& module_blacklist_cache_path,
    scoped_refptr<ModuleListFilter> module_list_filter,
    const std::vector<third_party_dlls::PackedListModule>&
        newly_blacklisted_modules,
    const std::vector<third_party_dlls::PackedListModule>& blocked_modules,
    size_t max_module_count,
    uint32_t min_time_date_stamp) {
  DCHECK(module_list_filter);

  // Emit some UMA metrics about the update.
  for (size_t i = 0; i < newly_blacklisted_modules.size(); ++i) {
    UMA_HISTOGRAM_ENUMERATION("ModuleBlacklistCache.BlacklistStatus",
                              BlacklistStatus::kNewlyBlacklisted);
  }
  for (size_t i = 0; i < blocked_modules.size(); ++i) {
    UMA_HISTOGRAM_ENUMERATION("ModuleBlacklistCache.BlacklistStatus",
                              BlacklistStatus::kBlocked);
  }

  ModuleBlacklistCacheUpdater::CacheUpdateResult result;

  // Read the existing cache.
  third_party_dlls::PackedListMetadata metadata;
  std::vector<third_party_dlls::PackedListModule> blacklisted_modules;
  ReadResult read_result =
      ReadModuleBlacklistCache(module_blacklist_cache_path, &metadata,
                               &blacklisted_modules, &result.old_md5_digest);
  UMA_HISTOGRAM_ENUMERATION("ModuleBlacklistCache.ReadResult", read_result);

  // Update the existing data with |newly_blacklisted_modules| and
  // |blocked_modules|.
  UpdateModuleBlacklistCacheData(
      *module_list_filter, newly_blacklisted_modules, blocked_modules,
      max_module_count, min_time_date_stamp, &metadata, &blacklisted_modules);
  // Note: This histogram is tied to the current value of kMaxModuleCount.
  //       Rename the histogram if that value is ever changed.
  UMA_HISTOGRAM_CUSTOM_COUNTS("ModuleBlacklistCache.ModuleCount",
                              blacklisted_modules.size(), 1, kMaxModuleCount,
                              50);

  // Then write the updated cache to disk.
  bool write_result =
      WriteModuleBlacklistCache(module_blacklist_cache_path, metadata,
                                blacklisted_modules, &result.new_md5_digest);
  UMA_HISTOGRAM_BOOLEAN("ModuleBlacklistCache.WriteResult", write_result);

  if (write_result) {
    // Write the path of the cache into the registry so that chrome_elf can find
    // it on its own.
    base::string16 cache_path_registry_key =
        install_static::GetRegistryPath().append(
            third_party_dlls::kThirdPartyRegKeyName);
    base::win::RegKey registry_key(
        HKEY_CURRENT_USER, cache_path_registry_key.c_str(), KEY_SET_VALUE);

    bool cache_path_updated = SUCCEEDED(
        registry_key.WriteValue(third_party_dlls::kBlFilePathRegValue,
                                module_blacklist_cache_path.value().c_str()));
    UMA_HISTOGRAM_BOOLEAN("ModuleBlacklistCache.BlacklistPathUpdated",
                          cache_path_updated);
  }

  return result;
}

}  // namespace

// static
constexpr base::TimeDelta ModuleBlacklistCacheUpdater::kUpdateTimerDuration;

ModuleBlacklistCacheUpdater::ModuleBlacklistCacheUpdater(
    ModuleDatabaseEventSource* module_database_event_source,
    const CertificateInfo& exe_certificate_info,
    scoped_refptr<ModuleListFilter> module_list_filter,
    OnCacheUpdatedCallback on_cache_updated_callback)
    : module_database_event_source_(module_database_event_source),
      exe_certificate_info_(exe_certificate_info),
      module_list_filter_(std::move(module_list_filter)),
      on_cache_updated_callback_(std::move(on_cache_updated_callback)),
      background_sequence_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BACKGROUND,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      // The use of base::Unretained() is safe here because the callback can
      // only be invoked while |module_load_attempt_log_listener_| is alive.
      module_load_attempt_log_listener_(
          base::BindRepeating(&ModuleBlacklistCacheUpdater::OnNewModulesBlocked,
                              base::Unretained(this))),
      weak_ptr_factory_(this) {
  DCHECK(module_list_filter_);
  module_database_event_source_->AddObserver(this);
}

ModuleBlacklistCacheUpdater::~ModuleBlacklistCacheUpdater() {
  module_database_event_source_->RemoveObserver(this);
}

// static
bool ModuleBlacklistCacheUpdater::IsThirdPartyModuleBlockingEnabled() {
  // The ThirdPartyConflictsManager can exist even if the blocking is disabled
  // because that class also controls the warning of incompatible applications.
  return ModuleDatabase::GetInstance() &&
         ModuleDatabase::GetInstance()->third_party_conflicts_manager() &&
         base::FeatureList::IsEnabled(features::kThirdPartyModulesBlocking);
}

// static
base::FilePath ModuleBlacklistCacheUpdater::GetModuleBlacklistCachePath() {
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
    return base::FilePath();

  return user_data_dir.Append(kModuleListComponentRelativePath)
      .Append(L"bldata");
}

// static
void ModuleBlacklistCacheUpdater::DeleteModuleBlacklistCache() {
  bool delete_result =
      base::DeleteFile(GetModuleBlacklistCachePath(), false /* recursive */);
  UMA_HISTOGRAM_BOOLEAN("ModuleBlacklistCache.DeleteResult", delete_result);
}

void ModuleBlacklistCacheUpdater::OnNewModuleFound(
    const ModuleInfoKey& module_key,
    const ModuleInfoData& module_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Only consider loaded modules that are not IMEs. Shell extensions are still
  // blocked.
  static constexpr uint32_t kModulePropertiesBitmask =
      ModuleInfoData::kPropertyLoadedModule | ModuleInfoData::kPropertyIme;
  if ((module_data.module_properties & kModulePropertiesBitmask) !=
      ModuleInfoData::kPropertyLoadedModule) {
    return;
  }

  // Explicitly whitelist modules whose signing cert's Subject field matches the
  // one in the current executable. No attempt is made to check the validity of
  // module signatures or of signing certs.
  if (exe_certificate_info_.type != CertificateType::NO_CERTIFICATE &&
      exe_certificate_info_.subject ==
          module_data.inspection_result->certificate_info.subject) {
    return;
  }

  // Never block a module seemingly signed by Microsoft. Again, no attempt is
  // made to check the validity of the certificate.
  if (IsMicrosoftModule(
          module_data.inspection_result->certificate_info.subject)) {
    return;
  }

// For developer builds only, whitelist modules in the same directory as the
// executable.
#if !defined(OFFICIAL_BUILD)
  base::FilePath exe_path;
  if (base::PathService::Get(base::DIR_EXE, &exe_path) &&
      exe_path.DirName().IsParent(module_key.module_path)) {
    return;
  }
#endif

  // Skip modules whitelisted by the Module List component.
  if (module_list_filter_->IsWhitelisted(module_key, module_data))
    return;

  // Some blacklisted modules are allowed to load.
  std::unique_ptr<chrome::conflicts::BlacklistAction> blacklist_action =
      module_list_filter_->IsBlacklisted(module_key, module_data);
  if (blacklist_action && blacklist_action->allow_load())
    return;

  // Insert the blacklisted module.
  newly_blacklisted_modules_.emplace_back();
  third_party_dlls::PackedListModule& module =
      newly_blacklisted_modules_.back();

  // Hash the basename.
  const std::string module_basename = base::UTF16ToUTF8(
      base::i18n::ToLower(module_key.module_path.BaseName().value()));
  base::SHA1HashBytes(reinterpret_cast<const uint8_t*>(module_basename.data()),
                      module_basename.length(), module.basename_hash);

  // Hash the code id.
  const std::string module_code_id = GenerateCodeId(module_key);
  base::SHA1HashBytes(reinterpret_cast<const uint8_t*>(module_code_id.data()),
                      module_code_id.length(), module.code_id_hash);

  module.time_date_stamp = CalculateTimeDateStamp(base::Time::Now());

  // Signal the module database that this module will be added to the cache.
  // Note that observers that care about this information should register to
  // the Module Database's observer interface after the ModuleBlacklistCache
  // instance.
  // The Module Database can be null during tests.
  auto* module_database = ModuleDatabase::GetInstance();
  if (module_database) {
    module_database->OnModuleAddedToBlacklist(
        module_key.module_path, module_key.module_size,
        module_key.module_time_date_stamp);
  }
}

void ModuleBlacklistCacheUpdater::OnModuleDatabaseIdle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  StartModuleBlacklistCacheUpdate();
}

void ModuleBlacklistCacheUpdater::OnNewModulesBlocked(
    std::vector<third_party_dlls::PackedListModule>&& blocked_modules) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  blocked_modules_.insert(blocked_modules_.begin(),
                          std::make_move_iterator(blocked_modules.begin()),
                          std::make_move_iterator(blocked_modules.end()));

  // Start the timer.
  timer_.Start(FROM_HERE,
               kUpdateTimerDuration,
               base::Bind(&ModuleBlacklistCacheUpdater::OnTimerExpired,
                          base::Unretained(this)));
}

void ModuleBlacklistCacheUpdater::OnTimerExpired() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  StartModuleBlacklistCacheUpdate();
}

void ModuleBlacklistCacheUpdater::StartModuleBlacklistCacheUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  timer_.Stop();

  base::FilePath cache_file_path = GetModuleBlacklistCachePath();
  if (cache_file_path.empty())
    return;

  // Calculate the minimum time date stamp.
  uint32_t min_time_date_stamp =
      CalculateTimeDateStamp(base::Time::Now() - kMaxEntryAge);

  // Update the module blacklist cache on a background sequence.
  base::PostTaskAndReplyWithResult(
      background_sequence_.get(), FROM_HERE,
      base::BindOnce(&UpdateModuleBlacklistCache, cache_file_path,
                     module_list_filter_, std::move(newly_blacklisted_modules_),
                     std::move(blocked_modules_), kMaxModuleCount,
                     min_time_date_stamp),
      base::BindOnce(
          &ModuleBlacklistCacheUpdater::OnModuleBlacklistCacheUpdated,
          weak_ptr_factory_.GetWeakPtr()));
}

void ModuleBlacklistCacheUpdater::OnModuleBlacklistCacheUpdated(
    const CacheUpdateResult& result) {
  on_cache_updated_callback_.Run(result);
}
