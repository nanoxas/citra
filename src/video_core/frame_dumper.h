// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <thread>
#include <vector>
#include "common/common_types.h"
#include "common/threadsafe_queue.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

/**
 * Dumps the frames drawn to a video (usually an AVI), at native resolution.
 * Note: each instance of this class only dumps a single screen.
 */
class FrameDumper {
public:
    /**
     * Frame dump data for a single screen
     * data is in RGB888 format, left to right then top to bottom
     */
    class FrameData {
    public:
        size_t width;
        size_t height;
        u32 stride;
        std::vector<u8> data;

        FrameData(size_t width_ = 0, size_t height_ = 0, u8* data_ = nullptr);
    };

    /// Initializes ffmpeg libraries. Does nothing if ffmpeg is already initialized.
    static void Initialize();
    void Shutdown();

    bool StartDumping(const std::string& path, const std::string& format, int width, int height);
    bool IsDumpingFrames();

    void AddFrame(FrameData& frame);

private:
    bool InitializeDumping();
    void FreeResources();
    /// Writes the encoded frame to the video file
    void WritePacket(AVPacket& packet);
    void ProcessFrame(FrameData& frame);
    void EndDumping();

    int width, height;
    bool is_dumping = false;
    std::string dump_path, dump_format;

    AVFormatContext* format_context{};
    AVCodecContext* codec_context{};
    AVStream* stream{};
    AVFrame *current_frame{}, *scaled_frame{};
    SwsContext* sws_context{};

    u64 frame_count{};
    Common::SPSCQueue<FrameData> frame_queue;

    std::thread frame_processing_thread;

    /// The pixel format the frames are stored in
    static constexpr AVPixelFormat pixel_format = AVPixelFormat::AV_PIX_FMT_BGRA;
};
