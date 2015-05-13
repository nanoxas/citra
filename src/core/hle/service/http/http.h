// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include <mutex>
#include <memory>
#include <thread>
#include <string>
#include <unordered_map>

#include "core/hle/result.h"

struct curl_slist;

namespace Service {
namespace HTTP {

typedef u32 ContextHandle;

/// HTTP operation that will be performed by the request (API-exposed).
enum RequestType : u32 {
    REQUEST_TYPE_NONE   = 0,
    REQUEST_TYPE_GET    = 1,
    REQUEST_TYPE_POST   = 2,
    REQUEST_TYPE_HEAD   = 3,
    REQUEST_TYPE_PUT    = 4,
    REQUEST_TYPE_DELETE = 5,
    REQUEST_TYPE_POST_  = 6,
    REQUEST_TYPE_PUT_   = 7
};

/// Current state of the HTTP request (API-exposed).
enum RequestState : u32 {
    REQUEST_STATE_NONE = 0, // TODO: Verify
    REQUEST_STATE_IN_PROGRESS = 5,
    REQUEST_STATE_READY = 8,
};

/** A helper struct that contains all the required information for a HTTP request.
 * This struct, as it can be accessed by multiple threads, contains a mutex that
 * must be locked whenever any of the struct's members are being accessed. This is
 * to prevent any possible race conditions.
 */
struct HttpContext {
    std::mutex mutex;

    //--- Ongoing request management
    RequestState state;               //< API-exposed current state of the HTTP request
    std::unique_ptr<std::thread> req_thread;
    bool cancel_request;              //< True if the request's thread should be canceled ASAP

    //--- Request data
    std::string url;                  //< URL to the target server.
    RequestType req_type;             //< Type of request that will be performed
    curl_slist* request_hdrs;         //< Buffer for request headers to be sent to the server.

    //--- Response data
    std::vector<u8> response_hdrs;    //< Buffer for response headers returned by the server.
    std::vector<u8> response_data;    //< Buffer for response body returned by the server.
    long response_code;               //< The three-digit HTTP response code returned by the server.
    double content_length;            //< The total size in bytes that will be downloaded this request.
    double downloaded_size;           //< The amount in bytes that has been downloaded so far.

    HttpContext();
    ~HttpContext();
};

extern std::unordered_map<ContextHandle, std::unique_ptr<HttpContext>> context_map;
extern ContextHandle next_handle;

/** Makes an HTTP request using libcurl. Ideally called as part of a new thread.
 * @param context A pointer to the HttpContext that will be used by this request. This pointer must remain valid.
 */
void MakeRequest(HttpContext* context);

/** Appends a name-value pair to a curl_slist*
 * @param hdr_list A reference to the curl_slist*
 */
void AddRequestHeader(const std::string& name, const std::string& value, curl_slist** hdr_list);

/// Initialize the HTTP service
void Init();

void ClearInstance();

/// Shutdown the HTTP service
void Shutdown();

} // namespace HTTP
} // namespace Service
