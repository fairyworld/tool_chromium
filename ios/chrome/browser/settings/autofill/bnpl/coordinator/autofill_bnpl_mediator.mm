// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/bnpl/coordinator/autofill_bnpl_mediator.h"

#import "base/check.h"
#import "components/autofill/core/browser/data_manager/personal_data_manager.h"
#import "components/prefs/pref_service.h"

@implementation AutofillBnplMediator {
  raw_ptr<autofill::PersonalDataManager> _personalDataManager;
  raw_ptr<PrefService> _prefs;
}

- (instancetype)initWithPersonalDataManager:
                    (autofill::PersonalDataManager*)personalDataManager
                                prefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    CHECK(personalDataManager);
    CHECK(prefService);
    _personalDataManager = personalDataManager;
    _prefs = prefService;
  }
  return self;
}

- (void)disconnect {
  _personalDataManager = nullptr;
  _prefs = nullptr;
}

@end
