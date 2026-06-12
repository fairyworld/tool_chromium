// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.printing;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.ServiceImpl;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.thinwebview.ThinWebViewPrintableFactory;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.WebContents;
import org.chromium.printing.Printable;

/** Implementation of {@link ThinWebViewPrintableFactory} for Chrome. */
@NullMarked
@ServiceImpl(ThinWebViewPrintableFactory.class)
public class ChromeThinWebViewPrintableFactory implements ThinWebViewPrintableFactory {
    @Override
    public @Nullable Printable create(WebContents webContents) {
        Profile profile = Profile.fromWebContents(webContents);
        if (profile == null) return null;
        if (!UserPrefs.get(profile).getBoolean(Pref.PRINTING_ENABLED)) {
            return null;
        }
        return new WebContentsPrinter(webContents);
    }
}
