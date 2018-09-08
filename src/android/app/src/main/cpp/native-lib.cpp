#include <jni.h>
#include <string>
#include <memory>
#include <utility>
#include <string>
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
void EmuWindow_Android::PollEvents(){}
void EmuWindow_Android::MakeCurrent(){}
void EmuWindow_Android::DoneCurrent(){}

extern "C" {
JNIEXPORT void JNICALL
Java_org_citra_1emu_citra_MainActivity_Load(JNIEnv *env, jobject obj, jstring jpath) {
    std::unique_ptr<EmuWindow_Android> emu_window = std::make_unique<EmuWindow_Android>();
    Core::System &system{Core::System::GetInstance()};
    std::string path = env->GetStringUTFChars(jpath, 0);
    const Core::System::ResultStatus load_result{system.Load(*emu_window, path)};
}
}
