// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRITICAL_ACTIONS_CONTENT_BROWSER_CRITICAL_ACTION_FACTORY_H_
#define COMPONENTS_CRITICAL_ACTIONS_CONTENT_BROWSER_CRITICAL_ACTION_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace critical_actions {

class CriticalActionService;

// A factory that creates and manages `CriticalActionService` instances
// for each `content::BrowserContext`.
class CriticalActionFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the singleton instance of `CriticalActionFactory`.
  static CriticalActionFactory* GetInstance();

  // Retrieves the `CriticalActionService` instance associated with the given
  // `context`, creating one if it does not exist yet. Returns nullptr if
  // `context` is off-the-record (incognito).
  static CriticalActionService* GetForBrowserContext(
      content::BrowserContext* context);

  CriticalActionFactory(const CriticalActionFactory&) = delete;
  CriticalActionFactory& operator=(const CriticalActionFactory&) = delete;

 private:
  friend base::NoDestructor<CriticalActionFactory>;

  CriticalActionFactory();
  ~CriticalActionFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace critical_actions

#endif  // COMPONENTS_CRITICAL_ACTIONS_CONTENT_BROWSER_CRITICAL_ACTION_FACTORY_H_
