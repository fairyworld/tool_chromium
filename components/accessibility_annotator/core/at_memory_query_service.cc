// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/at_memory_query_service.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/extend.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/i18n/break_iterator.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_data_provider.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_data_type.h"
#include "components/accessibility_annotator/core/annotation_reducer/query_classifier.h"
#include "components/personal_context/core/personal_context_debug_features.h"
#include "components/personal_context/core/personal_context_service.h"
#include "components/personal_context/proto/context_memory_service.pb.h"
#include "components/personal_context/proto/features/at_memory.pb.h"
#include "components/personal_context/proto/features/common_data.pb.h"
#include "net/base/network_change_notifier.h"

namespace accessibility_annotator {

namespace {

MemoryDataType ToMemoryDataType(
    personal_context::proto::MemoryDataType data_type) {
  switch (data_type) {
    case personal_context::proto::MEMORY_DATA_TYPE_UNSPECIFIED:
      return MemoryDataType::kUnknown;
    case personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL:
      return MemoryDataType::kNameFull;
    case personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_FULL:
      return MemoryDataType::kAddressFull;
    case personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_STREET_ADDRESS:
      return MemoryDataType::kAddressStreetAddress;
    case personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_CITY:
      return MemoryDataType::kAddressCity;
    case personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_STATE:
      return MemoryDataType::kAddressState;
    case personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_ZIP:
      return MemoryDataType::kAddressZip;
    case personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_COUNTRY:
      return MemoryDataType::kAddressCountry;
    case personal_context::proto::MEMORY_DATA_TYPE_PHONE:
      return MemoryDataType::kPhone;
    case personal_context::proto::MEMORY_DATA_TYPE_EMAIL:
      return MemoryDataType::kEmail;
    case personal_context::proto::MEMORY_DATA_TYPE_COMPANY_NAME:
      return MemoryDataType::kCompanyName;
    case personal_context::proto::MEMORY_DATA_TYPE_IBAN:
      return MemoryDataType::kIban;
    case personal_context::proto::MEMORY_DATA_TYPE_IBAN_NICKNAME:
      return MemoryDataType::kIbanNickname;
    case personal_context::proto::MEMORY_DATA_TYPE_VEHICLE:
      return MemoryDataType::kVehicle;
    case personal_context::proto::MEMORY_DATA_TYPE_VEHICLE_MAKE:
      return MemoryDataType::kVehicleMake;
    case personal_context::proto::MEMORY_DATA_TYPE_VEHICLE_MODEL:
      return MemoryDataType::kVehicleModel;
    case personal_context::proto::MEMORY_DATA_TYPE_VEHICLE_YEAR:
      return MemoryDataType::kVehicleYear;
    case personal_context::proto::MEMORY_DATA_TYPE_VEHICLE_OWNER:
      return MemoryDataType::kVehicleOwner;
    case personal_context::proto::MEMORY_DATA_TYPE_VEHICLE_PLATE_NUMBER:
      return MemoryDataType::kVehiclePlateNumber;
    case personal_context::proto::MEMORY_DATA_TYPE_VEHICLE_PLATE_STATE:
      return MemoryDataType::kVehiclePlateState;
    case personal_context::proto::MEMORY_DATA_TYPE_VEHICLE_VIN:
      return MemoryDataType::kVehicleVin;
    case personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_FULL:
      return MemoryDataType::kPassportFull;
    case personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_NAME:
      return MemoryDataType::kPassportName;
    case personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_COUNTRY:
      return MemoryDataType::kPassportCountry;
    case personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_NUMBER:
      return MemoryDataType::kPassportNumber;
    case personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_ISSUE_DATE:
      return MemoryDataType::kPassportIssueDate;
    case personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_EXPIRATION_DATE:
      return MemoryDataType::kPassportExpirationDate;
    case personal_context::proto::MEMORY_DATA_TYPE_FLIGHT_RESERVATION_FULL:
      return MemoryDataType::kFlightReservationFull;
    case personal_context::proto::
        MEMORY_DATA_TYPE_FLIGHT_RESERVATION_FLIGHT_NUMBER:
      return MemoryDataType::kFlightReservationFlightNumber;
    case personal_context::proto::
        MEMORY_DATA_TYPE_FLIGHT_RESERVATION_TICKET_NUMBER:
      return MemoryDataType::kFlightReservationTicketNumber;
    case personal_context::proto::
        MEMORY_DATA_TYPE_FLIGHT_RESERVATION_CONFIRMATION_CODE:
      return MemoryDataType::kFlightReservationConfirmationCode;
    case personal_context::proto::
        MEMORY_DATA_TYPE_FLIGHT_RESERVATION_PASSENGER_NAME:
      return MemoryDataType::kFlightReservationPassengerName;
    case personal_context::proto::
        MEMORY_DATA_TYPE_FLIGHT_RESERVATION_DEPARTURE_AIRPORT:
      return MemoryDataType::kFlightReservationDepartureAirport;
    case personal_context::proto::
        MEMORY_DATA_TYPE_FLIGHT_RESERVATION_ARRIVAL_AIRPORT:
      return MemoryDataType::kFlightReservationArrivalAirport;
    case personal_context::proto::
        MEMORY_DATA_TYPE_FLIGHT_RESERVATION_DEPARTURE_DATE:
      return MemoryDataType::kFlightReservationDepartureDate;
    case personal_context::proto::
        MEMORY_DATA_TYPE_FLIGHT_RESERVATION_ARRIVAL_DATE:
      return MemoryDataType::kFlightReservationArrivalDate;
    case personal_context::proto::MEMORY_DATA_TYPE_SHIPMENT_FULL:
      return MemoryDataType::kShipmentFull;
    case personal_context::proto::MEMORY_DATA_TYPE_SHIPMENT_TRACKING_NUMBER:
      return MemoryDataType::kShipmentTrackingNumber;
    case personal_context::proto::MEMORY_DATA_TYPE_SHIPMENT_ASSOCIATED_ORDER_ID:
      return MemoryDataType::kShipmentAssociatedOrderId;
    case personal_context::proto::MEMORY_DATA_TYPE_SHIPMENT_DELIVERY_ADDRESS:
      return MemoryDataType::kShipmentDeliveryAddress;
    case personal_context::proto::MEMORY_DATA_TYPE_SHIPMENT_DELIVERY_ZIP_CODE:
      return MemoryDataType::kShipmentDeliveryZipCode;
    case personal_context::proto::MEMORY_DATA_TYPE_SHIPMENT_CARRIER_NAME:
      return MemoryDataType::kShipmentCarrierName;
    case personal_context::proto::MEMORY_DATA_TYPE_SHIPMENT_CARRIER_DOMAIN:
      return MemoryDataType::kShipmentCarrierDomain;
    case personal_context::proto::
        MEMORY_DATA_TYPE_SHIPMENT_ESTIMATED_DELIVERY_DATE:
      return MemoryDataType::kShipmentEstimatedDeliveryDate;
    case personal_context::proto::MEMORY_DATA_TYPE_NATIONAL_ID_CARD_FULL:
      return MemoryDataType::kNationalIdCardFull;
    case personal_context::proto::MEMORY_DATA_TYPE_NATIONAL_ID_CARD_NAME:
      return MemoryDataType::kNationalIdCardName;
    case personal_context::proto::MEMORY_DATA_TYPE_NATIONAL_ID_CARD_COUNTRY:
      return MemoryDataType::kNationalIdCardCountry;
    case personal_context::proto::MEMORY_DATA_TYPE_NATIONAL_ID_CARD_NUMBER:
      return MemoryDataType::kNationalIdCardNumber;
    case personal_context::proto::MEMORY_DATA_TYPE_NATIONAL_ID_CARD_ISSUE_DATE:
      return MemoryDataType::kNationalIdCardIssueDate;
    case personal_context::proto::
        MEMORY_DATA_TYPE_NATIONAL_ID_CARD_EXPIRATION_DATE:
      return MemoryDataType::kNationalIdCardExpirationDate;
    case personal_context::proto::MEMORY_DATA_TYPE_REDRESS_NUMBER_FULL:
      return MemoryDataType::kRedressNumberFull;
    case personal_context::proto::MEMORY_DATA_TYPE_REDRESS_NUMBER_NAME:
      return MemoryDataType::kRedressNumberName;
    case personal_context::proto::MEMORY_DATA_TYPE_REDRESS_NUMBER_NUMBER:
      return MemoryDataType::kRedressNumberNumber;
    case personal_context::proto::MEMORY_DATA_TYPE_KNOWN_TRAVELER_NUMBER_FULL:
      return MemoryDataType::kKnownTravelerNumberFull;
    case personal_context::proto::MEMORY_DATA_TYPE_KNOWN_TRAVELER_NUMBER_NAME:
      return MemoryDataType::kKnownTravelerNumberName;
    case personal_context::proto::MEMORY_DATA_TYPE_KNOWN_TRAVELER_NUMBER_NUMBER:
      return MemoryDataType::kKnownTravelerNumberNumber;
    case personal_context::proto::
        MEMORY_DATA_TYPE_KNOWN_TRAVELER_NUMBER_EXPIRATION_DATE:
      return MemoryDataType::kKnownTravelerNumberExpirationDate;
    case personal_context::proto::MEMORY_DATA_TYPE_DRIVERS_LICENSE_FULL:
      return MemoryDataType::kDriversLicenseFull;
    case personal_context::proto::MEMORY_DATA_TYPE_DRIVERS_LICENSE_NAME:
      return MemoryDataType::kDriversLicenseName;
    case personal_context::proto::MEMORY_DATA_TYPE_DRIVERS_LICENSE_STATE:
      return MemoryDataType::kDriversLicenseState;
    case personal_context::proto::MEMORY_DATA_TYPE_DRIVERS_LICENSE_NUMBER:
      return MemoryDataType::kDriversLicenseNumber;
    case personal_context::proto::MEMORY_DATA_TYPE_DRIVERS_LICENSE_ISSUE_DATE:
      return MemoryDataType::kDriversLicenseIssueDate;
    case personal_context::proto::
        MEMORY_DATA_TYPE_DRIVERS_LICENSE_EXPIRATION_DATE:
      return MemoryDataType::kDriversLicenseExpirationDate;
    case personal_context::proto::MEMORY_DATA_TYPE_ORDER_FULL:
      return MemoryDataType::kOrderFull;
    case personal_context::proto::MEMORY_DATA_TYPE_ORDER_ID:
      return MemoryDataType::kOrderId;
    case personal_context::proto::MEMORY_DATA_TYPE_ORDER_ACCOUNT:
      return MemoryDataType::kOrderAccount;
    case personal_context::proto::MEMORY_DATA_TYPE_ORDER_DATE:
      return MemoryDataType::kOrderDate;
    case personal_context::proto::MEMORY_DATA_TYPE_ORDER_MERCHANT_NAME:
      return MemoryDataType::kOrderMerchantName;
    case personal_context::proto::MEMORY_DATA_TYPE_ORDER_MERCHANT_DOMAIN:
      return MemoryDataType::kOrderMerchantDomain;
    case personal_context::proto::MEMORY_DATA_TYPE_ORDER_PRODUCT_NAMES:
      return MemoryDataType::kOrderProductNames;
    case personal_context::proto::MEMORY_DATA_TYPE_ORDER_GRAND_TOTAL:
      return MemoryDataType::kOrderGrandTotal;
    case personal_context::proto::MEMORY_DATA_TYPE_CREDIT_CARD_NUMBER:
      return MemoryDataType::kCreditCardNumber;
    case personal_context::proto::MEMORY_DATA_TYPE_CREDIT_CARD_EXPIRATION_DATE:
      return MemoryDataType::kCreditCardExpirationDate;
    case personal_context::proto::MEMORY_DATA_TYPE_CREDIT_CARD_SECURITY_CODE:
      return MemoryDataType::kCreditCardSecurityCode;
    case personal_context::proto::MEMORY_DATA_TYPE_CREDIT_CARD_NAME_ON_CARD:
      return MemoryDataType::kCreditCardNameOnCard;
    case personal_context::proto::MEMORY_DATA_TYPE_CREDIT_CARD_NICKNAME:
      return MemoryDataType::kCreditCardNickname;
  }
}

std::vector<MemoryEntrySource> ExtractSources(
    const personal_context::proto::AtMemorySearchResult& proto_result) {
  std::vector<MemoryEntrySource> sources;
  for (const personal_context::proto::SourceReference& proto_source :
       proto_result.sources()) {
    if (proto_source.has_gmail()) {
      std::string_view message_url = proto_source.gmail().message_url();
      sources.emplace_back(MemoryEntrySourceType::kGmail,
                           message_url.empty()
                               ? std::nullopt
                               : std::make_optional<std::string>(message_url));
    } else if (proto_source.has_photos()) {
      std::string_view photos_url = proto_source.photos().photos_url();
      sources.emplace_back(MemoryEntrySourceType::kPhotos,
                           photos_url.empty()
                               ? std::nullopt
                               : std::make_optional<std::string>(photos_url));
    }
  }
  return sources;
}

std::vector<EntryMetadata> ExtractMetadata(
    const personal_context::proto::AtMemorySearchResult& proto_result) {
  std::vector<EntryMetadata> metadata_list;
  for (const personal_context::proto::Attribute& secondary :
       proto_result.secondary_attributes()) {
    MemoryDataType other_type = MemoryDataType::kUnknown;
    std::u16string other_type_name;
    if (secondary.has_schemaful_key()) {
      other_type = ToMemoryDataType(secondary.schemaful_key());
    } else if (secondary.has_schemaless_key()) {
      other_type_name = base::UTF8ToUTF16(secondary.schemaless_key());
    }
    metadata_list.emplace_back(other_type, other_type_name,
                               base::UTF8ToUTF16(secondary.value()));
  }
  return metadata_list;
}

MemorySearchResult ConvertToMemorySearchResult(
    const personal_context::proto::AtMemorySearchResult& proto_result) {
  MemoryDataType memory_data_type = MemoryDataType::kUnknown;
  std::u16string type_name;
  std::u16string primary_value;
  if (proto_result.has_primary_attribute()) {
    const personal_context::proto::Attribute& primary =
        proto_result.primary_attribute();
    if (primary.has_schemaful_key()) {
      memory_data_type = ToMemoryDataType(primary.schemaful_key());
    } else if (primary.has_schemaless_key()) {
      type_name = base::UTF8ToUTF16(primary.schemaless_key());
    }
    primary_value = base::UTF8ToUTF16(primary.value());
  }

  MemorySearchResult pcontext_result(memory_data_type, type_name, primary_value,
                                     proto_result.relevance_score());
  pcontext_result.sources = ExtractSources(proto_result);
  pcontext_result.metadata_list = ExtractMetadata(proto_result);
  return pcontext_result;
}

std::vector<MemorySearchResult> ExtractRemoteResults(
    const personal_context::proto::AtMemoryQueryResponse& response) {
  std::vector<MemorySearchResult> remote_results;
  for (const personal_context::proto::AtMemorySearchResult& proto_result :
       response.results()) {
    MemorySearchResult pcontext_result =
        ConvertToMemorySearchResult(proto_result);
    if (!pcontext_result.value.empty()) {
      remote_results.push_back(std::move(pcontext_result));
    }
  }
  return remote_results;
}

std::vector<personal_context::proto::MemoryDataType>
GetSupportedLocalDataTypes() {
  std::vector<personal_context::proto::MemoryDataType> types;
  for (int i = personal_context::proto::MemoryDataType_MIN + 1;
       i <= personal_context::proto::MemoryDataType_MAX; ++i) {
    if (personal_context::proto::MemoryDataType_IsValid(i)) {
      types.push_back(static_cast<personal_context::proto::MemoryDataType>(i));
    }
  }
  return types;
}

personal_context::proto::AtMemoryQueryRequest BuildAtMemoryQueryRequest(
    std::u16string_view query,
    const std::string& locale) {
  personal_context::proto::AtMemoryQueryRequest request;
  request.set_input_query(base::UTF16ToUTF8(query));
  request.set_locale(locale);
  for (auto type : GetSupportedLocalDataTypes()) {
    request.add_supported_local_data_types(type);
  }
  return request;
}

// Tokenizes `text` using native word boundaries and returns true if any
// token exists in `filter_words_set`.
bool TextContainsAnyFilterWord(
    std::u16string_view text,
    const base::flat_set<std::u16string>& filter_words_set) {
  base::i18n::BreakIterator iter(text, base::i18n::BreakIterator::BREAK_WORD);
  if (!iter.Init()) {
    return false;
  }

  while (iter.Advance()) {
    if (iter.IsWord()) {
      std::u16string word = base::ToLowerASCII(iter.GetString());
      if (filter_words_set.contains(word)) {
        return true;
      }
    }
  }
  return false;
}

// Returns true if at least one word in `filter_words_set` is present in
// `entry.value` or any of its `metadata_list` values.
bool EntryMatchesAnyFilterWord(
    const MemorySearchResult& entry,
    const base::flat_set<std::u16string>& filter_words_set) {
  if (TextContainsAnyFilterWord(entry.value, filter_words_set)) {
    return true;
  }
  return std::ranges::any_of(
      entry.metadata_list, [&](const EntryMetadata& metadata) {
        return TextContainsAnyFilterWord(metadata.value, filter_words_set);
      });
}

// Deduplicates search results in `MemorySearchResults`.
// An entry is considered a duplicate if its `type`, `value` and its
// `metadata_list` are identical to an entry already in the unique set.
// The first occurrence of a duplicate entry is preserved, maintaining its
// relative order and other fields (like confidence_score). The `sources` of
// subsequent duplicates are merged into the preserved entry.
void DeduplicateResults(std::vector<MemorySearchResult>& results) {
  std::vector<MemorySearchResult> unique_results;
  unique_results.reserve(results.size());
  for (MemorySearchResult& result : results) {
    auto it = std::ranges::find_if(
        unique_results, [&result](const MemorySearchResult& existing) {
          return existing.type == result.type &&
                 existing.value == result.value &&
                 existing.metadata_list == result.metadata_list;
        });
    if (it != unique_results.end()) {
      for (MemoryEntrySource& source : result.sources) {
        if (!std::ranges::contains(it->sources, source)) {
          it->sources.push_back(std::move(source));
        }
      }
    } else {
      unique_results.push_back(std::move(result));
    }
  }
  results = std::move(unique_results);
}

std::vector<MemorySearchResult> FilterResults(
    const std::vector<MemorySearchResult>& entries,
    const base::flat_set<std::u16string>& filter_words) {
  if (filter_words.empty()) {
    return entries;
  }
  std::vector<MemorySearchResult> filtered_entries;
  filtered_entries.reserve(entries.size());
  std::ranges::copy_if(entries, std::back_inserter(filtered_entries),
                       [&](const MemorySearchResult& entry) {
                         return EntryMatchesAnyFilterWord(entry, filter_words);
                       });
  return filtered_entries;
}

MemorySearchStatus MapContextMemoryError(
    personal_context::ContextMemoryError::ExecutionError error) {
  switch (error) {
    case personal_context::ContextMemoryError::ExecutionError::
        kPermissionDenied:
    case personal_context::ContextMemoryError::ExecutionError::
        kRequestThrottled:
    case personal_context::ContextMemoryError::ExecutionError::kRetryableError:
    case personal_context::ContextMemoryError::ExecutionError::
        kNonRetryableError:
    case personal_context::ContextMemoryError::ExecutionError::kCancelled:
    case personal_context::ContextMemoryError::ExecutionError::
        kResponseParseError:
    case personal_context::ContextMemoryError::ExecutionError::kInvalidRequest:
    case personal_context::ContextMemoryError::ExecutionError::kGenericFailure:
    case personal_context::ContextMemoryError::ExecutionError::kUnknown:
      return MemorySearchStatus::kInternalFailure;
  }
}

// For debugging purposes only. Runs a debug query that directly retrieves
// local suggestions via `data_provider`, bypassing query classification and
// remote resolution.
void QueryPersonalContextDebug(
    MemoryDataProvider* data_provider,
    base::RepeatingCallback<void(MemorySearchResults)> update_callback) {
  data_provider->RetrieveAll(
      {static_cast<MemoryDataType>(
          personal_context::features::debug::kMockPersonalContextResultTypeParam
              .Get())},
      base::BindOnce(
          [](base::RepeatingCallback<void(MemorySearchResults)> update_cb,
             std::vector<MemorySearchResult> results) {
            DeduplicateResults(results);
            update_cb.Run(MemorySearchResults(
                MemorySearchStatus::kFinalResponseSuccess, std::move(results)));
          },
          std::move(update_callback)));
}

}  // namespace

AtMemoryQueryService::AtMemoryQueryService(
    std::unique_ptr<AtMemoryQueryServiceDelegate> delegate,
    std::unique_ptr<MemoryDataProvider> data_provider,
    personal_context::PersonalContextService* personal_context_service,
    const std::string& locale,
    optimization_guide::RemoteModelExecutor* remote_model_executor)
    : delegate_(std::move(delegate)),
      data_provider_(std::move(data_provider)),
      personal_context_service_(personal_context_service),
      locale_(locale),
      classifier_(CreateQueryClassifier(remote_model_executor)) {}

AtMemoryQueryService::~AtMemoryQueryService() = default;

void AtMemoryQueryService::Shutdown() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  data_provider_.reset();
  personal_context_service_ = nullptr;
}

