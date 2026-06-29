// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.appcompat.widget.Toolbar;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.search.SettingsSearchCoordinator;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Implementation of {@link SettingsPage.FragmentDelegate} that manages {@link SettingsPage}
 * fragments. Exists because {@link SettingsPage} is at a lower level in the dependency graph than
 * some of the dependencies needed by {@link FragmentDependencyProvider}.
 */
@NullMarked
public class SettingsPageFragmentDelegateImpl
        implements SettingsPage.FragmentDelegate, SettingsMenuHelper.Delegate {
    private static final String SETTINGS_NATIVE_PAGE_TAG = "settings_native_page";

    private final Activity mActivity;
    private final Profile mProfile;
    private final WindowAndroid mWindowAndroid;
    private final ActivityResultTracker mActivityResultTracker;
    private final SnackbarManager mSnackbarManager;
    private final BottomSheetController mBottomSheetController;
    private final ModalDialogManager mModalDialogManager;

    private @Nullable SettingsHostFragment mSettingsHostFragment;
    private FragmentManager.@Nullable FragmentLifecycleCallbacks mDependencyProvider;

    public SettingsPageFragmentDelegateImpl(
            Activity activity,
            Profile profile,
            WindowAndroid windowAndroid,
            ActivityResultTracker activityResultTracker,
            SnackbarManager snackbarManager,
            BottomSheetController bottomSheetController,
            ModalDialogManager modalDialogManager) {
        mActivity = activity;
        mProfile = profile;
        mWindowAndroid = windowAndroid;
        mActivityResultTracker = activityResultTracker;
        mSnackbarManager = snackbarManager;
        mBottomSheetController = bottomSheetController;
        mModalDialogManager = modalDialogManager;
    }

    @Override
    public void initSettings(ViewGroup containerView) {
        FragmentManager fragmentManager =
                ((FragmentActivity) mActivity).getSupportFragmentManager();

        // Create the dependency provider for settings fragments.
        OneshotSupplierImpl<WindowAndroid> windowAndroidSupplier = new OneshotSupplierImpl<>();
        windowAndroidSupplier.set(mWindowAndroid);

        OneshotSupplierImpl<SnackbarManager> snackbarSupplier = new OneshotSupplierImpl<>();
        snackbarSupplier.set(mSnackbarManager);

        OneshotSupplierImpl<BottomSheetController> bottomSheetSupplier =
                new OneshotSupplierImpl<>();
        bottomSheetSupplier.set(mBottomSheetController);

        var modalDialogSupplier = ObservableSuppliers.<ModalDialogManager>createMonotonic();
        modalDialogSupplier.set(mModalDialogManager);

        mDependencyProvider =
                new FragmentDependencyProvider(
                        mActivity,
                        mProfile,
                        windowAndroidSupplier,
                        mActivityResultTracker,
                        snackbarSupplier,
                        bottomSheetSupplier,
                        modalDialogSupplier,
                        () -> null);

        fragmentManager.registerFragmentLifecycleCallbacks(
                mDependencyProvider, /* recursive= */ true);

        // Inflate the settings layout into the container view.
        // TODO(crbug.com/521895796): Rename settings_activity.xml since with settings-in-a-tab it
        // doesn't map directly to its own activity.
        View settingsView =
                LayoutInflater.from(mActivity).inflate(R.layout.settings_activity, null);
        containerView.addView(settingsView);
        ViewGroup fragmentContainer = settingsView.findViewById(R.id.content);
        Toolbar toolbar = settingsView.findViewById(R.id.action_bar);

        // Set up the back navigation arrow in the toolbar.
        // TODO(crbug.com/521895796): This is a placeholder for testing. Move the arrow to
        // the right column before launch.
        toolbar.setNavigationIcon(R.drawable.ic_arrow_back_white_24dp);
        toolbar.setNavigationOnClickListener(v -> mActivity.onBackPressed());

        toolbar.setTitle(R.string.settings);

        // TODO(crbug.com/521895796): Set up title updater.

        // TODO(crbug.com/521895796): Set up search coordinator.

        // Set up Help Menu on Toolbar.
        SettingsMenuHelper.onCreateOptionsMenu(toolbar.getMenu(), mActivity);
        SettingsMenuHelper.onPrepareOptionsMenu(toolbar.getMenu());
        toolbar.setOnMenuItemClickListener(
                item -> SettingsMenuHelper.onOptionsItemSelected(item, mActivity, this));

        mSettingsHostFragment =
                (SettingsHostFragment) fragmentManager.findFragmentByTag(SETTINGS_NATIVE_PAGE_TAG);
        if (mSettingsHostFragment == null) {
            mSettingsHostFragment = new SettingsHostFragment();
            fragmentManager
                    .beginTransaction()
                    .add(fragmentContainer.getId(), mSettingsHostFragment, SETTINGS_NATIVE_PAGE_TAG)
                    .commitAllowingStateLoss();
        }
    }

    @Override
    public void destroySettings() {
        FragmentManager fragmentManager =
                ((FragmentActivity) mActivity).getSupportFragmentManager();
        assumeNonNull(mDependencyProvider);
        fragmentManager.unregisterFragmentLifecycleCallbacks(mDependencyProvider);
        mDependencyProvider = null;
        assumeNonNull(mSettingsHostFragment);
        fragmentManager.beginTransaction().remove(mSettingsHostFragment).commitAllowingStateLoss();
        mSettingsHostFragment = null;
    }

    @Override
    public @Nullable Fragment getMainFragment() {
        return assumeNonNull(mSettingsHostFragment).getActiveFragment();
    }

    @Override
    public @Nullable MultiColumnSettings getMultiColumnSettings() {
        return (MultiColumnSettings) getMainFragment();
    }

    @Override
    public @Nullable SettingsSearchCoordinator getSearchCoordinator() {
        // TODO(crbug.com/521895796): Set up search coordinator.
        return null;
    }

    @Override
    public HelpAndFeedbackLauncher getHelpAndFeedbackLauncher() {
        return HelpAndFeedbackLauncherImpl.getForProfile(mProfile);
    }

    @Override
    public void finishSettings() {
        // TODO(crbug.com/521895796): Define settings-in-tab close behavior.
    }

    @Override
    public void onBackPressed() {
        mActivity.onBackPressed();
    }

    @Override
    public void finishCurrentSettings(Fragment fragment) {
        // TODO(crbug.com/521895796): Define settings-in-tab finish/back behavior.
    }
}
