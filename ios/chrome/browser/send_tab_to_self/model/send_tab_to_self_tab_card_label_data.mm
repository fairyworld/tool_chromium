// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_tab_card_label_data.h"

#import "base/memory/ptr_util.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

SendTabToSelfTabCardLabelData::SendTabToSelfTabCardLabelData(
    web::WebState* web_state,
    const std::string& sender_device_name)
    : web_state_(web_state), sender_device_name_(sender_device_name) {
  web_state_->AddObserver(this);
}

SendTabToSelfTabCardLabelData::~SendTabToSelfTabCardLabelData() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

NSString* SendTabToSelfTabCardLabelData::GetLabelText() const {
  return l10n_util::GetNSStringF(
      IDS_SEND_TAB_TO_SELF_INFOBAR_AUTO_OPEN_SUBTITLE,
      base::UTF8ToUTF16(sender_device_name_));
}

#pragma mark - web::WebStateObserver

void SendTabToSelfTabCardLabelData::WasShown(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  // Deletes itself since the tab is now viewed.
  RemoveFromWebState(web_state);
}

void SendTabToSelfTabCardLabelData::WebStateDestroyed(
    web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
  // Deletes itself.
  RemoveFromWebState(web_state);
}
