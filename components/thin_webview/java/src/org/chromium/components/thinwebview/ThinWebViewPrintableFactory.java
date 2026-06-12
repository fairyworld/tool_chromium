// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.thinwebview;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;
import org.chromium.printing.Printable;

/**
 * Factory for creating {@link Printable} instances for {@link WebContents}. Implemented in chrome/
 * to avoid components/ depending on chrome/ printing implementation.
 */
@NullMarked
public interface ThinWebViewPrintableFactory {
    /** Creates a {@link Printable} for the given {@link WebContents}. */
    @Nullable Printable create(WebContents webContents);
}