void AtMemoryQueryService::Query(
    std::u16string_view query,
    base::RepeatingCallback<void(MemorySearchResults)> update_callback) {
  // Invalidate any in-flight queries.
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (net::NetworkChangeNotifier::IsOffline()) {
    update_callback.Run(
        MemorySearchResults(MemorySearchStatus::kNoConnectionFailure));
    return;
  }

  // We can't query if we don't have any data providers configured.
  if (!data_provider_) {
    update_callback.Run(
        MemorySearchResults(MemorySearchStatus::kInternalFailure));
    return;
  }
  if (base::FeatureList::IsEnabled(
          personal_context::features::debug::kMockPersonalContextResult)) {
    QueryPersonalContextDebug(data_provider_.get(), std::move(update_callback));
    return;
  }

  // Run the query classifier to understand the user's intent, extracting
  // intent type and filter words.
  // TODO(crbug.com/524177036): The `PersonalContextService` query should
  // return the AutofillFetchPlan, which then should be used to access the data
  // from `AutofillDataProvider`.
  classifier_.Run(
      std::u16string(query),
      base::BindOnce(&AtMemoryQueryService::OnClassificationComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::u16string(query),
                     std::move(update_callback)));
}

void AtMemoryQueryService::OnClassificationComplete(
    std::u16string query,
    base::RepeatingCallback<void(MemorySearchResults)> update_callback,
    ClassifiedQuery classified_query) {
  // If the classifier couldn't figure out what the user is asking for, we try
  // the personal context resolver as a fallback.
  if (classified_query.intent == MemoryDataType::kUnknown) {
    QueryPersonalContext(std::move(query), classified_query, update_callback,
                         /*filtered_local_entries=*/{},
                         /*fallback_local_entries=*/{});
    return;
  }

  MemoryDataType intent = classified_query.intent;

  auto callback = base::BindOnce(
      &AtMemoryQueryService::OnDataRetrieved, weak_ptr_factory_.GetWeakPtr(),
      std::move(query), std::move(classified_query), update_callback);

  auto log_and_call_retrieved = base::BindOnce(
      [](base::OnceCallback<void(std::vector<MemorySearchResult>)> callback,
         std::vector<MemorySearchResult> results) {
        base::UmaHistogramCounts1000(
            "AccessibilityAnnotator.AtMemoryQueryService."
            "ProviderResultCount.AutofillDataProvider",
            results.size());
        std::move(callback).Run(std::move(results));
      },
      std::move(callback));

  data_provider_->RetrieveAll({intent}, std::move(log_and_call_retrieved));
}

