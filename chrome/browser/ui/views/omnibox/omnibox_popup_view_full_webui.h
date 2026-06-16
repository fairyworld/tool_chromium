// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_FULL_WEBUI_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_FULL_WEBUI_H_

#include <optional>

#include "base/callback_list.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"
#include "content/public/browser/web_contents_observer.h"

class LocationBar;
class OmniboxController;
class OmniboxView;
class OmniboxPopupPresenterDelegate;
class OmniboxPopupHandler;

class OmniboxPopupViewFullWebUI : public OmniboxPopupViewWebUI,
                                  public content::WebContentsObserver {
 public:
  OmniboxPopupViewFullWebUI(OmniboxView* omnibox_view,
                            OmniboxController* controller,
                            LocationBar* location_bar,
                            OmniboxPopupPresenterDelegate& presenter_delegate);
  OmniboxPopupViewFullWebUI(const OmniboxPopupViewFullWebUI&) = delete;
  OmniboxPopupViewFullWebUI& operator=(const OmniboxPopupViewFullWebUI&) =
      delete;
  ~OmniboxPopupViewFullWebUI() override;

  // OmniboxPopupView:
  // Pushes the current permanent display text (e.g. a URL) to the WebUI on
  // focus or if the text changed.
  void UpdatePopupAppearance() override;
  // Saves the current omnibox state (e.g. input) to the given tab's
  // user data, so it can be restored when switching back to this tab.
  void SaveStateToTab(content::WebContents* tab) override;
  // Pushes the current text to the WebUI.
  void PushTextToWebUI(bool is_double_click) override;
  // Called when the active tab changes.
  void OnTabChanged(content::WebContents* contents) override;
  // Called when the omnibox gains focus.
  void OnFocus() override;

  bool is_switching_tab() const override;

  // Called when the WebUI page handler is bound.
  void OnWebUIPopupHandlerBound();

 private:
  // Updates the popup state and pushes the current text to the WebUI if the
  // state is set to `kFull`. Enforces the order of operations to minimize
  // text flickers.
  void UpdatePopupStateAndContent(OmniboxPopupState state);

  // Gets the OmniboxPopupHandler associated with this view's WebUI.
  OmniboxPopupHandler* GetPopupHandler();

  // Callback when window-wide popup state changes.
  void OnPopupStateChanged(OmniboxPopupState old_state,
                           OmniboxPopupState new_state);

  // Caches last text pushed to WebUI to prevent redundant Mojo IPCs.
  // Set to std::nullopt on tab switch or popup close to force next update.
  std::optional<std::u16string> last_sent_text_;
  // True during tab switch. Prevents OmniboxPopupFullPresenter from closing
  // the popup widget on deactivation.
  bool is_switching_tab_ = false;
  // Restores selection range exactly once after a tab switch. Set in
  // OnTabChanged, consumed and cleared in PushTextToWebUI.
  std::optional<gfx::Range> tab_switch_selection_;
  // Subscription to window-wide popup state changes.
  base::CallbackListSubscription popup_state_subscription_;
  // Caches WebUI selection before popup closes, preserving it if the tab
  // deactivates after popup closure but before SaveStateToTab runs.
  std::optional<gfx::Range> pre_deactivation_selection_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_FULL_WEBUI_H_
