// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_TO_SELF_TAB_CARD_LABEL_DATA_H_
#define IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_TO_SELF_TAB_CARD_LABEL_DATA_H_

#import <string>

#import "base/memory/raw_ptr.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebState;
}

// A WebState UserData class that stores the metadata for a Send Tab to Self
// tab card label (like the sender device name). It automatically cleans itself
// up when the tab is shown (viewed by the user).
class SendTabToSelfTabCardLabelData
    : public web::WebStateUserData<SendTabToSelfTabCardLabelData>,
      public web::WebStateObserver {
 public:
  ~SendTabToSelfTabCardLabelData() override;

  // Returns the formatted localized label string for the tab card.
  NSString* GetLabelText() const;

 private:
  friend class web::WebStateUserData<SendTabToSelfTabCardLabelData>;

  SendTabToSelfTabCardLabelData(web::WebState* web_state,
                                const std::string& sender_device_name);

  // web::WebStateObserver:
  void WasShown(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  raw_ptr<web::WebState> web_state_ = nullptr;
  std::string sender_device_name_;
};

#endif  // IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_TO_SELF_TAB_CARD_LABEL_DATA_H_
