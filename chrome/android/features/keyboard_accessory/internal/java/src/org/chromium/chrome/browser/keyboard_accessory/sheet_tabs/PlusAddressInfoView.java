// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.widget.LinearLayout;

import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.chips.ChipView;

class PlusAddressInfoView extends LinearLayout {
    /** Constructor for inflating from XML. */
    public PlusAddressInfoView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
    }

    void setIconForBitmap(@Nullable Drawable icon) {}

    ChipView getPlusAddress() {
        return null;
    }
}
