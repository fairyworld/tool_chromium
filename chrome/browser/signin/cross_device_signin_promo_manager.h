// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CROSS_DEVICE_SIGNIN_PROMO_MANAGER_H_
#define CHROME_BROWSER_SIGNIN_CROSS_DEVICE_SIGNIN_PROMO_MANAGER_H_

#include "base/functional/callback_forward.h"

class Browser;
class Profile;

enum class CrossDeviceSigninPromoEntryPoint {
  kProfileMenu,
  kHistoryPage,
};

// Returns true if the cross-device sign-in promo should be shown.
bool ShouldShowCrossDeviceSigninPromo(
    CrossDeviceSigninPromoEntryPoint entry_point,
    Profile* profile);

// Called when the cross-device sign-in promo is shown to the user.
void OnCrossDeviceSigninPromoShown(CrossDeviceSigninPromoEntryPoint entry_point,
                                   Profile* profile);

// Called when the cross-device sign-in promo is dismissed by the user.
void OnCrossDeviceSigninPromoDismissed(
    CrossDeviceSigninPromoEntryPoint entry_point,
    Profile* profile);

// Opens the "Sign in to phone" QR code bubble associated with the specified
// `browser` window.
// `closing_callback` is run when the bubble closes.
void OpenSigninToPhoneQrCodeBubble(Browser* browser,
                                   base::OnceClosure closing_callback);

#endif  // CHROME_BROWSER_SIGNIN_CROSS_DEVICE_SIGNIN_PROMO_MANAGER_H_
