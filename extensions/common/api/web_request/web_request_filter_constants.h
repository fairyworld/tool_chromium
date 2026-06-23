// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_WEB_REQUEST_WEB_REQUEST_FILTER_CONSTANTS_H_
#define EXTENSIONS_COMMON_API_WEB_REQUEST_WEB_REQUEST_FILTER_CONSTANTS_H_

#include "extensions/common/url_pattern.h"

namespace extensions {

// The URL schemes a `webRequest.RequestFilter` URL pattern can match.
inline constexpr int kWebRequestFilterValidSchemes =
    URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS |
    URLPattern::SCHEME_FTP | URLPattern::SCHEME_FILE |
    URLPattern::SCHEME_EXTENSION | URLPattern::SCHEME_WS |
    URLPattern::SCHEME_WSS | URLPattern::SCHEME_UUID_IN_PACKAGE;

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_WEB_REQUEST_WEB_REQUEST_FILTER_CONSTANTS_H_
