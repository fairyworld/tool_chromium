// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/ai_mode_button_config.h"

#include "components/search_engines/search_engine_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ai_mode_button_config {

TEST(AiModeButtonConfigTest, GoogleConfigIsValid) {
  AiModeButtonConfig config;
  config.id = SearchEngineType::SEARCH_ENGINE_GOOGLE;
  // Google config should be valid even if other fields are empty.
  EXPECT_TRUE(config.IsValid());

  // Initially empty config should be invalid.
  config.id = SearchEngineType::SEARCH_ENGINE_BING;
  EXPECT_FALSE(config.IsValid());

  // Fill in all required fields.
  config.text = u"Bing AI";
  config.placeholder_text = u"Bing placeholder";
  config.tooltip = u"Bing tooltip";
  config.a11y_label = u"Bing a11y";
  config.context_menu_label = u"Bing menu";
  config.navigation_url = "https://bing.com/search?q={searchTerms}";
  config.navigation_url_empty = "https://bing.com/chat";
  config.favicon_url = "https://bing.com/favicon.ico";
  EXPECT_TRUE(config.IsValid());

  // Test individual fields missing.
  {
    AiModeButtonConfig config_copy = config;
    config_copy.text.clear();
    EXPECT_FALSE(config_copy.IsValid());
  }
  {
    AiModeButtonConfig config_copy = config;
    config_copy.placeholder_text.clear();
    EXPECT_FALSE(config_copy.IsValid());
  }
  {
    AiModeButtonConfig config_copy = config;
    config_copy.tooltip.clear();
    EXPECT_FALSE(config_copy.IsValid());
  }
  {
    AiModeButtonConfig config_copy = config;
    config_copy.a11y_label.clear();
    EXPECT_FALSE(config_copy.IsValid());
  }
  {
    AiModeButtonConfig config_copy = config;
    config_copy.context_menu_label.clear();
    EXPECT_FALSE(config_copy.IsValid());
  }
  {
    AiModeButtonConfig config_copy = config;
    config_copy.navigation_url.clear();
    EXPECT_FALSE(config_copy.IsValid());
  }
  {
    AiModeButtonConfig config_copy = config;
    config_copy.navigation_url_empty.clear();
    EXPECT_FALSE(config_copy.IsValid());
  }
  {
    AiModeButtonConfig config_copy = config;
    config_copy.favicon_url.clear();
    EXPECT_FALSE(config_copy.IsValid());
  }

  // Test string fields too long.
  {
    AiModeButtonConfig config_copy = config;
    config_copy.text = std::u16string(17, 'a');
    EXPECT_FALSE(config_copy.IsValid());
  }
  {
    AiModeButtonConfig config_copy = config;
    config_copy.placeholder_text = std::u16string(65, 'a');
    EXPECT_FALSE(config_copy.IsValid());
  }
  {
    AiModeButtonConfig config_copy = config;
    config_copy.tooltip = std::u16string(65, 'a');
    EXPECT_FALSE(config_copy.IsValid());
  }
  {
    AiModeButtonConfig config_copy = config;
    config_copy.a11y_label = std::u16string(65, 'a');
    EXPECT_FALSE(config_copy.IsValid());
  }
  {
    AiModeButtonConfig config_copy = config;
    config_copy.context_menu_label = std::u16string(65, 'a');
    EXPECT_FALSE(config_copy.IsValid());
  }

  // Test invalid urls.
  {
    AiModeButtonConfig config_copy = config;
    config_copy.navigation_url = "bad";
    EXPECT_FALSE(config_copy.IsValid());
  }
  {
    AiModeButtonConfig config_copy = config;
    config_copy.navigation_url_empty = "bad";
    EXPECT_FALSE(config_copy.IsValid());
  }
  {
    AiModeButtonConfig config_copy = config;
    config_copy.favicon_url = "bad";
    EXPECT_FALSE(config_copy.IsValid());
  }

  // Test missing search placeholder in `navigation_url`.
  {
    AiModeButtonConfig config_copy = config;
    config_copy.navigation_url = "https://bing.com/search?q=no_placeholder";
    EXPECT_FALSE(config_copy.IsValid());
  }
}

}  // namespace ai_mode_button_config
