// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/content/browser/critical_action_factory.h"

#include <memory>

#include "components/critical_actions/core/browser/critical_action_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace critical_actions {

class CriticalActionFactoryTest : public testing::Test {
 public:
  CriticalActionFactoryTest() = default;

 protected:
  void SetUp() override {
    BrowserContextDependencyManager::GetInstance()->MarkBrowserContextLive(
        &context_);
  }

  void TearDown() override {
    BrowserContextDependencyManager::GetInstance()
        ->DestroyBrowserContextServices(&context_);
  }

  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext context_;
};

// Verifies that GetForBrowserContext returns a valid service for a regular
// context and returns nullptr for an off-the-record context.
TEST_F(CriticalActionFactoryTest, GetForBrowserContext) {
  context_.set_is_off_the_record(false);
  CriticalActionService* service =
      CriticalActionFactory::GetForBrowserContext(&context_);
  EXPECT_NE(service, nullptr);

  // Check that the off-the-record context gets a null service.
  content::TestBrowserContext otr_context;
  otr_context.set_is_off_the_record(true);
  BrowserContextDependencyManager::GetInstance()->MarkBrowserContextLive(
      &otr_context);

  CriticalActionService* otr_service =
      CriticalActionFactory::GetForBrowserContext(&otr_context);
  EXPECT_EQ(otr_service, nullptr);

  BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(
      &otr_context);
}

// Verifies that calling GetForBrowserContext multiple times with the same
// context returns the same instance.
TEST_F(CriticalActionFactoryTest, ReturnsSameInstanceForSameContext) {
  CriticalActionService* service1 =
      CriticalActionFactory::GetForBrowserContext(&context_);
  EXPECT_NE(service1, nullptr);

  CriticalActionService* service2 =
      CriticalActionFactory::GetForBrowserContext(&context_);
  EXPECT_EQ(service1, service2);
}

// Verifies that calling GetForBrowserContext multiple times with different
// contexts returns different instances.
TEST_F(CriticalActionFactoryTest, UniqueInstancesForDifferentContexts) {
  CriticalActionService* service1 =
      CriticalActionFactory::GetForBrowserContext(&context_);
  EXPECT_NE(service1, nullptr);

  content::TestBrowserContext context2;
  BrowserContextDependencyManager::GetInstance()->MarkBrowserContextLive(
      &context2);

  CriticalActionService* service2 =
      CriticalActionFactory::GetForBrowserContext(&context2);
  EXPECT_NE(service2, nullptr);
  EXPECT_NE(service1, service2);

  BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(
      &context2);
}

}  // namespace critical_actions
