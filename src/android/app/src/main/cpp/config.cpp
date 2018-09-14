// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <iomanip>
#include <memory>
#include <sstream>
#include <unordered_map>
//#include <SDL.h>
#include <inih/cpp/INIReader.h>
#include "config.h"
#include "default_ini.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/param_package.h"
#include "core/hle/service/service.h"
#include "core/settings.h"
#include "input_common/main.h"
#include "input_common/udp/client.h"

Config::Config() {
    // TODO: Don't hardcode the path; let the frontend decide where to put the config files.
    android_config_loc = FileUtil::GetUserPath(D_CONFIG_IDX) + "android-config.ini";
    android_config = std::make_unique<INIReader>(android_config_loc);

    Reload();
}

Config::~Config() = default;

bool Config::LoadINI(const std::string& default_contents, bool retry) {
    const char* location = this->android_config_loc.c_str();
    if (android_config->ParseError() < 0) {
        if (retry) {
            LOG_WARNING(Config, "Failed to load {}. Creating file from defaults...", location);
            FileUtil::CreateFullPath(location);
            FileUtil::WriteStringToFile(true, default_contents, location);
            android_config = std::make_unique<INIReader>(location); // Reopen file

            return LoadINI(default_contents, false);
        }
        LOG_ERROR(Config, "Failed.");
        return false;
    }
    LOG_INFO(Config, "Successfully loaded {}", location);
    return true;
}

