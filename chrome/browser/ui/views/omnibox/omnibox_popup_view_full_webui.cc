// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_full_webui.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/views/omnibox/omnibox_full_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_full_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_delegate.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_handler.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/range/range.h"

OmniboxPopupViewFullWebUI::OmniboxPopupViewFullWebUI(
    OmniboxView* omnibox_view,
    OmniboxController* controller,
    LocationBar* location_bar,
    OmniboxPopupPresenterDelegate& presenter_delegate)
    : OmniboxPopupViewWebUI(
          omnibox_view,
          controller,
          location_bar,
          presenter_delegate,
          std::make_unique<OmniboxPopupFullPresenter>(location_bar,
                                                      presenter_delegate,
                                                      controller)) {}

OmniboxPopupViewFullWebUI::~OmniboxPopupViewFullWebUI() = default;

void OmniboxPopupViewFullWebUI::UpdatePopupAppearance() {
  // Intentional no-op. Content updates are handled via PushTextToWebUI called
  // directly from specific events (focus, tab switch).
}

void OmniboxPopupViewFullWebUI::PushTextToWebUI(bool is_double_click) {
  if (is_switching_tab_) {
    return;
  }

  OmniboxEditModel* edit_model = controller()->edit_model();
  edit_model->ResetDisplayTexts();

  OmniboxPopupHandler* popup_handler = GetPopupHandler();
  if (!popup_handler) {
    return;
  }

  const bool user_input_in_progress = edit_model->user_input_in_progress();
  const std::u16string text = user_input_in_progress
                                  ? edit_model->user_text()
                                  : edit_model->GetPermanentDisplayText();

  // Determine the default selection range.
  gfx::Range saved_selection = gfx::Range(0, text.length());
  if (!last_sent_text_.has_value()) {
    // Tab switch / activation restoration.
    if (tab_switch_selection_.has_value()) {
      saved_selection = *tab_switch_selection_;
    }
  } else if (user_input_in_progress) {
    // Currently open and active steady-state or editing.
    saved_selection = popup_handler->latest_selection();
  }

  const std::string text_utf8 = base::UTF16ToUTF8(text);

  if (is_double_click) {
    gfx::Range views_selection;
    if (auto* omnibox_view_views =
            static_cast<OmniboxViewViews*>(omnibox_view_)) {
      views_selection = omnibox_view_views->GetSelectedRange();
    }
    if (saved_selection != views_selection) {
      // TODO(crbug.com/514810983): Add a dedicated IPC method for selection
      // changes (e.g. during double clicks), we do not push the input text
      // and risk resetting DOM input state or scroll position.
      popup_handler->SetInputState(text_utf8, views_selection,
                                   user_input_in_progress, is_double_click);
    }
  }

  const bool text_changed =
      !last_sent_text_.has_value() || text != *last_sent_text_;
  if (text_changed || tab_switch_selection_.has_value()) {
    popup_handler->SetInputState(text_utf8, saved_selection,
                                 user_input_in_progress, is_double_click);
  }

  tab_switch_selection_ = std::nullopt;
  last_sent_text_ = text;
}

void OmniboxPopupViewFullWebUI::SaveStateToTab(content::WebContents* tab) {
  DCHECK(tab);
  is_switching_tab_ = true;

  auto* edit_model = controller()->edit_model();
  const OmniboxEditModel::State default_state =
      edit_model->GetStateForTabSwitch();
  std::unique_ptr<OmniboxEditModel::State> state;

  // `GetStateForTabSwitch()` reads `OmniboxView::GetText()`, which polls the
  // inactive native Views textfield. We override `user_text` here to prevent
  // wiping out uncommitted WebUI drafts on tab switches.
  if (edit_model->user_input_in_progress()) {
    std::u16string override_text = edit_model->user_text();
    state = std::make_unique<OmniboxEditModel::State>(
        /*user_input_in_progress=*/true, override_text, default_state.keyword,
        default_state.keyword_placeholder, default_state.keyword_state,
        default_state.keyword_mode_entry_method, default_state.focus_state,
        default_state.autocomplete_input);
  } else {
    state = std::make_unique<OmniboxEditModel::State>(default_state);
  }

  // The text input element lives in WebUI, so the native view hierarchy does
  // not track active text selection. Fetch it directly from the WebUI handler.
  gfx::Range selection;
  if (auto* popup_handler = GetPopupHandler()) {
    selection = popup_handler->latest_selection();
  }

  // We only need to sync the active selection because the WebUI input retains
  // its selection across blur and focus. `saved_selection_for_focus_change`
  // is set to `InvalidRange` because it is never used in WebUI.
  tab->SetUserData(
      OmniboxTabHelper::kOmniboxStateKey,
      std::make_unique<OmniboxState>(
          *state, selection,
          /*saved_selection_for_focus_change=*/gfx::Range::InvalidRange()));
}

void OmniboxPopupViewFullWebUI::OnTabChanged(content::WebContents* contents) {
  last_sent_text_.reset();

  OmniboxPopupState target_popup_state;
  auto* state = static_cast<OmniboxState*>(
      contents->GetUserData(OmniboxTabHelper::kOmniboxStateKey));
  // TODO(b/523277158): Make sure that when the user manually closes
  // the omnibox, no state is restored for it when the tab is switched back to.
  if (state) {
    // Restore the saved state for the tab.
    controller()->edit_model()->RestoreState(&state->model_state);
    target_popup_state = (state->model_state.user_input_in_progress ||
                          state->selection != gfx::Range(0, 0))
                             ? OmniboxPopupState::kFull
                             : OmniboxPopupState::kNone;
    tab_switch_selection_ = state->selection;
  } else {
    // No saved state: revert to default and re-evaluate popup visibility based
    // on current focus.
    controller()->edit_model()->Revert();
    controller()->edit_model()->OnChanged();
    target_popup_state = controller()->edit_model()->has_focus()
                             ? OmniboxPopupState::kFull
                             : OmniboxPopupState::kNone;
    tab_switch_selection_ = std::nullopt;
  }

  is_switching_tab_ = false;

  // Request focus before pushing content state so our `SetInputState` IPC
  // overrides any OS-default focus selection (such as macOS Select-All),
  // and to avoid invalidating Blink layout while focus is changing.
  if (target_popup_state == OmniboxPopupState::kFull) {
    presenter()->RequestFocus();
  }

  // Update popup state and push the restored content state exactly once.
  UpdatePopupStateAndContent(target_popup_state);
}

void OmniboxPopupViewFullWebUI::OnFocus() {
  UpdatePopupStateAndContent(OmniboxPopupState::kFull);
}

void OmniboxPopupViewFullWebUI::UpdatePopupStateAndContent(
    OmniboxPopupState state) {
  if (state == OmniboxPopupState::kFull) {
    PushTextToWebUI(false);
  }
  controller()->popup_state_manager()->SetPopupState(state);
}

OmniboxPopupHandler* OmniboxPopupViewFullWebUI::GetPopupHandler() {
  if (!presenter() || !presenter()->GetWebUIContent()) {
    return nullptr;
  }
  auto* contents_wrapper = presenter()->GetWebUIContent()->contents_wrapper();
  auto* webui_controller =
      contents_wrapper ? contents_wrapper->GetWebUIController() : nullptr;
  auto* popup_ui = static_cast<OmniboxPopupUI*>(webui_controller);
  return popup_ui ? popup_ui->popup_handler() : nullptr;
}
