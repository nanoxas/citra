// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include <mutex>
#include <memory>
#include <thread>
#include <unordered_map>

#include "core/hle/result.h"

struct curl_slist;

namespace Service {
namespace HTTP {

typedef u32 ContextHandle;

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

enum RequestState : u32 {
    REQUEST_STATE_DEFAULT, // TODO
    REQUEST_STATE_IN_PROGRESS = 5,
    REQUEST_STATE_READY = 8,
};

struct HttpContext {
    std::mutex mutex;
    bool should_quit;

    RequestState state;
    std::unique_ptr<std::thread> req_thread;

    std::string url;
    RequestType req_type;
    curl_slist* request_hdrs;

    std::vector<u8> response_hdrs;
    std::vector<u8> response_data;
    long response_code;
    double download_size;
    double downloaded_size;

    ~HttpContext();
};

extern std::unordered_map<ContextHandle, std::unique_ptr<HttpContext>> context_map;
extern ContextHandle next_handle;

void MakeRequest(HttpContext* context);
void AddRequestHeader(std::string name, std::string value, curl_slist** hdr_list);

/// Initialize the HTTP service
void Init();

void ClearInstance();

/// Shutdown the HTTP service
void Shutdown();

} // namespace HTTP
} // namespace Service
