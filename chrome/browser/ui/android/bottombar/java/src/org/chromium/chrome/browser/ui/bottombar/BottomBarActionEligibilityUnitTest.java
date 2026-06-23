// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicEnablingJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.actions.ActionId;

/** Unit tests for {@link BottomBarActionEligibility}. */
@NullMarked
@RunWith(BaseRobolectricTestRunner.class)
public class BottomBarActionEligibilityUnitTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private GlicEnabling.Natives mGlicEnablingJniMock;

    @Before
    public void setUp() {
        GlicEnablingJni.setInstanceForTesting(mGlicEnablingJniMock);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
    }

    @Test
    public void testGetEligibleExtraAction_GlicEnabled() {
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);
        assertEquals(ActionId.GLIC, BottomBarActionEligibility.getEligibleExtraAction(mProfile));
    }

    @Test
    public void testGetEligibleExtraAction_GlicDisabled() {
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(false);
        assertEquals(
                BottomBarActionEligibility.ACTION_NONE,
                BottomBarActionEligibility.getEligibleExtraAction(mProfile));
    }

    @Test
    public void testGetEligibleExtraAction_NullProfile() {
        assertEquals(
                BottomBarActionEligibility.ACTION_NONE,
                BottomBarActionEligibility.getEligibleExtraAction(null));
    }
}
