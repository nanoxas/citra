#include <jni.h>
#include <string>
#include <memory>
#include <utility>
#include <string>
#include "config.h"
#include "core/core.h"
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include "common/logging/log.h"
#include "core/settings.h"
#include "common/common_paths.h"
#include "emuwindowandroid.h"

extern "C" {
JNIEXPORT void JNICALL
Java_org_citra_1emu_citra_MainActivity_Load(JNIEnv *env, jobject obj, jstring jpath) {
    std::string path = env->GetStringUTFChars(jpath, 0);
    Config config;
    std::unique_ptr<EmuWindow_Android> emu_window = std::make_unique<EmuWindow_Android>();
    Core::System &system{Core::System::GetInstance()};
    const Core::System::ResultStatus load_result{system.Load(*emu_window, path)};
} ;
}

extern "C" {
JNIEXPORT void JNICALL
Java_org_citra_1emu_citra_MainActivity_setFilePaths(JNIEnv *env, jobject obj,
                                                    jstring external_file_path,
                                                    jstring cache_path) {
    FileUtil::external_files_path = env->GetStringUTFChars(external_file_path, 0);
    FileUtil::cache_path = env->GetStringUTFChars(cache_path, 0);
} ;
}

extern "C" {
JNIEXPORT void JNICALL
Java_org_citra_1emu_citra_MainActivity_initLogging(JNIEnv *env, jobject obj) {
    Log::Filter log_filter;
    log_filter.ParseFilterString(Settings::values.log_filter);
    Log::SetGlobalFilter(log_filter);

    Log::AddBackend(std::make_unique<Log::ColorConsoleBackend>());
    FileUtil::CreateFullPath(FileUtil::GetUserPath(D_LOGS_IDX));
    Log::AddBackend(
            std::make_unique<Log::FileBackend>(FileUtil::GetUserPath(D_LOGS_IDX) + LOG_FILE));
} ;
}
