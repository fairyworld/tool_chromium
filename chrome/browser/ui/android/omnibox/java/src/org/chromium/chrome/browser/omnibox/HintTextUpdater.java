// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.StringRes;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.contextual_tasks.ContextualTasksUtils;
import org.chromium.chrome.browser.omnibox.SearchEngineService.SearchEngineNameObserver;
import org.chromium.chrome.browser.omnibox.fusebox.ComposeboxQueryControllerBridge;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.components.contextual_search.InputState;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteInput.SiteSearchData;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.ToolConfigProto.ToolConfig;
import org.chromium.components.omnibox.ToolModeUtils;
import org.chromium.url.GURL;

/**
 * Handles tracking updates and calculating what should be used for the omnibox hint text. Accepts a
 * callback to push updated text back to client.
 */
@NullMarked
public class HintTextUpdater implements LocationBarDataProvider.Observer {
    private final Context mContext;
    private final LocationBarDataProvider mLocationBarDataProvider;
    private final LocationBarEmbedderUiOverrides mEmbedderUiOverrides;
    private final Callback<String> mUpdateHintTextCallback;
    private final MonotonicObservableSupplier<SearchEngineService> mSearchEngineServiceSupplier;
    private final SearchEngineNameObserver mSearchEngineNameObserver = this::updateHintText;
    private final Callback<@AutocompleteRequestType Integer> mAutocompleteRequestTypeObserver =
            (type) -> updateHintText();
    private final Callback<@Nullable SiteSearchData> mSiteSearchDataObserver =
            (siteSearchData) -> updateHintText();
    private final Callback<SearchEngineService> mSearchEngineServiceObserver =
            this::onSearchEngineServiceChanged;

    private @Nullable SearchEngineService mSearchEngineService;
    private @Nullable AutocompleteInput mCurrentInput;

    public HintTextUpdater(
            Context context,
            LocationBarDataProvider locationBarDataProvider,
            LocationBarEmbedderUiOverrides embedderUiOverrides,
            MonotonicObservableSupplier<SearchEngineService> searchEngineServiceSupplier,
            Callback<String> updateHintTextCallback) {
        mContext = context;
        mLocationBarDataProvider = locationBarDataProvider;
        mEmbedderUiOverrides = embedderUiOverrides;
        mSearchEngineServiceSupplier = searchEngineServiceSupplier;
        mUpdateHintTextCallback = updateHintTextCallback;
        mLocationBarDataProvider.addObserver(this);
        mSearchEngineServiceSupplier.addSyncObserverAndPostIfNonNull(mSearchEngineServiceObserver);
    }

    /** Clean up resources used by this class. */
    public void destroy() {
        mLocationBarDataProvider.removeObserver(this);
        mSearchEngineServiceSupplier.removeObserver(mSearchEngineServiceObserver);
        if (mSearchEngineService != null) {
            mSearchEngineService.removeSearchEngineNameObserver(mSearchEngineNameObserver);
        }
        endInput();
    }

    /**
     * Returns the hint text to be used on the New Tab Page.
     *
     * @param context The Android context used to load resources.
     * @param searchEngineService The SearchEngineService to get the search engine name and info.
     * @return The hint text string.
     */
    public static String getNtpHintText(Context context, SearchEngineService searchEngineService) {
        return getOmniboxHintText(
                context,
                AutocompleteRequestType.SEARCH,
                /* fuseboxSessionState= */ null,
                searchEngineService);
    }

    /**
     * Begins a new input session.
     *
     * @param input The input for the current session.
     */
    public void beginInput(AutocompleteInput input) {
        if (mCurrentInput != null) {
            endInput();
        }
        mCurrentInput = input;
        mCurrentInput.getRequestTypeSupplier().addSyncObserver(mAutocompleteRequestTypeObserver);
        mCurrentInput.getSiteSearchDataSupplier().addSyncObserver(mSiteSearchDataObserver);
        updateHintText();
    }

