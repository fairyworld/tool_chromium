// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_settings_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/metrics/user_metrics.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/utils/autofill_and_passwords_item_utils.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_settings_constants.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

enum SectionIdentifier {
  SectionIdentifierBasics = kSectionIdentifierEnumZero,
  SectionIdentifierSwitches,
  SectionIdentifierWhenOn,
  SectionIdentifierThingsToConsider,
  SectionIdentifierVerificationSwitch,
};

enum ItemType {
  ItemTypeEnhancedAutofillSwitch = kItemTypeEnumZero,
  ItemTypeVerificationSwitch,
  ItemTypeVerificationFooter,
  ItemTypeFooter,
  ItemTypeHeader,
  ItemTypeLabel,
};

}  // namespace

@implementation AutofillSettingsTableViewController {
  BOOL _settingsAreDismissed;
  BOOL _enhancedAutofillEnabled;
  BOOL _autofillAIAllowedByPolicy;
  BOOL _userVerificationEnabled;
  BOOL _userVerificationSwitchEnabled;
  BOOL _userVerificationSettingVisible;
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.delegate autofillSettingsTableViewControllerDidRemove:self];
  }
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = l10n_util::GetNSString(IDS_IOS_SETTINGS_AUTOFILL_SETTINGS);
  [self loadModel];
}

- (void)loadModel {
  [super loadModel];
  if (_settingsAreDismissed) {
    return;
  }

  TableViewModel<TableViewItem*>* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierSwitches];
  [model addItem:EnhancedAutofillSwitchItem(
                     ItemTypeEnhancedAutofillSwitch, _enhancedAutofillEnabled,
                     self, @selector(enhancedAutofillSwitchChanged:))
      toSectionWithIdentifier:SectionIdentifierSwitches];
  [model setFooter:EnhancedAutofillSwitchFooter(ItemTypeFooter)
      forSectionWithIdentifier:SectionIdentifierSwitches];

  [model addSectionWithIdentifier:SectionIdentifierWhenOn];
  [model setHeader:EnhancedAutofillWhenOnSectionHeader(ItemTypeHeader)
      forSectionWithIdentifier:SectionIdentifierWhenOn];
  [model addItem:EnhancedAutofillCanFillDifficultFieldsItem(ItemTypeLabel)
      toSectionWithIdentifier:SectionIdentifierWhenOn];

  [model addSectionWithIdentifier:SectionIdentifierThingsToConsider];
  [model setHeader:EnhancedAutofillThingsToConsiderSectionHeader(ItemTypeHeader)
      forSectionWithIdentifier:SectionIdentifierThingsToConsider];
  [model addItem:EnhancedAutofillDataUsageItem(ItemTypeLabel)
      toSectionWithIdentifier:SectionIdentifierThingsToConsider];

  if (!_autofillAIAllowedByPolicy) {
    [model addItem:EnhancedAutofillEnterpriseManagedLoggingDisabledItem(
                       ItemTypeLabel)
        toSectionWithIdentifier:SectionIdentifierThingsToConsider];
  }

  if (_userVerificationSettingVisible) {
    [model addSectionWithIdentifier:SectionIdentifierVerificationSwitch];
    [model addItem:AutofillVerificationSwitchItem(
                       ItemTypeVerificationSwitch,
                       _userVerificationSwitchEnabled, _userVerificationEnabled,
                       self, @selector(verificationSwitchChanged:))
        toSectionWithIdentifier:SectionIdentifierVerificationSwitch];
    [model setFooter:AutofillVerificationSwitchFooter(
                         ItemTypeVerificationFooter)
        forSectionWithIdentifier:SectionIdentifierVerificationSwitch];
  }
}

#pragma mark - AutofillSettingsConsumer

- (void)setEnhancedAutofillEnabled:(BOOL)enabled {
  if (_enhancedAutofillEnabled == enabled) {
    return;
  }
  _enhancedAutofillEnabled = enabled;
  if (self.isViewLoaded) {
    [self setSwitchItemOn:enabled
                 itemType:ItemTypeEnhancedAutofillSwitch
        sectionIdentifier:SectionIdentifierSwitches];
  }
}

- (void)setAutofillAIAllowedByPolicy:(BOOL)allowed {
  if (_autofillAIAllowedByPolicy == allowed) {
    return;
  }
  _autofillAIAllowedByPolicy = allowed;
  if (self.isViewLoaded) {
    [self reloadData];
  }
}

- (void)setUserVerificationEnabled:(BOOL)enabled {
  _userVerificationEnabled = enabled;
  if (self.isViewLoaded) {
    [self setSwitchItemOn:enabled
                 itemType:ItemTypeVerificationSwitch
        sectionIdentifier:SectionIdentifierVerificationSwitch];
  }
}

- (void)setUserVerificationSwitchEnabled:(BOOL)enabled {
  if (_userVerificationSwitchEnabled == enabled) {
    return;
  }
  _userVerificationSwitchEnabled = enabled;
  if (self.isViewLoaded) {
    [self updateVerificationSwitchEnabledState];
  }
}

- (void)setUserVerificationSettingVisible:(BOOL)visible {
  if (_userVerificationSettingVisible == visible) {
    return;
  }
  _userVerificationSettingVisible = visible;
  if (self.isViewLoaded) {
    [self reloadData];
  }
}

#pragma mark - Switch Callbacks

- (void)enhancedAutofillSwitchChanged:(UISwitch*)switchView {
  [self.mutator setEnhancedAutofillEnabled:switchView.on];
}

- (void)verificationSwitchChanged:(UISwitch*)switchView {
  [self.mutator setUserVerificationEnabled:switchView.on];
}

#pragma mark - Switch Helpers

- (void)setSwitchItemOn:(BOOL)on
               itemType:(ItemType)switchItemType
      sectionIdentifier:(SectionIdentifier)sectionIdentifier {
  if (![self.tableViewModel hasItemForItemType:switchItemType
                             sectionIdentifier:sectionIdentifier]) {
    return;
  }
  NSIndexPath* switchPath =
      [self.tableViewModel indexPathForItemType:switchItemType
                              sectionIdentifier:sectionIdentifier];
  TableViewSwitchItem* switchItem =
      base::apple::ObjCCastStrict<TableViewSwitchItem>(
          [self.tableViewModel itemAtIndexPath:switchPath]);
  switchItem.on = on;
  [self reconfigureCellsForItems:@[ switchItem ]];
}

- (void)updateVerificationSwitchEnabledState {
  if (![self.tableViewModel
          hasItemForItemType:ItemTypeVerificationSwitch
           sectionIdentifier:SectionIdentifierVerificationSwitch]) {
    return;
  }
  NSIndexPath* switchPath = [self.tableViewModel
      indexPathForItemType:ItemTypeVerificationSwitch
         sectionIdentifier:SectionIdentifierVerificationSwitch];
  TableViewSwitchItem* switchItem =
      base::apple::ObjCCastStrict<TableViewSwitchItem>(
          [self.tableViewModel itemAtIndexPath:switchPath]);
  switchItem.enabled = _userVerificationSwitchEnabled;
  [self reconfigureCellsForItems:@[ switchItem ]];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(base::UserMetricsAction("MobileAutofillSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction("MobileAutofillSettingsBack"));
}

- (void)settingsWillBeDismissed {
  DCHECK(!_settingsAreDismissed);

  _settingsAreDismissed = YES;
}

@end
