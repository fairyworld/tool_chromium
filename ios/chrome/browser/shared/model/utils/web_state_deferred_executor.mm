// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/utils/web_state_deferred_executor.h"

#import "base/memory/weak_ptr.h"
#import "ios/web/public/navigation/navigation_manager.h"

@implementation WebStateDeferredExecutor {
  // Observer for the web state loading.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  // Stores the callbacks to be used once the web state is loaded.
  std::unordered_map<web::WebStateID, WebStateLoadedCompletionBlock>
      _loadedCallbacks;
  // Stores the callbacks to be used once the web state is realized.
  std::unordered_map<web::WebStateID, WebStateRealizedCompletionBlock>
      _realizedCallbacks;
  // Temporarily stores the active observations.
  std::unordered_map<web::WebStateID, base::WeakPtr<web::WebState>>
      _activeObservations;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
  }

  return self;
}

- (void)ensureWebStateIsLoaded:(web::WebState*)webState
                withCompletion:(WebStateLoadedCompletionBlock)completion {
  _loadedCallbacks[webState->GetUniqueIdentifier()] = completion;
  BOOL realized = webState->IsRealized();

  if (!realized) {
    [self observeWebState:webState];
    [self forceRealizeWebState:webState];
  }

  // Ensure the web state is actually loading/loaded by triggering restoration
  // load.
  if (webState->GetNavigationManager()) {
    webState->GetNavigationManager()->LoadIfNecessary();
  }

  [self.delegate webStateDeferredExecutor:self willLoadWebState:webState];

  __weak __typeof(self) weakSelf = self;
  base::WeakPtr<web::WebState> weakWebState = webState->GetWeakPtr();

  _loadedCallbacks[webState->GetUniqueIdentifier()] =
      ^(web::WebState* innerWebState, BOOL success) {
        [weakSelf.delegate webStateDeferredExecutor:weakSelf
                                    didLoadWebState:innerWebState
                                            success:success];
        if (completion) {
          completion(innerWebState, success);
        }
      };

  if (webState->IsLoading()) {
    [self observeWebState:webState];
    return;
  }

  // Already loaded.
  [self webStateLoaded:webState success:YES];
}

- (void)ensureWebStateIsRealized:(web::WebState*)webState
                  withCompletion:(WebStateRealizedCompletionBlock)completion {
  BOOL realized = webState->IsRealized();

  if (realized) {
    _realizedCallbacks[webState->GetUniqueIdentifier()] = completion;
    [self invokeRealizedCallbacksForWebState:webState];
    return;
  }

  [self.delegate webStateDeferredExecutor:self
                 willForceRealizeWebState:webState];

  __weak __typeof(self) weakSelf = self;
  base::WeakPtr<web::WebState> weakWebState = webState->GetWeakPtr();

  _realizedCallbacks[webState->GetUniqueIdentifier()] =
      ^(web::WebState* innerWebState) {
        [weakSelf.delegate webStateDeferredExecutor:weakSelf
                            didForceRealizeWebState:innerWebState];
        if (completion) {
          completion(innerWebState);
        }
      };
  [self observeWebState:webState];
  [self forceRealizeWebState:webState];
}

#pragma mark - Private

- (void)observeWebState:(web::WebState*)webState {
  BOOL alreadyObserving =
      _activeObservations.find(webState->GetUniqueIdentifier()) !=
      _activeObservations.end();
  if (alreadyObserving) {
    return;
  }
  webState->AddObserver(_webStateObserverBridge.get());
  _activeObservations[webState->GetUniqueIdentifier()] = webState->GetWeakPtr();
}

- (void)removeObserverForWebState:(web::WebState*)webState {
  webState->RemoveObserver(_webStateObserverBridge.get());
  _activeObservations.erase(webState->GetUniqueIdentifier());
}

- (void)forceRealizeWebState:(web::WebState*)webState {
  web::IgnoreOverRealizationCheck();
  webState->ForceRealized();
}

- (void)webStateLoaded:(web::WebState*)webState success:(BOOL)success {
  const web::WebStateID webStateID = webState->GetUniqueIdentifier();
  if (auto block = _loadedCallbacks[webStateID]) {
    _loadedCallbacks.erase(webStateID);
    block(webState, success);
  }
}

- (void)invokeRealizedCallbacksForWebState:(web::WebState*)webState {
  const web::WebStateID webStateID = webState->GetUniqueIdentifier();
  if (auto block = _realizedCallbacks[webStateID]) {
    _realizedCallbacks.erase(webStateID);
    block(webState);
  }
}

- (void)removeRemainingWebStateObservations {
  std::vector<base::WeakPtr<web::WebState>> remainingObservedWebStates;
  remainingObservedWebStates.reserve(_activeObservations.size());

  for (auto kv : _activeObservations) {
    remainingObservedWebStates.push_back(kv.second);
  }

  for (base::WeakPtr<web::WebState> weakWebState : remainingObservedWebStates) {
    web::WebState* webState = weakWebState.get();
    if (webState) {
      [self removeObserverForWebState:webState];
    }
  }
}

- (void)dealloc {
  [self removeRemainingWebStateObservations];
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  [self removeObserverForWebState:webState];
  [self webStateLoaded:webState success:success];
}

- (void)webStateRealized:(web::WebState*)webState {
  if (!_loadedCallbacks.contains(webState->GetUniqueIdentifier())) {
    [self removeObserverForWebState:webState];
  }
  [self invokeRealizedCallbacksForWebState:webState];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  [self removeObserverForWebState:webState];
  [self invokeRealizedCallbacksForWebState:webState];
  [self webStateLoaded:webState success:NO];
}

@end
