// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_BNPL_UI_AUTOFILL_BNPL_TOS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_BNPL_UI_AUTOFILL_BNPL_TOS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"

@protocol AutofillBnplTosConsumer;
@class AutofillBnplTosMediator;

// Delegate protocol for the mediator to notify the coordinator of actions.
@protocol AutofillBnplTosMediatorDelegate <NSObject>

// Called when the user continues and the accept callback has been triggered.
- (void)tosMediatorDidAccept:(AutofillBnplTosMediator*)mediator;

// Called when the user cancels and the cancel callback has been triggered.
- (void)tosMediatorDidCancel:(AutofillBnplTosMediator*)mediator;

@end

// Mediator managing the data flow and callbacks for the BNPL ToS screen.
@interface AutofillBnplTosMediator : NSObject

// The consumer updated by this mediator.
@property(nonatomic, weak) id<AutofillBnplTosConsumer> consumer;

// The delegate handling completion actions.
@property(nonatomic, weak) id<AutofillBnplTosMediatorDelegate> delegate;

// Initializes the mediator with the BNPL ToS data model and the closure
// callbacks.
- (instancetype)initWithModel:(autofill::payments::BnplTosModel)model
               acceptCallback:(base::OnceClosure)acceptCallback
               cancelCallback:(base::OnceClosure)cancelCallback
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Triggers the accept callback and notifies delegate.
- (void)didTapContinue;

// Triggers the cancel callback and notifies delegate.
- (void)didTapCancel;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_BNPL_UI_AUTOFILL_BNPL_TOS_MEDIATOR_H_
