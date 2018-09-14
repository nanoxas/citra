#include <jni.h>
#include <string>
#include <memory>
#include <utility>
#include <string>
#include "config.h"
#include "core/frontend/emu_window.h"
#include "core/core.h"

class EmuWindow_Android : public EmuWindow {
public:
    explicit EmuWindow_Android();

    ~EmuWindow_Android();

    /// Swap buffers to display the next frame
    void SwapBuffers() override;

    /// Polls window events
    void PollEvents() override;

    /// Makes the graphics context current for the caller thread
    void MakeCurrent() override;

    /// Releases the GL context from the caller thread
    void DoneCurrent() override;

private:
};

EmuWindow_Android::EmuWindow_Android() = default;

EmuWindow_Android::~EmuWindow_Android() = default;

void EmuWindow_Android::SwapBuffers() {}

void EmuWindow_Android::PollEvents() {}

void EmuWindow_Android::MakeCurrent() {}

void EmuWindow_Android::DoneCurrent() {}

namespace Citra::Android {
    void Load(std::string path) {
        Config config;
        std::unique_ptr<EmuWindow_Android> emu_window = std::make_unique<EmuWindow_Android>();
        Core::System &system{Core::System::GetInstance()};
        const Core::System::ResultStatus load_result{system.Load(*emu_window, path)};
    }
}

extern "C" {
JNIEXPORT void JNICALL
Java_org_citra_1emu_citra_MainActivity_Load(JNIEnv *env, jobject obj, jstring jpath) {
    Citra::Android::Load(env->GetStringUTFChars(jpath, 0));
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
