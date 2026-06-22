// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_BNPL_UI_AUTOFILL_BNPL_TOS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_BNPL_UI_AUTOFILL_BNPL_TOS_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/autofill/bnpl/ui/autofill_bnpl_tos_consumer.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

@class AutofillBnplTosViewController;

// Delegate protocol to handle user actions in the Terms of Service view.
@protocol AutofillBnplTosViewControllerDelegate <NSObject>

// Called when the user clicks the primary "Continue" button.
- (void)didTapContinueInTosViewController:
    (AutofillBnplTosViewController*)viewController;

// Called when the user clicks the secondary "Cancel" button.
- (void)didTapCancelInTosViewController:
    (AutofillBnplTosViewController*)viewController;

// Called when the user clicks a hyperlink inside the legal text.
- (void)tosViewController:(AutofillBnplTosViewController*)viewController
              didTapOnURL:(NSURL*)url;

@end

// View Controller rendering the BNPL Terms of Service bottom sheet.
@interface AutofillBnplTosViewController
    : ConfirmationAlertViewController <AutofillBnplTosConsumer>

// The delegate for action events.
@property(nonatomic, weak) id<AutofillBnplTosViewControllerDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_BNPL_UI_AUTOFILL_BNPL_TOS_VIEW_CONTROLLER_H_