void Config::ReadValues() {
    // Controls
    for (int i = 0; i < Settings::NativeButton::NumButtons; ++i) {
        // TODO: implement controls
        Settings::values.buttons[i] = '0';
    }

    for (int i = 0; i < Settings::NativeAnalog::NumAnalogs; ++i) {
        // TODO: implement controls
        Settings::values.analogs[i] = '0';
    }

    Settings::values.motion_device =
            android_config->Get("Controls", "motion_device",
                             "engine:motion_emu,update_period:100,sensitivity:0.01,tilt_clamp:90.0");
    Settings::values.touch_device =
            android_config->Get("Controls", "touch_device", "engine:emu_window");
    Settings::values.udp_input_address =
            android_config->Get("Controls", "udp_input_address", InputCommon::CemuhookUDP::DEFAULT_ADDR);
    Settings::values.udp_input_port = static_cast<u16>(android_config->GetInteger(
            "Controls", "udp_input_port", InputCommon::CemuhookUDP::DEFAULT_PORT));

    // Core
    Settings::values.use_cpu_jit = android_config->GetBoolean("Core", "use_cpu_jit", true);

    // Renderer
    Settings::values.use_hw_renderer = android_config->GetBoolean("Renderer", "use_hw_renderer", true);
#ifdef __APPLE__
    // Hardware shader is broken on macos thanks to poor drivers.
    // We still want to provide this option for test/development purposes, but disable it by
    // default.
    Settings::values.use_hw_shader = android_config->GetBoolean("Renderer", "use_hw_shader", false);
#else
    Settings::values.use_hw_shader = android_config->GetBoolean("Renderer", "use_hw_shader", true);
#endif
    Settings::values.shaders_accurate_gs =
            android_config->GetBoolean("Renderer", "shaders_accurate_gs", true);
    Settings::values.shaders_accurate_mul =
            android_config->GetBoolean("Renderer", "shaders_accurate_mul", false);
    Settings::values.use_shader_jit = android_config->GetBoolean("Renderer", "use_shader_jit", true);
    Settings::values.resolution_factor =
            static_cast<u16>(android_config->GetInteger("Renderer", "resolution_factor", 1));
    Settings::values.use_vsync = android_config->GetBoolean("Renderer", "use_vsync", false);
    Settings::values.use_frame_limit = android_config->GetBoolean("Renderer", "use_frame_limit", true);
    Settings::values.frame_limit =
            static_cast<u16>(android_config->GetInteger("Renderer", "frame_limit", 100));

    Settings::values.toggle_3d = android_config->GetBoolean("Renderer", "toggle_3d", false);
    Settings::values.factor_3d =
            static_cast<u8>(android_config->GetInteger("Renderer", "factor_3d", 0));

    Settings::values.bg_red = (float)android_config->GetReal("Renderer", "bg_red", 0.0);
    Settings::values.bg_green = (float)android_config->GetReal("Renderer", "bg_green", 0.0);
    Settings::values.bg_blue = (float)android_config->GetReal("Renderer", "bg_blue", 0.0);

    // Layout
    Settings::values.layout_option =
            static_cast<Settings::LayoutOption>(android_config->GetInteger("Layout", "layout_option", 0));
    Settings::values.swap_screen = android_config->GetBoolean("Layout", "swap_screen", false);
    Settings::values.custom_layout = android_config->GetBoolean("Layout", "custom_layout", false);
    Settings::values.custom_top_left =
            static_cast<u16>(android_config->GetInteger("Layout", "custom_top_left", 0));
    Settings::values.custom_top_top =
            static_cast<u16>(android_config->GetInteger("Layout", "custom_top_top", 0));
    Settings::values.custom_top_right =
            static_cast<u16>(android_config->GetInteger("Layout", "custom_top_right", 400));
    Settings::values.custom_top_bottom =
            static_cast<u16>(android_config->GetInteger("Layout", "custom_top_bottom", 240));
    Settings::values.custom_bottom_left =
            static_cast<u16>(android_config->GetInteger("Layout", "custom_bottom_left", 40));
    Settings::values.custom_bottom_top =
            static_cast<u16>(android_config->GetInteger("Layout", "custom_bottom_top", 240));
    Settings::values.custom_bottom_right =
            static_cast<u16>(android_config->GetInteger("Layout", "custom_bottom_right", 360));
    Settings::values.custom_bottom_bottom =
            static_cast<u16>(android_config->GetInteger("Layout", "custom_bottom_bottom", 480));

    // Audio
    Settings::values.sink_id = android_config->Get("Audio", "output_engine", "auto");
    Settings::values.enable_audio_stretching =
            android_config->GetBoolean("Audio", "enable_audio_stretching", true);
    Settings::values.audio_device_id = android_config->Get("Audio", "output_device", "auto");
    Settings::values.volume = android_config->GetReal("Audio", "volume", 1);

    // Data Storage
    Settings::values.use_virtual_sd =
            android_config->GetBoolean("Data Storage", "use_virtual_sd", true);

    // System
    Settings::values.is_new_3ds = android_config->GetBoolean("System", "is_new_3ds", false);
    Settings::values.region_value =
            android_config->GetInteger("System", "region_value", Settings::REGION_VALUE_AUTO_SELECT);
    Settings::values.init_clock =
            static_cast<Settings::InitClock>(android_config->GetInteger("System", "init_clock", 1));
    {
        std::tm t;
        t.tm_sec = 1;
        t.tm_min = 0;
        t.tm_hour = 0;
        t.tm_mday = 1;
        t.tm_mon = 0;
        t.tm_year = 100;
        t.tm_isdst = 0;
        std::istringstream string_stream(
                android_config->Get("System", "init_time", "2000-01-01 00:00:01"));
        string_stream >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");
        if (string_stream.fail()) {
            LOG_ERROR(Config, "Failed To parse init_time. Using 2000-01-01 00:00:01");
        }
        Settings::values.init_time =
                std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::from_time_t(std::mktime(&t)).time_since_epoch())
                        .count();
    }

    // Camera
    using namespace Service::CAM;
    Settings::values.camera_name[OuterRightCamera] =
            android_config->Get("Camera", "camera_outer_right_name", "blank");
    Settings::values.camera_config[OuterRightCamera] =
            android_config->Get("Camera", "camera_outer_right_config", "");
    Settings::values.camera_flip[OuterRightCamera] =
            android_config->GetInteger("Camera", "camera_outer_right_flip", 0);
    Settings::values.camera_name[InnerCamera] =
            android_config->Get("Camera", "camera_inner_name", "blank");
    Settings::values.camera_config[InnerCamera] =
            android_config->Get("Camera", "camera_inner_config", "");
    Settings::values.camera_flip[InnerCamera] =
            android_config->GetInteger("Camera", "camera_inner_flip", 0);
    Settings::values.camera_name[OuterLeftCamera] =
            android_config->Get("Camera", "camera_outer_left_name", "blank");
    Settings::values.camera_config[OuterLeftCamera] =
            android_config->Get("Camera", "camera_outer_left_config", "");
    Settings::values.camera_flip[OuterLeftCamera] =
            android_config->GetInteger("Camera", "camera_outer_left_flip", 0);

    // Miscellaneous
    Settings::values.log_filter = android_config->Get("Miscellaneous", "log_filter", "*:Info");

    // Debugging
    Settings::values.use_gdbstub = android_config->GetBoolean("Debugging", "use_gdbstub", false);
    Settings::values.gdbstub_port =
            static_cast<u16>(android_config->GetInteger("Debugging", "gdbstub_port", 24689));

    for (const auto& service_module : Service::service_module_map) {
        bool use_lle = android_config->GetBoolean("Debugging", "LLE\\" + service_module.name, false);
        Settings::values.lle_modules.emplace(service_module.name, use_lle);
    }

    // Web Service
    Settings::values.enable_telemetry =
            android_config->GetBoolean("WebService", "enable_telemetry", true);
    Settings::values.telemetry_endpoint_url = android_config->Get(
            "WebService", "telemetry_endpoint_url", "https://services.citra-emu.org/api/telemetry");
    Settings::values.verify_endpoint_url = android_config->Get(
            "WebService", "verify_endpoint_url", "https://services.citra-emu.org/api/profile");
    Settings::values.announce_multiplayer_room_endpoint_url =
            android_config->Get("WebService", "announce_multiplayer_room_endpoint_url",
                             "https://services.citra-emu.org/api/multiplayer/rooms");
    Settings::values.citra_username = android_config->Get("WebService", "citra_username", "");
    Settings::values.citra_token = android_config->Get("WebService", "citra_token", "");
}

void Config::Reload() {
    LoadINI(DefaultINI::android_config_file);
    ReadValues();
}
