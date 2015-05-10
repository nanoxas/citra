// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <curl/curl.h>

#include "common/string_util.h"

#include "core/hle/service/http/http.h"
#include "core/hle/service/http/http_c.h"
#include "core/hle/service/service.h"

namespace Service {
namespace HTTP {

std::unordered_map<ContextHandle, std::unique_ptr<HttpContext>> context_map;
ContextHandle next_handle;

static int WriteBuf(u8 *data, size_t size, size_t nmemb, std::mutex *mutex, std::vector<u8> *buffer) {
    std::lock_guard<std::mutex> lock(*mutex);
    buffer->reserve(buffer->size() + size * nmemb);
    buffer->insert(buffer->end(), data, data + size * nmemb);
    return static_cast<int>(size * nmemb);
}

static int HdrWriter(u8 *data, size_t size, size_t nmemb, HttpContext *context) {
    if (context == nullptr || context->should_quit)
        return -1;
    return WriteBuf(data, size, nmemb, &context->mutex, &context->response_hdrs);
}

static int DataWriter(u8 *data, size_t size, size_t nmemb, HttpContext *context) {
    if (context == nullptr || context->should_quit)
        return -1;
    return WriteBuf(data, size, nmemb, &context->mutex, &context->response_data);
}

HttpContext::~HttpContext() {
    curl_slist_free_all(request_hdrs);
}

void MakeRequest(HttpContext* context) {
    CURL* connection = curl_easy_init();
    CURLcode res;
    {
        std::lock_guard<std::mutex> lock(context->mutex);

        context->state = REQUEST_STATE_IN_PROGRESS;
        res = curl_easy_setopt(connection, CURLOPT_URL, context->url.c_str());
        switch (context->req_type) {
            case REQUEST_TYPE_GET:
                res = curl_easy_setopt(connection, CURLOPT_HTTPGET, 1);
                break;
            case REQUEST_TYPE_POST:
            case REQUEST_TYPE_POST_:
                res = curl_easy_setopt(connection, CURLOPT_POST, 1);
                break;
            case REQUEST_TYPE_PUT:
            case REQUEST_TYPE_PUT_:
                res = curl_easy_setopt(connection, CURLOPT_UPLOAD, 1);
                break;
            case REQUEST_TYPE_DELETE:
                // TODO
                break;
            case REQUEST_TYPE_HEAD:
                res = curl_easy_setopt(connection, CURLOPT_NOBODY, 1);
                break;
        }

        res = curl_easy_setopt(connection, CURLOPT_HEADERFUNCTION, HdrWriter);
        res = curl_easy_setopt(connection, CURLOPT_WRITEFUNCTION, DataWriter);
        res = curl_easy_setopt(connection, CURLOPT_HEADERDATA, context);
        res = curl_easy_setopt(connection, CURLOPT_WRITEDATA, context);
        res = curl_easy_setopt(connection, CURLOPT_HTTPHEADER, context->request_hdrs);
    }

    res = curl_easy_perform(connection);
    {
        std::lock_guard<std::mutex> lock(context->mutex);

        res = curl_easy_getinfo(connection, CURLINFO_RESPONSE_CODE, &context->response_code);
        res = curl_easy_getinfo(connection, CURLINFO_SIZE_DOWNLOAD, &context->downloaded_size);
        res = curl_easy_getinfo(connection, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &context->download_size);

        context->state = REQUEST_STATE_READY;
    }

    curl_easy_cleanup(connection);
}

void AddRequestHeader(std::string name, std::string value, curl_slist** hdr_list) {
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

} // namespace HTTP
} // namespace Service
