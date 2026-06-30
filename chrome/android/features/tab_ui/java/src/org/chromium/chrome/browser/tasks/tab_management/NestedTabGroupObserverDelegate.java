// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.components.tab_group_sync.EitherId.EitherGroupId;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * {@link TabListMediator.TabListLayoutType#NESTED} implementation of {@link
 * TabGroupObserverDelegate}.
 */
@NullMarked
class NestedTabGroupObserverDelegate extends TabGroupObserverDelegate {
    NestedTabGroupObserverDelegate(TabListMediator mediator, TabListModel modelList) {
        super(mediator, modelList);
    }

    @Override
    public void didChangeTabGroupColor(Token tabGroupId, @TabGroupColorId int newColor) {
        super.didChangeTabGroupColor(tabGroupId, newColor);
        // Sync the color down to the child models so decorations (like the group spine) can
        // read it.
        updateColorForChildTabsInNestedLayout(tabGroupId, newColor);
    }

    @Override
    public void didChangeTabGroupCollapsed(Token tabGroupId, boolean isCollapsed, boolean animate) {
        int headerIndex = mModelList.indexFromTabGroupId(tabGroupId);
        if (headerIndex == TabModel.INVALID_TAB_INDEX) return;
        PropertyModel model = mModelList.get(headerIndex).model;

        if (isCollapsed == Boolean.TRUE.equals(model.get(TabProperties.IS_COLLAPSED))) {
            return;
        }

        model.set(TabProperties.IS_COLLAPSED, isCollapsed);

        if (isCollapsed) {
            removeChildTabs(tabGroupId);
        } else {
            mMediator.insertChildTabs(tabGroupId, headerIndex);
        }
    }

    @Override
    public void didMoveTabOutOfGroup(Tab movedTab, int prevFilterIndex) {
        TabModel tabModel = mMediator.getCurrentTabModelChecked();
        Tab previousGroupTab = tabModel.getRepresentativeTabAt(prevFilterIndex);
        assumeNonNull(previousGroupTab);

        Token oldTabGroupId = previousGroupTab.getTabGroupId();
        mMediator.updateTabGroupHeaderId(oldTabGroupId);
        syncChildTab(movedTab, oldTabGroupId);
    }

    @Override
    public void didMergeTabToGroup(Tab movedTab, boolean isDestinationTab) {
        syncChildTab(movedTab, /* oldTabGroupId= */ null);
    }

    /**
     * Updates the UI properties and positioning of a child tab in the NESTED layout when its group
     * membership changes.
     *
     * @param tab The tab whose group state is being updated.
     * @param oldTabGroupId The previous group ID of the tab, if any.
     */
    private void syncChildTab(Tab tab, @Nullable Token oldTabGroupId) {
        int srcIndex = mModelList.indexFromTabId(tab.getId());

        Token newTabGroupId = tab.getTabGroupId();
        if (oldTabGroupId == null && srcIndex != TabModel.INVALID_TAB_INDEX) {
            oldTabGroupId = mModelList.get(srcIndex).model.get(TabProperties.TAB_GROUP_ID);
        }

        int desIndex = mMediator.getInsertionIndexOfTabForNestedLayout(tab);

        if (srcIndex == TabModel.INVALID_TAB_INDEX && desIndex != TabModel.INVALID_TAB_INDEX) {
            // Tab is moving out of a collapsed group.
            TabModel tabModel = mMediator.getCurrentTabModelChecked();
            int currentTabId = TabModelUtils.getCurrentTabId(tabModel);
            mMediator.addTabInfoToModelForTab(tab, desIndex, currentTabId == tab.getId());
        } else if (srcIndex != TabModel.INVALID_TAB_INDEX
                && desIndex == TabModel.INVALID_TAB_INDEX) {
            // Tab is moving into a collapsed group.
            mModelList.removeAt(srcIndex);
        } else if (srcIndex != TabModel.INVALID_TAB_INDEX) {
            PropertyModel model = mModelList.get(srcIndex).model;
            mMediator.setupGroupPropertiesForChildTab(tab, model);

            mMediator.bindTabActionStateProperties(mMediator.getTabActionState(), tab, model);

            if (srcIndex != desIndex) {
                mModelList.move(srcIndex, desIndex);
            }

            if (newTabGroupId != null) {
                int newTabUiIndex = mModelList.indexFromTabId(tab.getId());
                mMediator.ensureGroupHeaderExistsInNestedLayout(tab, newTabGroupId, newTabUiIndex);
            }
        }

        if (newTabGroupId != null) {
            mMediator.updateTabGroupTitle(newTabGroupId);
        }
        if (oldTabGroupId != null && !oldTabGroupId.equals(newTabGroupId)) {
            mMediator.updateTabGroupTitle(oldTabGroupId);
        }
    }

    private void removeChildTabs(Token tabGroupId) {
        int headerIndex = mModelList.indexFromTabGroupId(tabGroupId);
        if (headerIndex == TabModel.INVALID_TAB_INDEX) return;
        TabModel tabModel = mMediator.getCurrentTabModelChecked();
        int childCount = tabModel.getTabsInGroup(tabGroupId).size();

        for (int i = 0; i < childCount; i++) {
            if (headerIndex + 1 < mModelList.size()) {
                mModelList.removeAt(headerIndex + 1);
            }
        }
    }

    /**
     * Updates the UI properties of child tabs when their group color changes.
     *
     * @param tabGroupId The ID of the tab group.
     * @param newColor The new color of the tab group.
     */
    private void updateColorForChildTabsInNestedLayout(
            Token tabGroupId, @TabGroupColorId int newColor) {
        // TODO(crbug.com/509226293): Consider defining a method in TabListModel that fetches all
        // child tabs in a group.
        boolean foundGroup = false;
        for (int i = 0; i < mModelList.size(); i++) {
            PropertyModel childModel = mModelList.get(i).model;
            if (childModel.get(CARD_TYPE) == TAB
                    && tabGroupId.equals(childModel.get(TabProperties.TAB_GROUP_ID))) {
                mMediator.updateTabGroupColorViewProvider(
                        EitherGroupId.createLocalId(new LocalTabGroupId(tabGroupId)),
                        childModel,
                        newColor);
                foundGroup = true;
            } else if (foundGroup) {
                break;
            }
        }
    }
}
