// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <thread>

#include "core/hle/hle.h"
#include "core/hle/service/http/http.h"
#include "core/hle/service/http/http_c.h"

#include "common/logging/log.h"
#include "common/make_unique.h"
#include "common/string_util.h"

namespace Service {
namespace HTTP {

static void Initialize(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

static void CreateContext(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 url_len = cmd_buff[1];
    RequestType req_type = static_cast<RequestType>(cmd_buff[2]);
    char* url_ptr = reinterpret_cast<char*>(Memory::GetPointer(cmd_buff[4]));

    LOG_DEBUG(Service, "request url=%s req_type=%u", url_ptr, req_type);
    auto context = Common::make_unique<HttpContext>();

    context->req_type = req_type;
    context->url = std::string(url_ptr, url_len);
    context_map.emplace(next_handle, std::move(context));

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = next_handle;

    next_handle++;
}

static void CloseContext(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    ContextHandle handle = static_cast<ContextHandle>(cmd_buff[1]);
    auto map_it = context_map.find(handle);
    if (map_it == context_map.end()) {
        cmd_buff[1] = 0xD8E007F7;
        return;
    }

    map_it->second->cancel_request = true;
    if (map_it->second->req_thread != nullptr)
        map_it->second->req_thread->join();

    context_map.erase(handle);

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

static void CancelConnection(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    ContextHandle handle = static_cast<ContextHandle>(cmd_buff[1]);

    auto map_it = context_map.find(handle);
    if (map_it == context_map.end()) {
        cmd_buff[1] = 0xD8E007F7;
        return;
    }
    map_it->second->cancel_request = true;

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

static void GetRequestState(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    ContextHandle handle = static_cast<ContextHandle>(cmd_buff[1]);

    auto map_it = context_map.find(handle);
    if (map_it == context_map.end()) {
        cmd_buff[1] = 0xD8E007F7;
        return;
    }

    cmd_buff[1] = RESULT_SUCCESS.raw;

    std::lock_guard<std::mutex> lock(map_it->second->mutex);
    cmd_buff[2] = map_it->second->state;
}

static void GetDownloadSizeState(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    ContextHandle handle = static_cast<ContextHandle>(cmd_buff[1]);

    auto map_it = context_map.find(handle);
    if (map_it == context_map.end()) {
        cmd_buff[1] = 0xD8E007F7;
        return;
    }

    HttpContext* response = map_it->second.get();
    std::lock_guard<std::mutex> lock(response->mutex);

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = (u32) response->downloaded_size;
    // TODO: invalid content-length?
    cmd_buff[3] = (u32) response->content_length;
}

static void BeginRequest(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    ContextHandle handle = static_cast<ContextHandle>(cmd_buff[1]);

    auto map_it = context_map.find(handle);
    if (map_it == context_map.end()) {
        cmd_buff[1] = 0xD8E007F7;
        return;
    }

    HttpContext* context = map_it->second.get();
    std::lock_guard<std::mutex> lock(context->mutex);

    context->req_thread = Common::make_unique<std::thread>(MakeRequest, context);
    context->state = REQUEST_STATE_IN_PROGRESS;

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

static void ReceiveData(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    ContextHandle handle = static_cast<ContextHandle>(cmd_buff[1]);
    u32 buf_size = cmd_buff[2];
    u8* buffer = Memory::GetPointer(cmd_buff[4]);

    auto map_it = context_map.find(handle);
    if (map_it == context_map.end()) {
        cmd_buff[1] = 0xD8E007F7;
        return;
    }

    std::lock_guard<std::mutex> lock(map_it->second->mutex);
    const std::vector<u8>& data = map_it->second->response_data;
    if (buf_size < data.size()) {
        cmd_buff[1] = 0xD840A02B;
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
    char* hdr_name_buf = reinterpret_cast<char*>(Memory::GetPointer(cmd_buff[5]));
    char* hdr_val_buf  = reinterpret_cast<char*>(Memory::GetPointer(cmd_buff[7]));

    std::string hdr_name = std::string(hdr_name_buf, hdr_name_len);
    std::string hdr_val  = std::string(hdr_val_buf, hdr_val_len);

    if (hdr_name.empty()) {
        cmd_buff[1] = 0xD8E0A002;
        return;
    }

    auto map_it = context_map.find(handle);
    if (map_it == context_map.end()) {
        cmd_buff[1] = 0xD8E007F7;
        return;
    }

    std::lock_guard<std::mutex> lock(map_it->second->mutex);
    AddRequestHeader(hdr_name.c_str(),
                     hdr_val.c_str(),
                     &map_it->second->request_hdrs);
    cmd_buff[1] = RESULT_SUCCESS.raw;
}

static void GetResponseStatusCode(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    ContextHandle handle = static_cast<ContextHandle>(cmd_buff[1]);

    auto map_it = context_map.find(handle);
    if (map_it == context_map.end()) {
        cmd_buff[1] = 0xD8E007F7;
        return;
    }

    // TODO: Verify behavior
    while (true) {
        std::lock_guard<std::mutex> lock(map_it->second->mutex);
        if (map_it->second->state == REQUEST_STATE_READY)
            break;
    }

    std::lock_guard<std::mutex> lock(map_it->second->mutex);

    // TODO: Request not done yet

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = (u32)map_it->second->response_code;
}

static void Finalize(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    ClearInstance();

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010044, Initialize,              "Initialize"},
    {0x00020082, CreateContext,           "CreateContext"},
    {0x00030040, CloseContext,            "CloseContext"},
    {0x00040040, CancelConnection,        "CancelConnection"},
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

HTTP_C_Interface::HTTP_C_Interface() {
    Register(FunctionTable);
}

}
} // namespace
