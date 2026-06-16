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
                                                      controller)) {
  // Listen for WebUI page handler binding to push initial states.
  presenter()->SetHandlerBoundCallback(
      base::BindRepeating(&OmniboxPopupViewFullWebUI::OnWebUIPopupHandlerBound,
                          base::Unretained(this)));
  // Monitor window-wide popup state changes to cache selection ranges.
  popup_state_subscription_ =
      controller->popup_state_manager()->AddPopupStateChangedCallback(
          base::BindRepeating(&OmniboxPopupViewFullWebUI::OnPopupStateChanged,
                              base::Unretained(this)));
}

OmniboxPopupViewFullWebUI::~OmniboxPopupViewFullWebUI() {
  if (presenter()) {
    // Avoid calling OnWebUIPopupHandlerBound on destroyed view.
    presenter()->SetHandlerBoundCallback(base::NullCallback());
  }
}

void OmniboxPopupViewFullWebUI::UpdatePopupAppearance() {
  OmniboxPopupState current_state =
      controller()->popup_state_manager()->popup_state();
  if (current_state != OmniboxPopupState::kFull) {
    return;
  }

  OmniboxEditModel* edit_model = controller()->edit_model();
  if (edit_model->user_input_in_progress()) {
    // User is typing. Update the cache to match what the WebUI already has
    // so we don't get stale cache desyncs.
    last_sent_text_ = edit_model->user_text();
  } else {
    // Reverted or permanent text. Push it to WebUI.
    PushTextToWebUI(false);
  }
}

void OmniboxPopupViewFullWebUI::PushTextToWebUI(bool is_double_click) {
  // Suppress updates during tab switch to prevent race conditions/flicker.
  // Correct state will be pushed once the tab switch completes.
  if (is_switching_tab_) {
    return;
  }

  OmniboxEditModel* edit_model = controller()->edit_model();
  edit_model->ResetDisplayTexts();

  OmniboxPopupHandler* popup_handler = GetPopupHandler();
  if (!popup_handler) {
    return;
  }

  // Use draft text if input is in progress, otherwise permanent page text.
  const bool user_input_in_progress = edit_model->user_input_in_progress();
  const std::u16string text = user_input_in_progress
                                  ? edit_model->user_text()
                                  : edit_model->GetPermanentDisplayText();

  gfx::Range saved_selection = gfx::Range(0, text.length());
  if (!last_sent_text_.has_value()) {
    // Restore selection range saved during tab switch.
    if (tab_switch_selection_.has_value()) {
      saved_selection = *tab_switch_selection_;
    }
  } else if (user_input_in_progress) {
    // Use WebUI's cached selection to preserve cursor position.
    saved_selection = popup_handler->latest_selection();
  }

  const std::string text_utf8 = base::UTF16ToUTF8(text);
  const std::string full_url =
      base::UTF16ToUTF8(controller()->client()->GetFormattedFullURL());

  if (is_double_click) {
    gfx::Range views_selection;
    if (auto* omnibox_view_views =
            static_cast<OmniboxViewViews*>(omnibox_view_)) {
      views_selection = omnibox_view_views->GetSelectedRange();
    }
    if (saved_selection != views_selection) {
      // On double-click, push views selection to WebUI without rewriting text,
      // preventing DOM input/scroll state reset.
      popup_handler->SetInputState(text_utf8, views_selection,
                                   user_input_in_progress, is_double_click,
                                   full_url);
    }
  }

  // Only push if text changed or if restoring selection on tab switch.
  const bool text_changed =
      !last_sent_text_.has_value() || text != *last_sent_text_;
  if (text_changed || tab_switch_selection_.has_value()) {
    popup_handler->SetInputState(text_utf8, saved_selection,
                                 user_input_in_progress, is_double_click,
                                 full_url);
  }

  // Clear tab-switch selection and cache the last sent text.
  tab_switch_selection_ = std::nullopt;
  last_sent_text_ = text;
}

