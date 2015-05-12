// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include <curl/curl.h>

#include "common/string_util.h"

#include "core/hle/service/http/http.h"
#include "core/hle/service/http/http_c.h"
#include "core/hle/service/service.h"

namespace Service {
namespace HTTP {

std::unordered_map<ContextHandle, std::unique_ptr<HttpContext>> context_map;
ContextHandle next_handle;

static int BufWriter(u8 *data, size_t size, size_t nmemb, std::vector<u8>* out_buffer) {
    if (out_buffer == nullptr)
        return -1;
    out_buffer->insert(out_buffer->end(), data, data + size * nmemb);
    return static_cast<int>(size * nmemb);
}

HttpContext::~HttpContext() {
    curl_slist_free_all(request_hdrs);
}

static CURLcode SetConnectionType(CURL* connection, RequestType type) {
    switch (type) {
    case REQUEST_TYPE_GET:
        return curl_easy_setopt(connection, CURLOPT_HTTPGET, 1);
    case REQUEST_TYPE_POST:
    case REQUEST_TYPE_POST_:
        return curl_easy_setopt(connection, CURLOPT_POST, 1);
    case REQUEST_TYPE_PUT:
    case REQUEST_TYPE_PUT_:
        return curl_easy_setopt(connection, CURLOPT_UPLOAD, 1);
    case REQUEST_TYPE_DELETE:
        // TODO
        break;
    case REQUEST_TYPE_HEAD:
        return curl_easy_setopt(connection, CURLOPT_NOBODY, 1);
    }
}

void MakeRequest(HttpContext* context) {
    CURLM* manager = curl_multi_init();
    CURL* connection = curl_easy_init();

    CURLcode res;
    CURLMcode mres;

    {
        std::lock_guard<std::mutex> lock(context->mutex);

        res = curl_easy_setopt(connection, CURLOPT_URL, context->url.c_str());
        res = SetConnectionType(connection, context->req_type);
        res = curl_easy_setopt(connection, CURLOPT_HTTPHEADER, context->request_hdrs);
    }

    std::vector<u8> response_hdrs, response_data;
    res = curl_easy_setopt(connection, CURLOPT_HEADERFUNCTION, BufWriter);
    res = curl_easy_setopt(connection, CURLOPT_WRITEFUNCTION, BufWriter);
    res = curl_easy_setopt(connection, CURLOPT_HEADERDATA, &response_hdrs);
    res = curl_easy_setopt(connection, CURLOPT_WRITEDATA, &response_data);

    mres = curl_multi_add_handle(manager, connection);

    int still_running;
    int repeats;
    mres = curl_multi_perform(manager, &still_running);

    do {
        // Number of file descriptors. If this is zero, nothing happened to
        // the active connection.
        int numfds;

        if (curl_multi_wait(manager, NULL, 0, 1000, &numfds) != CURLM_OK)
            break;

        if (numfds == 0) {
            if (repeats++ > 1)
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
        } else {
            repeats = 0;
        }

        {
            std::lock_guard<std::mutex> lock(context->mutex);
            if (context->should_quit)
                break;
        }

        mres = curl_multi_perform(manager, &still_running);
    } while (still_running != 0);

    {
        std::lock_guard<std::mutex> lock(context->mutex);

        context->response_hdrs = std::move(response_hdrs);
        context->response_data = std::move(response_data);
        res = curl_easy_getinfo(connection, CURLINFO_RESPONSE_CODE, &context->response_code);
        res = curl_easy_getinfo(connection, CURLINFO_SIZE_DOWNLOAD, &context->downloaded_size);
        res = curl_easy_getinfo(connection, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &context->content_length);

        context->state = REQUEST_STATE_READY;
    }

    curl_multi_remove_handle(manager, connection);
    curl_easy_cleanup(connection);
    curl_multi_cleanup(manager);
}

void AddRequestHeader(const std::string& name, const std::string& value, curl_slist** hdr_list) {
    std::string header = Common::StringFromFormat("%s: %s", name.c_str(), value.c_str());
    *hdr_list = curl_slist_append(*hdr_list, header.c_str());
}

void Init() {
    AddService(new HTTP_C_Interface);

    curl_global_init(CURL_GLOBAL_DEFAULT);
}

void ClearInstance() {
    for (auto& pair : context_map) {
        pair.second->should_quit = true;
        if (pair.second->req_thread != nullptr)
            pair.second->req_thread->join();
    }

    context_map.clear();
    next_handle = 0;
}

void Shutdown() {
    curl_global_cleanup();
    ClearInstance();
}

}; // namespace HTTP
} // namespace Service
