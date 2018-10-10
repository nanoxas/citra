// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/file_util.h"
#include "common/logging/log.h"
#include "video_core/frame_dumper.h"

FrameDumper::FrameData::FrameData(size_t width_, size_t height_, u8* data_)
    : width(width_), height(height_) {
    // rotate the data by 270 degrees while copying
    stride = width * 4;
    data.resize(width * height * 4);
    for (size_t x = 0; x < height; x++) {
        for (size_t y = 0; y < width; y++) {
            for (size_t k = 0; k < 4; k++) {
                data[(height - 1 - x) * stride + y * 4 + k] = data_[y * height * 4 + x * 4 + k];
            }
        }
    }
}

void FrameDumper::Initialize() {
    static bool initialized = false;
    if (initialized)
        return;
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
    av_register_all();
#endif
    avformat_network_init();
    initialized = true;
}

bool FrameDumper::InitializeDumping() {
    if (!FileUtil::CreateFullPath(dump_path)) {
        return false;
    }

    frame_count = 0;

    // Get output format
    AVOutputFormat* output_format =
        av_guess_format(dump_format.c_str(), dump_path.c_str(), nullptr);
    if (!output_format) {
        LOG_ERROR(Render, "Could not get format {}", dump_format);
        return false;
    }

    // Initialize format context
    if (avformat_alloc_output_context2(&format_context, output_format, nullptr, dump_path.c_str()) <
        0) {
        LOG_ERROR(Render, "Could not allocate output context");
        return false;
    }

    // Initialize codec
    AVCodecID codec_id = output_format->video_codec;
    const AVCodec* codec = avcodec_find_encoder(codec_id);
    codec_context = avcodec_alloc_context3(codec);
    if (!codec || !codec_context) {
        LOG_ERROR(Render, "Could not find encoder or allocate codec context");
        return false;
    }

    // Configure codec context
    // Force XVID FourCC for better compatibility
    if (codec->id == AV_CODEC_ID_MPEG4)
        codec_context->codec_tag = MKTAG('X', 'V', 'I', 'D');
    codec_context->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_context->bit_rate = 2500000;
    codec_context->width = width;
    codec_context->height = height;
    codec_context->time_base.num = 1;
    codec_context->time_base.den = 60;
    codec_context->gop_size = 12;
    codec_context->pix_fmt = AV_PIX_FMT_YUV420P;
    if (output_format->flags & AVFMT_GLOBALHEADER)
        codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(codec_context, codec, nullptr) < 0) {
        LOG_ERROR(Render, "Could not open codec");
        return false;
    }

    // Allocate frames
    current_frame = av_frame_alloc();
    scaled_frame = av_frame_alloc();
    scaled_frame->format = codec_context->pix_fmt;
    scaled_frame->width = width;
    scaled_frame->height = height;
    if (av_frame_get_buffer(scaled_frame, 1) < 0) {
        LOG_ERROR(Render, "Could not allocate frame buffer");
        return false;
    }

    // Create stream
    stream = avformat_new_stream(format_context, codec);
    if (!stream || avcodec_parameters_from_context(stream->codecpar, codec_context) < 0) {
        LOG_ERROR(Render, "Could not create stream");
        return false;
    }

    // Open video file
    if (avio_open(&format_context->pb, dump_path.c_str(), AVIO_FLAG_WRITE) < 0 ||
        avformat_write_header(format_context, nullptr)) {
        LOG_ERROR(Render, "Could not open {}", dump_path);
        return false;
    }

    LOG_INFO(Render, "Dumping frames to {} ({}x{})", dump_path, width, height);
    return true;
}

void FrameDumper::FreeResources() {
    av_frame_free(&current_frame);

    avcodec_free_context(&codec_context);

    if (format_context) {
        avio_closep(&format_context->pb);
    }
    avformat_free_context(format_context);
    format_context = nullptr;

    if (sws_context) {
        sws_freeContext(sws_context);
        sws_context = nullptr;
    }
}

void FrameDumper::WritePacket(AVPacket& packet) {
    if (packet.pts != static_cast<s64>(AV_NOPTS_VALUE)) {
        packet.pts = av_rescale_q(packet.pts, codec_context->time_base, stream->time_base);
    }
    if (packet.dts != static_cast<s64>(AV_NOPTS_VALUE)) {
        packet.dts = av_rescale_q(packet.dts, codec_context->time_base, stream->time_base);
    }
    packet.stream_index = stream->index;
    av_interleaved_write_frame(format_context, &packet);
}

bool FrameDumper::StartDumping(const std::string& path, const std::string& format, int width,
                               int height) {
    dump_path = path;
    dump_format = format;
    this->width = width;
    this->height = height;
    if (!InitializeDumping()) {
        FreeResources();
        return false;
    }
    if (frame_processing_thread.joinable())
        frame_processing_thread.join();
    frame_processing_thread = std::thread([&] {
        FrameData frame;
        while (frame_queue.PopWait(frame)) {
            if (frame.width == 0 && frame.height == 0) {
                // An empty frame marks the end of frame data
                break;
            }
            ProcessFrame(frame);
        }
        EndDumping();
    });
    return true;
}

void FrameDumper::AddFrame(FrameData& frame) {
    frame_queue.Push(std::move(frame));
}

void FrameDumper::ProcessFrame(FrameData& frame) {
    if (frame.width != width || frame.height != height) {
        LOG_ERROR(Render, "Frame dropped: resolution does not match");
        return;
    }
    // Prepare frame
    current_frame->data[0] = frame.data.data();
    current_frame->linesize[0] = frame.stride;
    current_frame->format = pixel_format;
    current_frame->width = width;
    current_frame->height = height;

    // Scale the frame
    sws_context =
        sws_getCachedContext(sws_context, width, height, pixel_format, width, height,
                             codec_context->pix_fmt, SWS_BICUBIC, nullptr, nullptr, nullptr);
    if (sws_context) {
        sws_scale(sws_context, current_frame->data, current_frame->linesize, 0, height,
                  scaled_frame->data, scaled_frame->linesize);
    }
    scaled_frame->pts = frame_count++;

    // Initialize packet
    AVPacket packet;
    av_init_packet(&packet);
    packet.data = nullptr;
    packet.size = 0;

    // Encode frame
    if (avcodec_send_frame(codec_context, scaled_frame) < 0) {
        LOG_ERROR(Render, "Frame dropped: could not send frame");
        return;
    }
    int error = avcodec_receive_packet(codec_context, &packet);
    if (error < 0 && error != AVERROR(EAGAIN)) {
        LOG_ERROR(Render, "Frame dropped: could not encode video");
        return;
    }

    // Write frame to video file
    if (error >= 0)
        WritePacket(packet);
}

void FrameDumper::EndDumping() {
    LOG_INFO(Render, "Ending frame dumping");
    av_write_trailer(format_context);
    FreeResources();
}
