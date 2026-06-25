// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/cross_device_signin_promo_manager.h"

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "components/signin/public/base/signin_switches.h"

bool ShouldShowCrossDeviceSigninPromo(
    CrossDeviceSigninPromoEntryPoint entry_point,
    Profile* profile) {
  // TODO(crbug.com/527889253): Implement the actual logic.
  return base::FeatureList::IsEnabled(switches::kCrossDeviceSigninFromDesktop);
}

void OnCrossDeviceSigninPromoShown(CrossDeviceSigninPromoEntryPoint entry_point,
                                   Profile* profile) {
  // TODO(crbug.com/527889253): Implement the actual logic.
}

void OnCrossDeviceSigninPromoDismissed(
    CrossDeviceSigninPromoEntryPoint entry_point,
    Profile* profile) {
  // TODO(crbug.com/527889253): Implement the actual logic.
}

void OpenSigninToPhoneQrCodeBubble(Browser* browser,
                                   base::OnceClosure closing_callback) {
  CHECK(base::FeatureList::IsEnabled(switches::kCrossDeviceSigninFromDesktop));
  // TODO(crbug.com/527889253): Implement the actual logic.
  std::move(closing_callback).Run();
}
