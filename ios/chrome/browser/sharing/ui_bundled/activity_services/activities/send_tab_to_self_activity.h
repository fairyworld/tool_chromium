// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARING_UI_BUNDLED_ACTIVITY_SERVICES_ACTIVITIES_SEND_TAB_TO_SELF_ACTIVITY_H_
#define IOS_CHROME_BROWSER_SHARING_UI_BUNDLED_ACTIVITY_SERVICES_ACTIVITIES_SEND_TAB_TO_SELF_ACTIVITY_H_

#import "components/sync_device_info/device_info.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/activities/chrome_activity.h"

@protocol BrowserCoordinatorCommands;
@class ShareToData;

// Activity representing the generic "Send to your devices" action, which
// opens a target device selector picker UI when executed. Displayed in the
// Action (bottom) row of the Share Sheet.
@interface SendTabToSelfActivity : ChromeActivity

// Initializes the send tab to self activity with the given `data` and the
// `handler` that is used to add the tab to the other device. This initializer
// is used for the generic picker entry point.
- (instancetype)initWithData:(ShareToData*)data
                     handler:(id<BrowserCoordinatorCommands>)handler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

// Subclass representing a direct-send shortcut to a specific target device,
// executing a background send transaction immediately when executed. Displayed
// in the Share (top) row of the Share Sheet.
@interface SendTabToSelfShareActivity : SendTabToSelfActivity

// Initializes the send tab to self activity in device-specific mode,
// sending the tab directly to that target device with the given
// `activityTitle`, `deviceName`, `cacheGUID`, and `formFactor`.
- (instancetype)initWithData:(ShareToData*)data
                     handler:(id<BrowserCoordinatorCommands>)handler
               activityTitle:(NSString*)activityTitle
                  deviceName:(NSString*)deviceName
                   cacheGUID:(NSString*)cacheGUID
                  formFactor:(syncer::DeviceInfo::FormFactor)formFactor
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithData:(ShareToData*)data
                     handler:(id<BrowserCoordinatorCommands>)handler
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SHARING_UI_BUNDLED_ACTIVITY_SERVICES_ACTIVITIES_SEND_TAB_TO_SELF_ACTIVITY_H_