    /** Ends the current input session. */
    public void endInput() {
        if (mCurrentInput != null) {
            mCurrentInput.getRequestTypeSupplier().removeObserver(mAutocompleteRequestTypeObserver);
            mCurrentInput.getSiteSearchDataSupplier().removeObserver(mSiteSearchDataObserver);
        }
        mCurrentInput = null;
        updateHintText();
    }

    @Override
    public void onTitleChanged() {
        updateHintText();
    }

    private void onSearchEngineServiceChanged(@Nullable SearchEngineService service) {
        if (mSearchEngineService != null) {
            mSearchEngineService.removeSearchEngineNameObserver(mSearchEngineNameObserver);
        }
        mSearchEngineService = service;
        if (mSearchEngineService != null) {
            mSearchEngineService.addSearchEngineNameObserver(mSearchEngineNameObserver);
        }
        updateHintText();
    }

    private void updateHintText() {
        if (mSearchEngineService == null) return;
        if (mEmbedderUiOverrides.isEmbedderControlledHint()) return;

        if (mCurrentInput != null && mCurrentInput.getSiteSearchData() != null) {
            mUpdateHintTextCallback.onResult("");
            return;
        }

        @AutocompleteRequestType
        int requestType =
                mCurrentInput == null
                        ? mLocationBarDataProvider.getDefaultRequestType()
                        : mCurrentInput.getRequestType();

        FuseboxSessionState fuseboxSession = FuseboxSessionState.from(mLocationBarDataProvider);
        String hint =
                getOmniboxHintText(mContext, requestType, fuseboxSession, mSearchEngineService);
        mUpdateHintTextCallback.onResult(hint);
    }

    private static String getOmniboxHintText(
            Context context,
            @AutocompleteRequestType int type,
            @Nullable FuseboxSessionState fuseboxSessionState,
            SearchEngineService searchEngineService) {
        if (fuseboxSessionState != null) {
            var input = fuseboxSessionState.getAutocompleteInput();
            if (input != null) {
                GURL url = input.getPageUrl();
                String title = input.getPageTitle();
                if (ContextualTasksUtils.isContextualTasksUrl(url) && !TextUtils.isEmpty(title)) {
                    return title;
                }
            }
        }

        String searchEngineName = searchEngineService.getSearchEngineName();
        if (TextUtils.isEmpty(searchEngineName)) {
            return OmniboxResourceProvider.getString(context, R.string.omnibox_empty_hint);
        }

        if (OmniboxFeatures.sShowModelPicker.getValue()) {
            if (ToolModeUtils.isAimRequest(type)) {
                String toolHint = getToolHintFromInputState(type, fuseboxSessionState);
                if (!TextUtils.isEmpty(toolHint)) {
                    return toolHint;
                }
            }
        }

        @StringRes
        int res =
                switch (type) {
                    case AutocompleteRequestType.AI_MODE ->
                            R.string.omnibox_ai_mode_scope_placeholder_text;
                    case AutocompleteRequestType.IMAGE_GENERATION ->
                            R.string.omnibox_empty_hint_for_image_generation;
                    default ->
                            OmniboxFeatures.sUseAskHintForNtp.getValue()
                                            && searchEngineService.isDefaultSearchEngineGoogle()
                                    ? R.string.omnibox_empty_ask_hint_with_dse_name
                                    : R.string.omnibox_empty_hint_with_dse_name;
                };
        return OmniboxResourceProvider.getString(context, res, searchEngineName);
    }

    private static @Nullable String getToolHintFromInputState(
            @AutocompleteRequestType int requestType,
            @Nullable FuseboxSessionState fuseboxSessionState) {
        if (fuseboxSessionState == null) return null;

        ComposeboxQueryControllerBridge bridge =
                fuseboxSessionState.getComposeboxQueryControllerBridge();
        if (bridge == null) return null;

        InputState inputState = bridge.getInputStateSupplier().get();
        if (inputState == null) return null;

        int activeTool =
                ToolModeUtils.getToolModeForRequestType(requestType, /* hasAttachments= */ false);
        for (ToolConfig config : inputState.toolConfigs) {
            if (config.getToolValue() == activeTool) {
                return config.getHintText();
            }
        }

        return null;
    }
}
