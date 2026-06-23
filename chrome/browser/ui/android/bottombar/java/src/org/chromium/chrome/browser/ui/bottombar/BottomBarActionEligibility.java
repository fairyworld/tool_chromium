// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.actions.ActionId;

/** Helper class to resolve the eligibility of bottom bar actions based on profile. */
@NullMarked
class BottomBarActionEligibility {

    /** Represents a sentinel value indicating that no action is eligible. */
    static final int ACTION_NONE = -1;

    /**
     * Resolves which action (if any) should be displayed in the bottom bar's shared extra
     * container.
     *
     * @param profile The current user profile.
     * @return The eligible {@link ActionId} (currently only {@link ActionId#GLIC}), or {@link
     *     #ACTION_NONE} if no action is eligible.
     */
    /* package-private */ static int getEligibleExtraAction(@Nullable Profile profile) {
        if (profile == null) return ACTION_NONE;

        // Evaluate if GLIC is enabled.
        boolean isGlicEligible = GlicEnabling.isEnabledForProfile(profile);

        return isGlicEligible ? ActionId.GLIC : ACTION_NONE;
    }
}
