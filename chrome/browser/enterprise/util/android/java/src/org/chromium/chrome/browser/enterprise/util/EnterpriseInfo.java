// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enterprise.util;

import org.chromium.base.Callback;
import org.chromium.build.annotations.Nullable;

/**
 * @deprecated Use {@link org.chromium.components.policy.EnterpriseInfo} instead.
 */
@Deprecated
public class EnterpriseInfo {
    private static @Nullable EnterpriseInfo sInstance;

    /**
     * @deprecated Use {@link org.chromium.components.policy.EnterpriseInfo.OwnedState} instead.
     */
    @Deprecated
    public static class OwnedState {
        public final boolean mDeviceOwned;
        public final boolean mProfileOwned;

        public OwnedState(boolean isDeviceOwned, boolean isProfileOwned) {
            mDeviceOwned = isDeviceOwned;
            mProfileOwned = isProfileOwned;
        }

        @Override
        public boolean equals(Object other) {
            if (this == other) return true;
            if (other == null) return false;
            if (!(other instanceof OwnedState)) return false;
            OwnedState otherOwnedState = (OwnedState) other;
            return this.mDeviceOwned == otherOwnedState.mDeviceOwned
                    && this.mProfileOwned == otherOwnedState.mProfileOwned;
        }
    }

    public static EnterpriseInfo getInstance() {
        if (sInstance == null) sInstance = new EnterpriseInfo();
        return sInstance;
    }

    public void getDeviceEnterpriseInfo(Callback<OwnedState> callback) {
        org.chromium.components.policy.EnterpriseInfo.getInstance()
                .getDeviceEnterpriseInfo(
                        result -> {
                            if (result == null) {
                                callback.onResult(null);
                            } else {
                                callback.onResult(
                                        new OwnedState(result.mDeviceOwned, result.mProfileOwned));
                            }
                        });
    }

    public @Nullable OwnedState getDeviceEnterpriseInfoSync() {
        var result =
                org.chromium.components.policy.EnterpriseInfo.getInstance()
                        .getDeviceEnterpriseInfoSync();
        if (result == null) return null;
        return new OwnedState(result.mDeviceOwned, result.mProfileOwned);
    }

    public void logDeviceEnterpriseInfo() {
        org.chromium.components.policy.EnterpriseInfo.getInstance().logDeviceEnterpriseInfo();
    }

    public static void setInstanceForTest(EnterpriseInfo instance) {
        var oldValue = sInstance;
        sInstance = instance;
        org.chromium.base.ResettersForTesting.register(() -> sInstance = oldValue);

        if (instance == null) {
            org.chromium.components.policy.EnterpriseInfo.setInstanceForTest(null); // IN-TEST
        } else {
            // Forward mock calls to the underlying policy singleton to prevent split-brain mocking
            org.chromium.components.policy.EnterpriseInfo.setInstanceForTest( // IN-TEST
                    new org.chromium.components.policy.EnterpriseInfo() {
                        @Override
                        public void getDeviceEnterpriseInfo(
                                Callback<org.chromium.components.policy.EnterpriseInfo.OwnedState>
                                        callback) {
                            instance.getDeviceEnterpriseInfo(
                                    result -> {
                                        if (result == null) {
                                            callback.onResult(null);
                                        } else {
                                            callback.onResult(
                                                    new org.chromium.components.policy
                                                            .EnterpriseInfo.OwnedState(
                                                            result.mDeviceOwned,
                                                            result.mProfileOwned));
                                        }
                                    });
                        }

                        @Override
                        public org.chromium.components.policy.EnterpriseInfo.@Nullable OwnedState
                                getDeviceEnterpriseInfoSync() {
                            var result = instance.getDeviceEnterpriseInfoSync();
                            if (result == null) return null;
                            return new org.chromium.components.policy.EnterpriseInfo.OwnedState(
                                    result.mDeviceOwned, result.mProfileOwned);
                        }

                        @Override
                        public void logDeviceEnterpriseInfo() {
                            instance.logDeviceEnterpriseInfo();
                        }
                    });
        }
    }
}
