// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.SearchEngineService.SearchEngineNameObserver;
import org.chromium.chrome.browser.omnibox.fusebox.ComposeboxQueryControllerBridge;
import org.chromium.components.contextual_search.InputState;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteInput.SiteSearchData;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.ToolConfigProto.ToolConfig;
import org.chromium.components.omnibox.ToolModeProto.ToolMode;
import org.chromium.url.GURL;

/** Unit tests for {@link HintTextUpdater}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HintTextUpdaterTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private LocationBarDataProvider mLocationBarDataProvider;
    @Mock private SearchEngineService mSearchEngineService;
    @Mock private AutocompleteInput mAutocompleteInput;
    @Mock private LocationBarEmbedderUiOverrides mEmbedderUiOverrides;
    @Mock private Callback<String> mUpdateHintTextCallback;
    @Mock private FuseboxSessionState mFuseboxSessionState;
    @Mock private ComposeboxQueryControllerBridge mComposeboxQueryControllerBridge;

    @Captor private ArgumentCaptor<String> mHintTextCaptor;
    @Captor private ArgumentCaptor<SearchEngineNameObserver> mSearchEngineNameObserverCaptor;

    private final SettableMonotonicObservableSupplier<InputState> mInputStateSupplier =
            ObservableSuppliers.createMonotonic();
    private final SettableNonNullObservableSupplier<Integer> mRequestTypeSupplier =
            ObservableSuppliers.createNonNull(AutocompleteRequestType.SEARCH);
    private final SettableNullableObservableSupplier<SiteSearchData> mSiteSearchDataSupplier =
            ObservableSuppliers.createNullable();
    private final SettableMonotonicObservableSupplier<SearchEngineService>
            mSearchEngineServiceSupplier = ObservableSuppliers.createMonotonic();

    private HintTextUpdater mUpdater;
    private SearchEngineNameObserver mSearchEngineNameObserver;

    @Before
    @SuppressWarnings("unchecked")
    public void setUp() {
        mUpdater =
                new HintTextUpdater(
                        ApplicationProvider.getApplicationContext(),
                        mLocationBarDataProvider,
                        mEmbedderUiOverrides,
                        mSearchEngineServiceSupplier,
                        mUpdateHintTextCallback);

        when(mLocationBarDataProvider.getFuseboxSessionState()).thenReturn(mFuseboxSessionState);
        when(mLocationBarDataProvider.getDefaultRequestType())
                .thenReturn(AutocompleteRequestType.SEARCH);
        when(mFuseboxSessionState.getAutocompleteInput()).thenReturn(mAutocompleteInput);
        when(mAutocompleteInput.getRequestTypeSupplier()).thenReturn(mRequestTypeSupplier);
        when(mAutocompleteInput.getSiteSearchDataSupplier()).thenReturn(mSiteSearchDataSupplier);
        when(mAutocompleteInput.getRequestType()).thenAnswer(inv -> mRequestTypeSupplier.get());
        when(mAutocompleteInput.getSiteSearchData())
                .thenAnswer(inv -> mSiteSearchDataSupplier.get());
        when(mAutocompleteInput.getPageUrl()).thenReturn(GURL.emptyGURL());
        when(mAutocompleteInput.getPageTitle()).thenReturn("");
        FuseboxSessionState.setInstanceForTesting(mFuseboxSessionState);
        mSearchEngineServiceSupplier.set(mSearchEngineService);

        verify(mSearchEngineService)
                .addSearchEngineNameObserver(mSearchEngineNameObserverCaptor.capture());
        mSearchEngineNameObserver = mSearchEngineNameObserverCaptor.getValue();

        mUpdater.beginInput(mAutocompleteInput);
        clearInvocations(mUpdateHintTextCallback);
    }

    @Test
    public void testDefaultEnabledBehavior() {
        when(mSearchEngineService.getSearchEngineName()).thenReturn("Google");

        mSearchEngineNameObserver.onSearchEngineNameChanged();

        verify(mUpdateHintTextCallback).onResult(mHintTextCaptor.capture());
        assertEquals("Search Google or type URL", mHintTextCaptor.getValue());
    }

    @Test
    public void testUpdateHintText_EmbedderControlledHint_DoesNotUpdate() {
        when(mEmbedderUiOverrides.isEmbedderControlledHint()).thenReturn(true);

        mSearchEngineNameObserver.onSearchEngineNameChanged();

        verify(mUpdateHintTextCallback, never()).onResult(any());
    }

    @Test
    public void testUpdateHintText_SiteSearchActive_HidesHintText() {
        mSiteSearchDataSupplier.set(new SiteSearchData("keyword", "Search keyword"));

        verify(mUpdateHintTextCallback).onResult(eq(""));
    }

    @Test
    public void testGetOmniboxHintText_ContextualTasks() {
        GURL aiUrl = new GURL("chrome://contextual-tasks");
        String aiTitle = "My AI Page";
        when(mAutocompleteInput.getPageUrl()).thenReturn(aiUrl);
        when(mAutocompleteInput.getPageTitle()).thenReturn(aiTitle);

        mUpdater.onTitleChanged();

        verify(mUpdateHintTextCallback).onResult(eq(aiTitle));
    }

    @Test
    @SuppressWarnings("unchecked")
    public void testGetOmniboxHintText_FuseboxSessionState() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        when(mSearchEngineService.getSearchEngineName()).thenReturn("Google");

        String searchEngineHint = "Search Google or type URL";
        String aiModeHint = "Ask anything";
        String imageGenHint = "Image Gen Tool Hint";
        String deepSearchHint = "Deep Search Tool Hint";
        String canvasHint = "Canvas Tool Hint";

        when(mFuseboxSessionState.getComposeboxQueryControllerBridge())
                .thenReturn(mComposeboxQueryControllerBridge);
        when(mComposeboxQueryControllerBridge.getInputStateSupplier())
                .thenReturn(mInputStateSupplier);

        ToolConfig aiModeConfig =
                ToolConfig.newBuilder()
                        .setTool(ToolMode.TOOL_MODE_UNSPECIFIED)
                        .setHintText(aiModeHint)
                        .build();
        ToolConfig imageGenConfig =
                ToolConfig.newBuilder()
                        .setTool(ToolMode.TOOL_MODE_IMAGE_GEN)
                        .setHintText(imageGenHint)
                        .build();
        ToolConfig deepSearchConfig =
                ToolConfig.newBuilder()
                        .setTool(ToolMode.TOOL_MODE_DEEP_SEARCH)
                        .setHintText(deepSearchHint)
                        .build();
        ToolConfig canvasConfig =
                ToolConfig.newBuilder()
                        .setTool(ToolMode.TOOL_MODE_CANVAS)
                        .setHintText(canvasHint)
                        .build();
        byte[][] toolConfigs =
                new byte[][] {
                    aiModeConfig.toByteArray(),
                    imageGenConfig.toByteArray(),
                    deepSearchConfig.toByteArray(),
                    canvasConfig.toByteArray()
                };
        // Test InputState is null (supplier starts as null).
        mUpdater.onTitleChanged();
        verify(mUpdateHintTextCallback).onResult(eq(searchEngineHint));
        clearInvocations(mUpdateHintTextCallback);

        InputState inputState = new InputState.Builder().withToolConfigs(toolConfigs).build();
        mInputStateSupplier.set(inputState);

        mRequestTypeSupplier.set(AutocompleteRequestType.IMAGE_GENERATION);
        verify(mUpdateHintTextCallback).onResult(eq(imageGenHint));

        clearInvocations(mUpdateHintTextCallback);
        mRequestTypeSupplier.set(AutocompleteRequestType.DEEP_SEARCH);
        verify(mUpdateHintTextCallback).onResult(eq(deepSearchHint));

        clearInvocations(mUpdateHintTextCallback);
        mRequestTypeSupplier.set(AutocompleteRequestType.CANVAS);
        verify(mUpdateHintTextCallback).onResult(eq(canvasHint));

        clearInvocations(mUpdateHintTextCallback);
        mRequestTypeSupplier.set(AutocompleteRequestType.AI_MODE);
        verify(mUpdateHintTextCallback).onResult(eq(aiModeHint));

        clearInvocations(mUpdateHintTextCallback);
        OmniboxFeatures.sShowModelPicker.setForTesting(false);
        mRequestTypeSupplier.set(AutocompleteRequestType.DEEP_SEARCH);
        verify(mUpdateHintTextCallback).onResult(eq(searchEngineHint));
        OmniboxFeatures.sShowModelPicker.setForTesting(true);

        clearInvocations(mUpdateHintTextCallback);
        when(mFuseboxSessionState.getComposeboxQueryControllerBridge()).thenReturn(null);
        mUpdater.onTitleChanged();
        verify(mUpdateHintTextCallback).onResult(eq(searchEngineHint));
        when(mFuseboxSessionState.getComposeboxQueryControllerBridge())
                .thenReturn(mComposeboxQueryControllerBridge);

        clearInvocations(mUpdateHintTextCallback);
        InputState emptyHintState = new InputState.Builder().withHintText("").build();
        mInputStateSupplier.set(emptyHintState);
        mUpdater.onTitleChanged();
        verify(mUpdateHintTextCallback).onResult(eq(searchEngineHint));
    }

    @Test
    @SuppressWarnings("unchecked")
    public void testGetOmniboxHintText_ModelPickerDisabled() {
        when(mSearchEngineService.getSearchEngineName()).thenReturn("Google");
        when(mSearchEngineService.isDefaultSearchEngineGoogle()).thenReturn(true);

        clearInvocations(mUpdateHintTextCallback);
        mSearchEngineNameObserver.onSearchEngineNameChanged();
        verify(mUpdateHintTextCallback).onResult(eq("Search Google or type URL"));

        clearInvocations(mUpdateHintTextCallback);
        mRequestTypeSupplier.set(AutocompleteRequestType.AI_MODE);
        verify(mUpdateHintTextCallback).onResult(eq("Ask anything"));

        clearInvocations(mUpdateHintTextCallback);
        mRequestTypeSupplier.set(AutocompleteRequestType.IMAGE_GENERATION);
        verify(mUpdateHintTextCallback).onResult(eq("Describe your image"));
    }

    @Test
    @SuppressWarnings("unchecked")
    public void testGetOmniboxHintText_UseAskHintForNtp() {
        when(mSearchEngineService.getSearchEngineName()).thenReturn("Google");
        when(mSearchEngineService.isDefaultSearchEngineGoogle()).thenReturn(true);

        clearInvocations(mUpdateHintTextCallback);
        OmniboxFeatures.sUseAskHintForNtp.setForTesting(false);
        mUpdater.onTitleChanged();
        verify(mUpdateHintTextCallback).onResult(eq("Search Google or type URL"));

        clearInvocations(mUpdateHintTextCallback);
        OmniboxFeatures.sUseAskHintForNtp.setForTesting(true);
        mUpdater.onTitleChanged();
        verify(mUpdateHintTextCallback).onResult(eq("Ask Google or type URL"));

        clearInvocations(mUpdateHintTextCallback);
        when(mSearchEngineService.getSearchEngineName()).thenReturn("Yahoo");
        when(mSearchEngineService.isDefaultSearchEngineGoogle()).thenReturn(false);
        mSearchEngineNameObserver.onSearchEngineNameChanged();
        verify(mUpdateHintTextCallback).onResult(eq("Search Yahoo or type URL"));

        OmniboxFeatures.sUseAskHintForNtp.setForTesting(false);
    }
}