void AtMemoryQueryService::OnDataRetrieved(
    std::u16string query,
    ClassifiedQuery classified_query,
    base::RepeatingCallback<void(MemorySearchResults)> update_callback,
    std::vector<MemorySearchResult> entries) {
  DeduplicateResults(entries);
  std::vector<MemorySearchResult> filtered_entries =
      FilterResults(entries, classified_query.filter_words);

  if (!personal_context_service_) {
    update_callback.Run(MemorySearchResults(
        MemorySearchStatus::kFinalResponseSuccess,
        filtered_entries.empty() ? std::move(entries)
                                 : std::move(filtered_entries)));
    return;
  }

  if (filtered_entries.empty()) {
    QueryPersonalContext(std::move(query), std::move(classified_query),
                         update_callback,
                         /*filtered_local_entries=*/{},
                         /*fallback_local_entries=*/std::move(entries));
    return;
  }

  // Report the matching local results as a partial response.
  update_callback.Run(MemorySearchResults(
      MemorySearchStatus::kPartialResponseSuccess, filtered_entries));

  // Query the personal context resolver in the background. If it finds matches,
  // they will be merged with the filtered local entries. Otherwise, we fallback
  // to the filtered local entries.
  std::vector<MemorySearchResult> fallback_local_entries = filtered_entries;
  QueryPersonalContext(std::move(query), classified_query, update_callback,
                       std::move(filtered_entries),
                       std::move(fallback_local_entries));
}

