// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/coordinator/main_toolbar_mediator.h"

#import "components/omnibox/browser/omnibox_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

@interface MainToolbarMediator () <BooleanObserver>
@end

@implementation MainToolbarMediator {
  PrefBackedBoolean* _bottomOmniboxPref;
  __weak LayoutState* _layoutState;
}

- (instancetype)initWithPrefService:(PrefService*)prefService
                        layoutState:(LayoutState*)layoutState {
  self = [super init];
  if (self) {
    CHECK(prefService);
    CHECK(layoutState);
    _layoutState = layoutState;
    _bottomOmniboxPref = [[PrefBackedBoolean alloc]
        initWithPrefService:prefService
                   prefName:omnibox::kIsOmniboxInBottomPosition];
    [_bottomOmniboxPref setObserver:self];

    // Set the initial toolbar position.
    _layoutState.toolbarPosition = [self isOmniboxInBottomPosition]
                                       ? ToolbarPosition::kBottom
                                       : ToolbarPosition::kTop;
  }
  return self;
}

- (void)disconnect {
  [_bottomOmniboxPref stop];
  _bottomOmniboxPref = nil;
}

- (BOOL)isOmniboxInBottomPosition {
  return IsBottomOmniboxAvailable() && _bottomOmniboxPref.value;
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _bottomOmniboxPref) {
    _layoutState.toolbarPosition = [self isOmniboxInBottomPosition]
                                       ? ToolbarPosition::kBottom
                                       : ToolbarPosition::kTop;
  }
}

@end
