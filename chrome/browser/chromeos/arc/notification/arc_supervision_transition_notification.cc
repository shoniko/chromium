// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/notification/arc_supervision_transition_notification.h"

#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "base/bind.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/account_id/account_id.h"
#include "components/arc/arc_prefs.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "url/gurl.h"

namespace arc {

namespace {

// Id of the notifier.
constexpr char kNotifierId[] = "arc_supervision_transition";

// Observes following ARC++ events that dismisses notification.
//   * ARC++ opted out.
//   * supervision transition completed.
// If one of these events happens notification is automatically dismissed.
class NotificationDelegate : public message_center::NotificationDelegate,
                             public ArcSessionManager::Observer {
 public:
  explicit NotificationDelegate(Profile* profile) : profile_(profile) {
    ArcSessionManager::Get()->AddObserver(this);
    pref_change_registrar_.Init(profile_->GetPrefs());
    pref_change_registrar_.Add(
        prefs::kArcSupervisionTransition,
        base::BindRepeating(&NotificationDelegate::OnTransitionChanged,
                            base::Unretained(this)));
  }

  // ArcSessionManager::Observer:
  void OnArcPlayStoreEnabledChanged(bool enabled) override {
    // ARC++ Play Store can be only opted out in case notifcation is shown.
    DCHECK(!enabled);
    Dismiss();
  }

 private:
  ~NotificationDelegate() override {
    ArcSessionManager::Get()->RemoveObserver(this);
  }

  // Dismisses currently active notification.
  void Dismiss() {
    NotificationDisplayService::GetForProfile(profile_)->Close(
        NotificationHandler::Type::TRANSIENT,
        kSupervisionTransitionNotificationId);
  }

  // Called in case transition state is changed.
  void OnTransitionChanged() {
    DCHECK_EQ(ArcSupervisionTransition::NO_TRANSITION,
              GetSupervisionTransition(profile_));
    Dismiss();
  }

  // Not owned.
  Profile* const profile_;

  // Registrar used to monitor ARC enabled state.
  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(NotificationDelegate);
};

}  // namespace

const char kSupervisionTransitionNotificationId[] =
    "arc_supervision_transition/notification";

void ShowSupervisionTransitionNotification(Profile* profile) {
  const ArcSupervisionTransition transition = GetSupervisionTransition(profile);
  int message_id = 0;
  int title_id = 0;
  switch (transition) {
    case ArcSupervisionTransition::CHILD_TO_REGULAR:
      title_id = IDS_ARC_CHILD_TRANSITION_TO_REGULAR_TITLE;
      message_id = IDS_ARC_CHILD_TRANSITION_TO_REGULAR_MESSAGE;
      break;
    case ArcSupervisionTransition::REGULAR_TO_CHILD:
      title_id = IDS_ARC_CHILD_TRANSITION_FROM_REGULAR_TITLE;
      message_id = IDS_ARC_CHILD_TRANSITION_FROM_REGULAR_MESSAGE;
      break;
    default:
      NOTREACHED() << "Notification requested with no transition in progress";
      return;
  }
  message_center::NotifierId notifier_id(
      message_center::NotifierId::SYSTEM_COMPONENT, kNotifierId);
  notifier_id.profile_id =
      multi_user_util::GetAccountIdFromProfile(profile).GetUserEmail();

  std::unique_ptr<message_center::Notification> notification =
      message_center::Notification::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          kSupervisionTransitionNotificationId,
          l10n_util::GetStringUTF16(title_id),
          l10n_util::GetStringUTF16(message_id), gfx::Image(),
          l10n_util::GetStringUTF16(IDS_ARC_NOTIFICATION_DISPLAY_SOURCE),
          GURL(), notifier_id, message_center::RichNotificationData(),
          new NotificationDelegate(profile), ash::kNotificationFamilyLinkIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);
  NotificationDisplayService::GetForProfile(profile)->Display(
      NotificationHandler::Type::TRANSIENT, *notification);
}

}  // namespace arc
