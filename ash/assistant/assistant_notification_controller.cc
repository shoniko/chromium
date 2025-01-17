// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_notification_controller.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/new_window_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr char kNotificationId[] = "assistant";
constexpr char kNotifierAssistant[] = "assistant";

// Delegate for an assistant notification.
class AssistantNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  AssistantNotificationDelegate(
      base::WeakPtr<AssistantNotificationController> notification_controller,
      base::WeakPtr<AssistantController> assistant_controller,
      chromeos::assistant::mojom::AssistantNotificationPtr notification)
      : notification_controller_(std::move(notification_controller)),
        assistant_controller_(std::move(assistant_controller)),
        notification_(std::move(notification)) {
    DCHECK(notification_);
  }

  // message_center::NotificationDelegate:
  void Close(bool by_user) override {
    // If |by_user| is true, means that this close action is initiated by user,
    // need to dismiss this notification at server to notify other devices.
    // If |by_user| is false, means that this close action is initiated from the
    // server, so do not need to dismiss this notification again.
    if (by_user && notification_controller_)
      notification_controller_->DismissNotification(notification_.Clone());
  }

  void Click(const base::Optional<int>& button_index,
             const base::Optional<base::string16>& reply) override {
    // Open the action url if it is valid.
    if (notification_->action_url.is_valid() && assistant_controller_)
      assistant_controller_->OpenUrl(notification_->action_url);

    // TODO(wutao): retrieve the notification.
  }

 private:
  // Refcounted.
  ~AssistantNotificationDelegate() override = default;

  base::WeakPtr<AssistantNotificationController> notification_controller_;

  base::WeakPtr<AssistantController> assistant_controller_;

  chromeos::assistant::mojom::AssistantNotificationPtr notification_;

  DISALLOW_COPY_AND_ASSIGN(AssistantNotificationDelegate);
};

}  // namespace

AssistantNotificationController::AssistantNotificationController(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller),
      assistant_notification_subscriber_binding_(this),
      weak_factory_(this) {}

AssistantNotificationController::~AssistantNotificationController() = default;

void AssistantNotificationController::SetAssistant(
    chromeos::assistant::mojom::Assistant* assistant) {
  assistant_ = assistant;

  // Subscribe to Assistant notification events.
  chromeos::assistant::mojom::AssistantNotificationSubscriberPtr ptr;
  assistant_notification_subscriber_binding_.Bind(mojo::MakeRequest(&ptr));
  assistant_->AddAssistantNotificationSubscriber(std::move(ptr));
}

void AssistantNotificationController::DismissNotification(
    AssistantNotificationPtr notification) {
  assistant_->DismissNotification(std::move(notification));
}

void AssistantNotificationController::OnShowNotification(
    AssistantNotificationPtr notification) {
  DCHECK(assistant_);

  // Create the specified |notification| that should be rendered in the
  // |message_center| for the interaction.
  notification_ = std::move(notification);
  const base::string16 title = base::UTF8ToUTF16(notification_->title);
  const base::string16 message = base::UTF8ToUTF16(notification_->message);
  const base::string16 display_source =
      l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_NOTIFICATION_DISPLAY_SOURCE);

  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  message_center::RichNotificationData optional_field;

  std::unique_ptr<message_center::Notification> system_notification =
      message_center::Notification::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId, title,
          message, gfx::Image(), display_source, GURL(),
          message_center::NotifierId(
              message_center::NotifierId::SYSTEM_COMPONENT, kNotifierAssistant),
          optional_field,
          new AssistantNotificationDelegate(weak_factory_.GetWeakPtr(),
                                            assistant_controller_->GetWeakPtr(),
                                            notification_.Clone()),
          kAssistantIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);
  system_notification->set_priority(message_center::DEFAULT_PRIORITY);
  message_center->AddNotification(std::move(system_notification));
}

void AssistantNotificationController::OnRemoveNotification(
    const std::string& grouping_key) {
  if (!grouping_key.empty() &&
      (!notification_ || notification_->grouping_key != grouping_key)) {
    return;
  }

  // message_center has only one Assistant notification, so there is no
  // difference between removing all and removing one Assistant notification.
  notification_.reset();
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  message_center->RemoveNotification(kNotificationId, /*by_user=*/false);
}

}  // namespace ash
