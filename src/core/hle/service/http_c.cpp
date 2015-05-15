// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <stdexcept>

#include <curl/curl.h>

#include "core/hle/hle.h"
#include "core/hle/service/http_c.h"

#include "common/logging/log.h"
#include "common/make_unique.h"
#include "common/string_util.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace HTTP_C

namespace HTTP_C {

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
    REQUEST_STATE_IN_PROGRESS = 5,
    REQUEST_STATE_READY = 7,
};

struct HttpContext {
    std::string url;
    RequestType req_type;
    RequestState req_state;
    curl_slist* request_hdrs;
    std::vector<u8> response_hdrs;
    std::vector<u8> response_data;
    long response_code;
    double download_size;
    double downloaded_size;

    ~HttpContext() {
        curl_slist_free_all(request_hdrs);
    }
};

static std::unordered_map<ContextHandle, std::unique_ptr<HttpContext>> handle_map;
static ContextHandle next_handle;

static int writer_callback(u8* data, size_t size, size_t nmemb, std::vector<u8>* writer_data) {
    if (writer_data == nullptr)
        return 0;
    writer_data->reserve(writer_data->size() + size * nmemb);
    writer_data->insert(writer_data->end(), data, data + size * nmemb);
    return (int)(size * nmemb);
}

static void Initialize(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    curl_global_init(CURL_GLOBAL_DEFAULT);

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

static void CreateContext(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 url_len = cmd_buff[1];
    RequestType req_type = static_cast<RequestType>(cmd_buff[2]);
    u32 url_ptr = cmd_buff[4];

    std::string url(reinterpret_cast<const char*>(Memory::GetPointer(url_ptr)), url_len);

    // This should never even happen in the first place with 64-bit handles, 
    while (handle_map.count(next_handle) != 0) {
        ++next_handle;
    }

    LOG_DEBUG(Service, "context url=%s req_type=%u",
              url.c_str(), req_type);

    HttpContext* context = new HttpContext({});
    context->req_type = req_type;
    context->url = url;

    handle_map.emplace(next_handle, std::unique_ptr<HttpContext>(context));

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = next_handle;

    next_handle++;
}

static void CloseContext(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    ContextHandle handle = static_cast<ContextHandle>(cmd_buff[1]);
    auto map_it = handle_map.find(handle);
    if (map_it == handle_map.end()) {
        cmd_buff[1] = -1; // TODO: Find proper result code for invalid handle
        return;
    }
    handle_map.erase(handle);

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

static void GetRequestState(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    ContextHandle handle = static_cast<ContextHandle>(cmd_buff[1]);

    auto map_it = handle_map.find(handle);
    if (map_it == handle_map.end()) {
        cmd_buff[1] = -1; // TODO: Find proper result code for invalid handle
        return;
    }

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = map_it->second->req_state;
}

static void GetDownloadSizeState(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    ContextHandle handle = static_cast<ContextHandle>(cmd_buff[1]);

    auto map_it = handle_map.find(handle);
    if (map_it == handle_map.end()) {
        cmd_buff[1] = -1; // TODO: Find proper result code for invalid handle
        return;
    }

    HttpContext* context = map_it->second.get();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = (u32)context->downloaded_size;
    // TODO: invalid content-length?
    cmd_buff[3] = (u32)context->download_size;
    return;
}

static void BeginRequest(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    ContextHandle handle = static_cast<ContextHandle>(cmd_buff[1]);

    auto map_it = handle_map.find(handle);
    if (map_it == handle_map.end()) {
        cmd_buff[1] = -1; // TODO: Find proper result code for invalid handle
        return;
    }

    HttpContext* context = map_it->second.get();
    CURL* connection = curl_easy_init();

    CURLcode res;
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
        case REQUEST_TYPE_DELETE:
            break;
        case REQUEST_TYPE_HEAD:
            res = curl_easy_setopt(connection, CURLOPT_NOBODY, 1);
            break;
    }

    res = curl_easy_setopt(connection, CURLOPT_WRITEFUNCTION, writer_callback);
    res = curl_easy_setopt(connection, CURLOPT_WRITEHEADER, &context->response_hdrs);
    res = curl_easy_setopt(connection, CURLOPT_WRITEDATA, &context->response_data);

    curl_easy_setopt(connection, CURLOPT_HTTPHEADER, context->request_hdrs);

    context->req_state = REQUEST_STATE_IN_PROGRESS;
    res = curl_easy_perform(connection);
    context->req_state = REQUEST_STATE_READY;

    curl_easy_getinfo(connection, CURLINFO_RESPONSE_CODE, &context->response_code);
    curl_easy_getinfo(connection, CURLINFO_SIZE_DOWNLOAD, &context->downloaded_size);
    curl_easy_getinfo(connection, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &context->download_size);

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

static void ReceiveData(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    ContextHandle handle = static_cast<ContextHandle>(cmd_buff[1]);
    u32 buf_size = cmd_buff[2];    
    u8* buffer = Memory::GetPointer(cmd_buff[4]);

    auto map_it = handle_map.find(handle);
    if (map_it == handle_map.end()) {
        cmd_buff[1] = -1; // TODO: Find proper result code for invalid handle
        return;
    }

    const std::vector<u8>& data = map_it->second->response_data;
    if (buf_size < data.size()) {
        cmd_buff[1] = 0xd840a02b;
        return;
    }

    std::copy(data.begin(), data.end(), buffer);
    cmd_buff[1] = RESULT_SUCCESS.raw;
}

static void AddRequestHeader(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    ContextHandle handle = static_cast<ContextHandle>(cmd_buff[1]);
    u32 hdr_name_len = cmd_buff[2];
    u32 hdr_val_len  = cmd_buff[3];
    u32 hdr_name_buf = cmd_buff[5];
    u32 hdr_val_buf  = cmd_buff[7];

    // TODO: something is NULL
    // TODO: header value is empty
    // TODO: header name is empty
    std::string header = Common::StringFromFormat("%s: %s",
        std::string(reinterpret_cast<const char*>(Memory::GetPointer(hdr_name_buf)), hdr_name_len).c_str(),
        std::string(reinterpret_cast<const char*>(Memory::GetPointer(hdr_val_buf)), hdr_val_len).c_str());

    auto map_it = handle_map.find(handle);
    if (map_it == handle_map.end()) {
        cmd_buff[1] = -1; // TODO: Find proper result code for invalid handle
        return;
    }

    map_it->second->request_hdrs = curl_slist_append(map_it->second->request_hdrs, header.c_str());
    cmd_buff[1] = RESULT_SUCCESS.raw;
}

static void GetResponseStatusCode(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    ContextHandle handle = static_cast<ContextHandle>(cmd_buff[1]);

    auto map_it = handle_map.find(handle);
    if (map_it == handle_map.end()) {
        cmd_buff[1] = -1; // TODO: Find proper result code for invalid handle
        return;
    }

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = (u32)map_it->second->response_code;
}

static void Finalize(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    handle_map.clear();
    next_handle = 0;
    curl_global_cleanup();

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010044, Initialize,              "Initialize"},
    {0x00020082, CreateContext,           "CreateContext"},
    {0x00030040, CloseContext,            "CloseContext"},
    {0x00040040, nullptr,                 "CancelConnection"},
    {0x00050040, GetRequestState,         "GetRequestState"},
    {0x00060040, GetDownloadSizeState,    "GetDownloadSizeState"},
    {0x00070040, nullptr,                 "GetRequestError"},
    {0x00080042, nullptr,                 "InitializeConnectionSession"},
    {0x00090040, BeginRequest,            "BeginRequest"},
    {0x000A0040, nullptr,                 "BeginRequestAsync"},
    {0x000B0082, ReceiveData,             "ReceiveData"},
    {0x000C0102, nullptr,                 "ReceiveDataTimeout"},
    {0x000D0146, nullptr,                 "SetProxy"},
    {0x000E0040, nullptr,                 "SetProxyDefault"},
    {0x000F00C4, nullptr,                 "SetBasicAuthorization"},
    {0x00100080, nullptr,                 "SetSocketBufferSize"},
    {0x001100C4, AddRequestHeader,        "AddRequestHeader"},
    {0x001200C4, nullptr,                 "AddPostDataAscii"},
    {0x001300C4, nullptr,                 "AddPostDataBinary"},
    {0x00140082, nullptr,                 "AddPostDataRaw"},
    {0x00150080, nullptr,                 "SetPostDataType"},
    {0x001600C4, nullptr,                 "SendPostDataAscii"},
    {0x00170144, nullptr,                 "SendPostDataAsciiTimeout"},
    {0x001800C4, nullptr,                 "SendPostDataBinary"},
    {0x00190144, nullptr,                 "SendPostDataBinaryTimeout"},
    {0x001A0082, nullptr,                 "SendPostDataRaw"},
    {0x001B0102, nullptr,                 "SendPostDataRawTimeout"},
    {0x001C0080, nullptr,                 "SetPostDataEncoding"},
    {0x001D0040, nullptr,                 "NotifyFinishSendPostData"},
    {0x001E00C4, nullptr,                 "GetResponseHeader"},
    {0x001F0144, nullptr,                 "GetResponseHeaderTimeout"},
    {0x00200082, nullptr,                 "GetResponseData"},
    {0x00210102, nullptr,                 "GetResponseDataTimeout"},
    {0x00220040, GetResponseStatusCode,   "GetResponseStatusCode"},
    {0x002300C0, nullptr,                 "GetResponseStatusCodeTimeout"},
    {0x00240082, nullptr,                 "AddTrustedRootCA"},
    {0x00350186, nullptr,                 "SetDefaultProxy"},
    {0x00360000, nullptr,                 "ClearDNSCache"},
    {0x00370080, nullptr,                 "SetKeepAlive"},
    {0x003800C0, Finalize,                "Finalize"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
}

} // namespace
