// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_refresh/discover_feed_provider.h"

#import "base/check_deref.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/sequence_checker.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/thread/web_thread.h"

namespace {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DiscoverFeedServiceAvailability {
  kAvailable = 0,
  kUnavailable = 1,
  kMaxValue = kUnavailable,
};
}  // namespace

@implementation DiscoverFeedProvider {
  raw_ptr<DiscoverFeedService> _service;
  SEQUENCE_CHECKER(_sequenceChecker);
}

- (instancetype)init {
  self = [super init];
  return self;
}

#pragma mark - AppRefreshProvider

- (NSString*)identifier {
  return @"Discover";
}

- (BOOL)isDue {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  DiscoverFeedService* service = [self discoverFeedService];
  if (!service) {
    return NO;
  }

  NSDate* earliestDate = service->GetEarliestBackgroundRefreshBeginDate();
  if (!earliestDate) {
    return NO;
  }

  return [earliestDate timeIntervalSinceNow] <= 0;
}

- (base::TimeDelta)refreshInterval {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  DiscoverFeedService* service = [self discoverFeedService];
  if (!service) {
    return kDiscoverFeedBackgroundRefreshNoServiceInterval.Get();
  }

  NSDate* earliestDate = service->GetEarliestBackgroundRefreshBeginDate();
  if (!earliestDate) {
    return kDiscoverFeedBackgroundRefreshNoDateInterval.Get();
  }

  base::Time earliestTime = base::Time::FromNSDate(earliestDate);
  base::TimeDelta interval = earliestTime - base::Time::Now();

  if (interval.is_negative() || interval.is_zero()) {
    return kDiscoverFeedBackgroundRefreshMinBuffer.Get();
  }

  return interval;
}

- (void)handleRefreshWithCompletion:(ProceduralBlock)completion {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  // Trigger caching of the service.
  DiscoverFeedService* service = [self discoverFeedService];

  // Log service availability.
  DiscoverFeedServiceAvailability availability =
      service ? DiscoverFeedServiceAvailability::kAvailable
              : DiscoverFeedServiceAvailability::kUnavailable;
  base::UmaHistogramEnumeration(
      "IOS.Discover.BackgroundRefresh.ServiceAvailability", availability);

  __weak __typeof(self) weakSelf = self;
  [super handleRefreshWithCompletion:^{
    [weakSelf refreshCompleted];
    if (completion) {
      completion();
    }
  }];
}

- (id<AppRefreshProviderTask>)task {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  // TODO(crbug.com/528112746): Implement AppRefreshProviderTask for discover
  // feed.
  return nil;
}

- (void)cancelRefresh {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [super cancelRefresh];
  _service = nullptr;
}

#pragma mark - Private

- (void)refreshCompleted {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _service = nullptr;
}

- (DiscoverFeedService*)discoverFeedService {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!_service) {
    _service = [self loadDiscoverFeedService];
  }
  return _service.get();
}

- (DiscoverFeedService*)loadDiscoverFeedService {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  ProfileManagerIOS* profileManager =
      GetApplicationContext()->GetProfileManager();
  PrefService* localState = GetApplicationContext()->GetLocalState();
  if (!profileManager || !localState) {
    return nullptr;
  }

  ProfileIOS* profile = nullptr;
  std::string profileName = localState->GetString(prefs::kLastUsedProfile);
  if (!profileName.empty()) {
    profile = profileManager->GetProfileWithName(profileName);
  }

  // Fallback: Use first loaded profile if last-used is missing/unloaded.
  if (!profile) {
    std::vector<ProfileIOS*> loadedProfiles =
        profileManager->GetLoadedProfiles();
    if (!loadedProfiles.empty()) {
      profile = loadedProfiles[0];
    }
  }

  if (!profile) {
    return nullptr;
  }

  // Get the original profile if in Incognito state.
  profile = profile->GetOriginalProfile();

  return DiscoverFeedServiceFactory::GetForProfile(profile);
}

@end
