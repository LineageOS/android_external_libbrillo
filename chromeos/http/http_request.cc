// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/http/http_request.h>

#include <base/logging.h>
#include <chromeos/map_utils.h>
#include <chromeos/mime_utils.h>
#include <chromeos/strings/string_utils.h>

namespace chromeos {
namespace http {

// request_type
const char request_type::kOptions[]               = "OPTIONS";
const char request_type::kGet[]                   = "GET";
const char request_type::kHead[]                  = "HEAD";
const char request_type::kPost[]                  = "POST";
const char request_type::kPut[]                   = "PUT";
const char request_type::kPatch[]                 = "PATCH";
const char request_type::kDelete[]                = "DELETE";
const char request_type::kTrace[]                 = "TRACE";
const char request_type::kConnect[]               = "CONNECT";
const char request_type::kCopy[]                  = "COPY";
const char request_type::kMove[]                  = "MOVE";

// request_header
const char request_header::kAccept[]              = "Accept";
const char request_header::kAcceptCharset[]       = "Accept-Charset";
const char request_header::kAcceptEncoding[]      = "Accept-Encoding";
const char request_header::kAcceptLanguage[]      = "Accept-Language";
const char request_header::kAllow[]               = "Allow";
const char request_header::kAuthorization[]       = "Authorization";
const char request_header::kCacheControl[]        = "Cache-Control";
const char request_header::kConnection[]          = "Connection";
const char request_header::kContentEncoding[]     = "Content-Encoding";
const char request_header::kContentLanguage[]     = "Content-Language";
const char request_header::kContentLength[]       = "Content-Length";
const char request_header::kContentLocation[]     = "Content-Location";
const char request_header::kContentMd5[]          = "Content-MD5";
const char request_header::kContentRange[]        = "Content-Range";
const char request_header::kContentType[]         = "Content-Type";
const char request_header::kCookie[]              = "Cookie";
const char request_header::kDate[]                = "Date";
const char request_header::kExpect[]              = "Expect";
const char request_header::kExpires[]             = "Expires";
const char request_header::kFrom[]                = "From";
const char request_header::kHost[]                = "Host";
const char request_header::kIfMatch[]             = "If-Match";
const char request_header::kIfModifiedSince[]     = "If-Modified-Since";
const char request_header::kIfNoneMatch[]         = "If-None-Match";
const char request_header::kIfRange[]             = "If-Range";
const char request_header::kIfUnmodifiedSince[]   = "If-Unmodified-Since";
const char request_header::kLastModified[]        = "Last-Modified";
const char request_header::kMaxForwards[]         = "Max-Forwards";
const char request_header::kPragma[]              = "Pragma";
const char request_header::kProxyAuthorization[]  = "Proxy-Authorization";
const char request_header::kRange[]               = "Range";
const char request_header::kReferer[]             = "Referer";
const char request_header::kTE[]                  = "TE";
const char request_header::kTrailer[]             = "Trailer";
const char request_header::kTransferEncoding[]    = "Transfer-Encoding";
const char request_header::kUpgrade[]             = "Upgrade";
const char request_header::kUserAgent[]           = "User-Agent";
const char request_header::kVia[]                 = "Via";
const char request_header::kWarning[]             = "Warning";

// response_header
const char response_header::kAcceptRanges[]       = "Accept-Ranges";
const char response_header::kAge[]                = "Age";
const char response_header::kAllow[]              = "Allow";
const char response_header::kCacheControl[]       = "Cache-Control";
const char response_header::kConnection[]         = "Connection";
const char response_header::kContentEncoding[]    = "Content-Encoding";
const char response_header::kContentLanguage[]    = "Content-Language";
const char response_header::kContentLength[]      = "Content-Length";
const char response_header::kContentLocation[]    = "Content-Location";
const char response_header::kContentMd5[]         = "Content-MD5";
const char response_header::kContentRange[]       = "Content-Range";
const char response_header::kContentType[]        = "Content-Type";
const char response_header::kDate[]               = "Date";
const char response_header::kETag[]               = "ETag";
const char response_header::kExpires[]            = "Expires";
const char response_header::kLastModified[]       = "Last-Modified";
const char response_header::kLocation[]           = "Location";
const char response_header::kPragma[]             = "Pragma";
const char response_header::kProxyAuthenticate[]  = "Proxy-Authenticate";
const char response_header::kRetryAfter[]         = "Retry-After";
const char response_header::kServer[]             = "Server";
const char response_header::kSetCookie[]          = "Set-Cookie";
const char response_header::kTrailer[]            = "Trailer";
const char response_header::kTransferEncoding[]   = "Transfer-Encoding";
const char response_header::kUpgrade[]            = "Upgrade";
const char response_header::kVary[]               = "Vary";
const char response_header::kVia[]                = "Via";
const char response_header::kWarning[]            = "Warning";
const char response_header::kWwwAuthenticate[]    = "WWW-Authenticate";

// ***********************************************************
// ********************** Request Class **********************
// ***********************************************************
Request::Request(const std::string& url, const std::string& method,
                 std::shared_ptr<Transport> transport) :
    transport_(transport), request_url_(url), method_(method) {
  VLOG(1) << "http::Request created";
  if (!transport_)
    transport_ = http::Transport::CreateDefault();
}

Request::~Request() {
  VLOG(1) << "http::Request destroyed";
}

void Request::AddRange(int64_t bytes) {
  if (bytes < 0) {
    ranges_.emplace_back(Request::range_value_omitted, -bytes);
  } else {
    ranges_.emplace_back(bytes, Request::range_value_omitted);
  }
}

void Request::AddRange(uint64_t from_byte, uint64_t to_byte) {
  ranges_.emplace_back(from_byte, to_byte);
}

std::unique_ptr<Response> Request::GetResponseAndBlock(
    chromeos::ErrorPtr* error) {
  if (!SendRequestIfNeeded(error) || !connection_->FinishRequest(error))
    return std::unique_ptr<Response>();
  std::unique_ptr<Response> response(new Response(std::move(connection_)));
  transport_.reset();  // Indicate that the response has been received
  return response;
}

void Request::SetAccept(const std::string& accept_mime_types) {
  accept_ = accept_mime_types;
}

std::string Request::GetAccept() const {
  return accept_;
}

void Request::SetContentType(const std::string& contentType) {
  content_type_ = contentType;
}

std::string Request::GetContentType() const {
  return content_type_;
}

void Request::AddHeader(const std::string& header, const std::string& value) {
  headers_[header] = value;
}

void Request::AddHeaders(const HeaderList& headers) {
  headers_.insert(headers.begin(), headers.end());
}

bool Request::AddRequestBody(const void* data,
                             size_t size,
                             chromeos::ErrorPtr* error) {
  if (!SendRequestIfNeeded(error))
    return false;
  return connection_->WriteRequestData(data, size, error);
}

void Request::SetReferer(const std::string& referer) {
  referer_ = referer;
}

std::string Request::GetReferer() const {
  return referer_;
}

void Request::SetUserAgent(const std::string& user_agent) {
  user_agent_ = user_agent;
}

std::string Request::GetUserAgent() const {
  return user_agent_;
}

bool Request::SendRequestIfNeeded(chromeos::ErrorPtr* error) {
  if (transport_) {
    if (!connection_) {
      http::HeaderList headers = chromeos::MapToVector(headers_);
      std::vector<std::string> ranges;
      if (method_ != request_type::kHead) {
        ranges.reserve(ranges_.size());
        for (auto p : ranges_) {
          if (p.first != range_value_omitted ||
              p.second != range_value_omitted) {
            std::string range;
            if (p.first != range_value_omitted) {
              range = chromeos::string_utils::ToString(p.first);
            }
            range += '-';
            if (p.second != range_value_omitted) {
              range += chromeos::string_utils::ToString(p.second);
            }
            ranges.push_back(range);
          }
        }
      }
      if (!ranges.empty())
        headers.emplace_back(request_header::kRange,
                             "bytes=" +
                                 chromeos::string_utils::Join(',', ranges));

      headers.emplace_back(request_header::kAccept, GetAccept());
      if (method_ != request_type::kGet && method_ != request_type::kHead) {
        if (!content_type_.empty())
          headers.emplace_back(request_header::kContentType, content_type_);
      }
      connection_ = transport_->CreateConnection(transport_, request_url_,
                                                 method_, headers,
                                                 user_agent_, referer_,
                                                 error);
    }

    if (connection_)
      return true;
  } else {
    chromeos::Error::AddTo(error, FROM_HERE, http::kErrorDomain,
                           "response_already_received",
                           "HTTP response already received");
  }
  return false;
}

// ************************************************************
// ********************** Response Class **********************
// ************************************************************
Response::Response(std::unique_ptr<Connection> connection)
    : connection_(std::move(connection)) {
  VLOG(1) << "http::Response created";
  // Response object doesn't have streaming interface for response data (yet),
  // so read the data into a buffer and cache it.
  if (connection_) {
    size_t size = static_cast<size_t>(connection_->GetResponseDataSize());
    response_data_.reserve(size);
    unsigned char buffer[1024];
    size_t read = 0;
    while (connection_->ReadResponseData(buffer, sizeof(buffer),
                                         &read, nullptr) && read > 0) {
      response_data_.insert(response_data_.end(), buffer, buffer + read);
    }
  }
}

Response::~Response() {
  VLOG(1) << "http::Response destroyed";
}

bool Response::IsSuccessful() const {
  int code = GetStatusCode();
  return code >= status_code::Continue && code < status_code::BadRequest;
}

int Response::GetStatusCode() const {
  if (!connection_)
    return -1;

  return connection_->GetResponseStatusCode();
}

std::string Response::GetStatusText() const {
  if (!connection_)
    return std::string();

  return connection_->GetResponseStatusText();
}

std::string Response::GetContentType() const {
  return GetHeader(response_header::kContentType);
}

const std::vector<unsigned char>& Response::GetData() const {
  return response_data_;
}

std::string Response::GetDataAsString() const {
  if (response_data_.empty())
    return std::string();

  const char* data_buf = reinterpret_cast<const char*>(response_data_.data());
  return std::string(data_buf, data_buf + response_data_.size());
}

std::string Response::GetHeader(const std::string& header_name) const {
  if (connection_)
    return connection_->GetResponseHeader(header_name);

  return std::string();
}

}  // namespace http
}  // namespace chromeos