void AtMemoryQueryService::QueryPersonalContext(
    std::u16string query,
    ClassifiedQuery classified_query,
    base::RepeatingCallback<void(MemorySearchResults)> update_callback,
    std::vector<MemorySearchResult> filtered_local_entries,
    std::vector<MemorySearchResult> fallback_local_entries) {
  if (!personal_context_service_) {
    update_callback.Run(
        MemorySearchResults(classified_query.intent == MemoryDataType::kUnknown
                                ? MemorySearchStatus::kUnsupportedQuery
                                : MemorySearchStatus::kFinalResponseSuccess,
                            std::move(fallback_local_entries)));
    return;
  }

  personal_context::proto::AtMemoryQueryRequest request_metadata =
      BuildAtMemoryQueryRequest(query, locale_);

  personal_context::ContextMemoryRequestOptions options;
  options.request_timeout = base::Seconds(30);
  personal_context_service_->FetchContext(
      personal_context::proto::CONTEXT_MEMORY_FEATURE_AT_MEMORY,
      request_metadata, options,
      base::BindOnce(&AtMemoryQueryService::OnPersonalContextQueryComplete,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(classified_query), update_callback,
                     std::move(filtered_local_entries),
                     std::move(fallback_local_entries)));
}

void AtMemoryQueryService::OnPersonalContextQueryComplete(
    ClassifiedQuery classified_query,
    base::RepeatingCallback<void(MemorySearchResults)> update_callback,
    std::vector<MemorySearchResult> filtered_local_entries,
    std::vector<MemorySearchResult> fallback_local_entries,
    personal_context::FetchContextResult result) {
  if (!result.response.has_value()) {
    personal_context::ContextMemoryError::ExecutionError error =
        result.response.error().error();
    if (error ==
        personal_context::ContextMemoryError::ExecutionError::kCancelled) {
      return;
    }
    update_callback.Run(MemorySearchResults(MapContextMemoryError(error),
                                            std::move(fallback_local_entries)));
    return;
  }

  personal_context::proto::AtMemoryQueryResponse response;
  if (!response.ParseFromString(result.response->value())) {
    update_callback.Run(
        MemorySearchResults(MemorySearchStatus::kInternalFailure,
                            std::move(fallback_local_entries)));
    return;
  }

  std::vector<MemorySearchResult> remote_results =
      ExtractRemoteResults(response);

  std::vector<MemorySearchResult> filtered_personal_context_entries =
      FilterResults(remote_results, classified_query.filter_words);

  // If the query completed successfully but returned no additional results
  // after filtering, return the fallback local entries with the final status.
  if (filtered_personal_context_entries.empty()) {
    update_callback.Run(
        MemorySearchResults(classified_query.intent == MemoryDataType::kUnknown
                                ? MemorySearchStatus::kUnsupportedQuery
                                : MemorySearchStatus::kFinalResponseSuccess,
                            std::move(fallback_local_entries)));
    return;
  }

  // In order to avoid extra allocations, remote results are merged into the
  // `filtered_local_entries` vector.
  base::Extend(filtered_local_entries,
               std::move(filtered_personal_context_entries));
  DeduplicateResults(filtered_local_entries);
  update_callback.Run(
      MemorySearchResults(MemorySearchStatus::kFinalResponseSuccess,
                          std::move(filtered_local_entries)));
}

}  // namespace accessibility_annotator
