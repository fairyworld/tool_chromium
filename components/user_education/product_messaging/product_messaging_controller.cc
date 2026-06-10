// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/product_messaging/product_messaging_controller.h"

#include <algorithm>
#include <compare>
#include <sstream>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "components/user_education/common/session/user_education_session_manager.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "components/user_education/product_messaging/product_messaging_policy.h"

namespace user_education {

// ProductMessagingHandleImpl

ProductMessagingHandleImpl::ProductMessagingHandleImpl(
    ProductMessageKey message_key,
    base::WeakPtr<ProductMessagingController> controller)
    : message_key_(message_key), controller_(controller) {
  CHECK(message_key_);
  CHECK(controller_);
}

ProductMessagingHandleImpl::~ProductMessagingHandleImpl() {
  controller_->ReleaseHandle(message_key_, shown_);
  controller_.reset();
  superseded_subscription_ = {};
  superseded_callback_.Reset();
  message_key_ = ProductMessageKey();
}

void ProductMessagingHandleImpl::SetShown() {
  CHECK(!shown_);
  shown_ = true;
  controller_->OnMessageShown(message_key_);
}

void ProductMessagingHandleImpl::SetSupersededCallback(
    ProductMessageStatusCallback callback) {
  if (callback.is_null()) {
    superseded_subscription_ = {};
    superseded_callback_.Reset();
    return;
  }
  CHECK(!superseded_subscription_);
  CHECK(!superseded_callback_);

  superseded_callback_ = std::move(callback);
  superseded_subscription_ =
      controller_->status_update_callbacks_.Add(base::BindRepeating(
          &ProductMessagingHandleImpl::OnStatusChange, base::Unretained(this)));
}

void ProductMessagingHandleImpl::OnStatusChange(ProductMessageKey key,
                                                ProductMessageStatus status) {
  CHECK(superseded_callback_);
  CHECK(message_key_);
  if (key.type() <= message_key_.type()) {
    return;
  }
  if (status != ProductMessageStatus::kEligible &&
      status != ProductMessageStatus::kShowing) {
    return;
  }
  superseded_callback_.Run(key, status);
}

// ProductMessagingController

ProductMessagingController::ProductMessagingController() = default;
ProductMessagingController::~ProductMessagingController() = default;

void ProductMessagingController::Init(
    UserEducationSessionProvider& session_provider,
    UserEducationStorageService& storage_service,
    std::unique_ptr<ProductMessagingPolicy> policy) {
  CHECK(!storage_service_);
  CHECK(!policy_);
  CHECK(policy);
  storage_service_ = &storage_service;
  if (session_provider.GetNewSessionSinceStartup()) {
    storage_service_->ResetProductMessagingData();
  }
  session_subscription_ =
      session_provider.AddNewSessionCallback(base::BindRepeating(
          &ProductMessagingController::OnNewSession, base::Unretained(this)));
  policy_ = std::move(policy);
}

bool ProductMessagingController::IsMessageQueued(
    ProductMessageKey message_key) const {
  return pending_messages_.contains(message_key);
}

void ProductMessagingController::QueueMessage(
    ProductMessageKey message_key,
    ProductMessageReadyCallback ready_to_start_callback) {
  CHECK(message_key);
  CHECK(!ready_to_start_callback.is_null());

  // Cannot re-queue a notice.
  if (GetProductMessageStatus(message_key) != ProductMessageStatus::kNone) {
    return;
  }

  const auto result = pending_messages_.emplace(
      message_key, std::move(ready_to_start_callback));
  CHECK(result.second) << "Duplicate message ID: " << message_key.ToString();
  status_update_callbacks_.Notify(message_key, ProductMessageStatus::kQueued);
  MaybeShowNextMessage();
}

void ProductMessagingController::UnqueueMessage(ProductMessageKey message_key) {
  pending_messages_.erase(message_key);
}

// Returns the status of `message`.
ProductMessageStatus ProductMessagingController::GetProductMessageStatus(
    ProductMessageKey message) const {
  if (!message) {
    return ProductMessageStatus::kNone;
  }
  if (message == current_message_) {
    return current_message_shown_ ? ProductMessageStatus::kShowing
                                  : ProductMessageStatus::kEligible;
  }
  if (IsMessageQueued(message)) {
    return ProductMessageStatus::kQueued;
  }
  return ProductMessageStatus::kNone;
}

// Returns queued or showing messages. Can be filtered by priority and by
// status.
base::flat_map<ProductMessageKey, ProductMessageStatus>
ProductMessagingController::GetAllMessages(
    std::initializer_list<ProductMessageStatus> statuses_to_retrieve,
    ProductMessageType priority_higher_than) const {
  const base::flat_set<ProductMessageStatus> filter_statuses(
      statuses_to_retrieve);
  base::flat_map<ProductMessageKey, ProductMessageStatus> infos;
  if (current_message_ && current_message_.type() > priority_higher_than) {
    if (current_message_shown_ &&
        filter_statuses.contains(ProductMessageStatus::kShowing)) {
      infos.emplace(current_message_, ProductMessageStatus::kShowing);
    }
    if (!current_message_shown_ &&
        filter_statuses.contains(ProductMessageStatus::kEligible)) {
      infos.emplace(current_message_, ProductMessageStatus::kEligible);
    }
  }
  if (filter_statuses.contains(ProductMessageStatus::kQueued)) {
    for (auto& [key, data] : pending_messages_) {
      if (key.type() > priority_higher_than) {
        infos.emplace(key, ProductMessageStatus::kQueued);
      }
    }
  }
  return infos;
}

base::CallbackListSubscription
ProductMessagingController::AddStatusUpdateCallbackForTesting(
    ProductMessageStatusCallback callback) {
  return status_update_callbacks_.Add(std::move(callback));
}

bool ProductMessagingController::HasPendingMessagesForTesting() const {
  return current_message_ || !pending_messages_.empty();
}

void ProductMessagingController::ReleaseHandle(ProductMessageKey message_key,
                                               bool message_shown) {
  CHECK_EQ(current_message_, message_key);
  if (message_shown) {
    ProductMessagingData data = storage_service_->ReadProductMessagingData();
    const auto insert_result = data.shown_notices.insert(message_key.GetName());
    if (insert_result.second) {
      storage_service_->SaveProductMessagingData(data);
    }
  }
  current_message_ = ProductMessageKey();
  current_message_shown_ = false;
  MaybeShowNextMessage();
}

void ProductMessagingController::MaybeShowNextMessage() {
  if (!ready_to_show()) {
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProductMessagingController::MaybeShowNextMessageImpl,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ProductMessagingController::PurgeBlockedMessages() {
  ProductMessagingPolicy::Ids ids;
  for (const auto& name :
       storage_service_->ReadProductMessagingData().shown_notices) {
    std::string actual_name = name;
    actual_name += internal::kProductMessageUniqueIdSuffix;
    const auto id =
        internal::ProductMessageUniqueId::FromName(actual_name.c_str());
    ids.insert(id);
  }
  std::vector<ProductMessageKey> to_purge;
  for (const auto& [key, data] : pending_messages_) {
    if (policy_->BlockedByAnyOf(key, ids, /*include_self=*/true)) {
      to_purge.push_back(key);
      continue;
    }
  }
  for (auto id : to_purge) {
    pending_messages_.erase(id);
  }
}

void ProductMessagingController::MaybeShowNextMessageImpl() {
  if (!ready_to_show()) {
    return;
  }

  PurgeBlockedMessages();
  if (pending_messages_.empty()) {
    return;
  }

  ProductMessagingPolicy::Ids ids;
  std::set<ProductMessageType> types;
  for (const auto& [key, data] : pending_messages_) {
    ids.insert(key.id());
    types.insert(key.type());
  }

  // Find a notice that is not waiting for any other notices to show.
  ProductMessageKey to_show;
  for (const auto& [key, data] : pending_messages_) {
    if (policy_->BlockedByAnyOf(key, ids, /*include_self=*/false) ||
        policy_->MustShowAfterAnyOf(key, ids)) {
      continue;
    }
    bool allowed = true;
    for (ProductMessageType type : types) {
      if (policy_->GetRelationship(key.type(), type) ==
          ProductMessagingPolicy::TypeRelationship::kSupersededBy) {
        allowed = false;
        break;
      }
    }
    if (allowed) {
      to_show = key;
      break;
    }
  }

  if (!to_show) {
    NOTREACHED() << "Circular dependency in required notifications:"
                 << DumpData();
  }

  // Fire the next notice.
  ProductMessageReadyCallback cb = std::move(pending_messages_[to_show]);
  pending_messages_.erase(to_show);
  current_message_ = to_show;
  current_message_shown_ = false;
  std::move(cb).Run(base::WrapUnique(
      new ProductMessagingHandleImpl(to_show, weak_ptr_factory_.GetWeakPtr())));
  status_update_callbacks_.Notify(to_show, ProductMessageStatus::kEligible);
}

void ProductMessagingController::OnNewSession() {
  storage_service_->ResetProductMessagingData();
}

void ProductMessagingController::OnMessageShown(ProductMessageKey message_key) {
  if (message_key == current_message_) {
    current_message_shown_ = true;
    status_update_callbacks_.Notify(message_key,
                                    ProductMessageStatus::kShowing);
  }
}

std::string ProductMessagingController::DumpData() const {
  std::ostringstream oss;
  for (const auto& [key, data] : pending_messages_) {
    oss << "\n{ key: " << key.ToString() << " }";
  }
  return oss.str();
}

}  // namespace user_education