void OmniboxPopupViewFullWebUI::SaveStateToTab(content::WebContents* tab) {
  DCHECK(tab);
  // Mark that a tab switch is starting. This prevents widget closure when the
  // browser deactivates the current tab's window context.
  is_switching_tab_ = true;

  auto* edit_model = controller()->edit_model();
  const OmniboxEditModel::State default_state =
      edit_model->GetStateForTabSwitch();
  std::unique_ptr<OmniboxEditModel::State> state;

  // Override `user_text` to prevent GetStateForTabSwitch() (which polls the
  // inactive Views textfield) from overwriting in-progress WebUI drafts.
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

  // Fetch selection directly from WebUI handler since Views doesn't track it.
  gfx::Range selection;
  bool has_handler = GetPopupHandler() != nullptr;
  gfx::Range handler_selection = has_handler
                                     ? GetPopupHandler()->latest_selection()
                                     : gfx::Range::InvalidRange();
  bool is_active_tab = tab == web_contents();
  if (is_active_tab) {
    // Use cached pre-deactivation selection if popup already closed;
    // otherwise, retrieve it from the handler.
    if (pre_deactivation_selection_.has_value()) {
      selection = *pre_deactivation_selection_;
    } else if (controller()->popup_state_manager()->popup_state() ==
                   OmniboxPopupState::kFull &&
               has_handler) {
      selection = handler_selection;
    }
  }
  pre_deactivation_selection_ = std::nullopt;

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
  Observe(contents);
  // Reset cache to force pushing the restored tab's state.
  last_sent_text_.reset();
  pre_deactivation_selection_ = std::nullopt;

  OmniboxPopupState target_popup_state;
  auto* state = static_cast<OmniboxState*>(
      contents->GetUserData(OmniboxTabHelper::kOmniboxStateKey));
  // TODO(b/523277158): Make sure that when the user manually closes
  // the omnibox, no state is restored for it when the tab is switched back to.
  if (state) {
    // Restore state to edit model and cache selection for PushTextToWebUI.
    controller()->edit_model()->RestoreState(&state->model_state);
    target_popup_state = (state->model_state.user_input_in_progress ||
                          state->selection != gfx::Range(0, 0))
                             ? OmniboxPopupState::kFull
                             : OmniboxPopupState::kNone;
    tab_switch_selection_ = state->selection;
  } else {
    // No saved state: revert to permanent state.
    controller()->edit_model()->Revert();
    controller()->edit_model()->OnChanged();
    target_popup_state = controller()->edit_model()->has_focus()
                             ? OmniboxPopupState::kFull
                             : OmniboxPopupState::kNone;
    tab_switch_selection_ = std::nullopt;
  }

  // Resume outgoing updates and apply restored state.
  is_switching_tab_ = false;
  UpdatePopupStateAndContent(target_popup_state);
  pre_deactivation_selection_ = std::nullopt;
}

void OmniboxPopupViewFullWebUI::OnFocus() {
  if (is_switching_tab_) {
    return;
  }
  last_sent_text_.reset();
  presenter()->RequestFocus();
  UpdatePopupStateAndContent(OmniboxPopupState::kFull);
}

void OmniboxPopupViewFullWebUI::OnWebUIPopupHandlerBound() {
  last_sent_text_.reset();
  if (controller()->popup_state_manager()->popup_state() ==
      OmniboxPopupState::kFull) {
    PushTextToWebUI(false);
  }
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

bool OmniboxPopupViewFullWebUI::is_switching_tab() const {
  return is_switching_tab_;
}

void OmniboxPopupViewFullWebUI::OnPopupStateChanged(
    OmniboxPopupState old_state,
    OmniboxPopupState new_state) {
  // Cache selection when popup closes to preserve it in case the tab
  // deactivates afterwards.
  if (old_state == OmniboxPopupState::kFull &&
      new_state == OmniboxPopupState::kNone) {
    if (GetPopupHandler()) {
      pre_deactivation_selection_ = GetPopupHandler()->latest_selection();
    }
  }
}
