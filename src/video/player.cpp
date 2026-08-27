#include "player.hpp"

// FFmpeg includes (C API)
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

#include <cstring>
#include <chrono>
#include <thread>

namespace entergram {

// =============================================================================
// FFmpeg Video Player Implementation
//
// Uses FFmpeg libraries to decode H.264 video from MP4 containers.
// Converts NV12/YUV420P frames to RGBA for OpenGL texture upload.
//
// Key FFmpeg components:
//   - AVFormatContext: container parsing (MP4)
//   - AVCodecContext:  decoder configuration (H.264)
//   - AVFrame:         decoded frame buffer (YUV)
//   - SwsContext:      color space conversion (YUV → RGBA)
//
// Color conversion follows BT.709 standard:
//   Y' = 0.2126*R + 0.7152*G + 0.0722*B
//   The conversion is done in CPU via sws_scale for simplicity.
// =============================================================================

struct VideoPlayer::Impl {
    AVFormatContext* format_ctx = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    AVFrame* frame = nullptr;
    AVFrame* rgba_frame = nullptr;
    uint8_t* rgba_buffer = nullptr;
    int video_stream_index = -1;
    SwsContext* sws_ctx = nullptr;
    enum AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;

    bool eof = false;
    bool paused = false;
    bool has_frame = false;
    RgbaFrame pending_frame;

    ~Impl() {
        close();
    }

    void close() {
        if (sws_ctx) {
            sws_freeContext(sws_ctx);
            sws_ctx = nullptr;
        }
        if (rgba_buffer) {
            av_free(rgba_buffer);
            rgba_buffer = nullptr;
        }
        if (rgba_frame) {
            av_frame_free(&rgba_frame);
            rgba_frame = nullptr;
        }
        if (frame) {
            av_frame_free(&frame);
            frame = nullptr;
        }
        if (codec_ctx) {
            avcodec_free_context(&codec_ctx);
            codec_ctx = nullptr;
        }
        if (format_ctx) {
            avformat_close_input(&format_ctx);
            format_ctx = nullptr;
        }
        video_stream_index = -1;
        eof = false;
        paused = false;
        has_frame = false;
        pix_fmt = AV_PIX_FMT_NONE;
    }
};

VideoPlayer::VideoPlayer() : impl_(std::make_unique<Impl>()) {}

VideoPlayer::~VideoPlayer() = default;

void VideoPlayer::close() {
    impl_->close();
}

bool VideoPlayer::open(const std::string& file_path) {
    impl_->close();

    // Register all codecs/muxers (required in older FFmpeg, no-op in newer)
    // av_register_all();  // Deprecated in FFmpeg 4+

    // Open the input container
    if (avformat_open_input(&impl_->format_ctx, file_path.c_str(), nullptr, nullptr) != 0) {
        last_error_ = "Failed to open video file: " + file_path;
        return false;
    }

    // Retrieve stream information
    if (avformat_find_stream_info(impl_->format_ctx, nullptr) < 0) {
        last_error_ = "Failed to find stream info";
        impl_->close();
        return false;
    }

    // Find the video stream
    impl_->video_stream_index = av_find_best_stream(
        impl_->format_ctx,
        AVMEDIA_TYPE_VIDEO,
        -1, -1, nullptr, 0
    );
    if (impl_->video_stream_index < 0) {
        last_error_ = "No video stream found";
        impl_->close();
        return false;
    }

    AVStream* video_stream = impl_->format_ctx->streams[impl_->video_stream_index];

    // Find the decoder
    const AVCodec* codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
    if (!codec) {
        last_error_ = "Unsupported codec";
        impl_->close();
        return false;
    }

    // Allocate codec context
    impl_->codec_ctx = avcodec_alloc_context3(codec);
    if (!impl_->codec_ctx) {
        last_error_ = "Failed to allocate codec context";
        impl_->close();
        return false;
    }

    // Copy codec parameters
    if (avcodec_parameters_to_context(impl_->codec_ctx, video_stream->codecpar) < 0) {
        last_error_ = "Failed to copy codec parameters";
        avcodec_free_context(&impl_->codec_ctx);
        impl_->close();
        return false;
    }

    // Set low_delay to reduce latency
    impl_->codec_ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;

    // Open the codec
    if (avcodec_open2(impl_->codec_ctx, codec, nullptr) < 0) {
        last_error_ = "Failed to open codec";
        avcodec_free_context(&impl_->codec_ctx);
        impl_->close();
        return false;
    }

    impl_->pix_fmt = static_cast<enum AVPixelFormat>(video_stream->codecpar->format);

    // Allocate frames
    impl_->frame = av_frame_alloc();
    impl_->rgba_frame = av_frame_alloc();

    // Allocate RGBA buffer
    int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA,
        impl_->codec_ctx->width, impl_->codec_ctx->height, 1);
    impl_->rgba_buffer = (uint8_t*)av_malloc(num_bytes);
    av_image_fill_arrays(impl_->rgba_frame->data, impl_->rgba_frame->linesize,
        impl_->rgba_buffer, AV_PIX_FMT_RGBA,
        impl_->codec_ctx->width, impl_->codec_ctx->height, 1);

    // Create sws context for YUV → RGBA conversion
    impl_->sws_ctx = sws_getContext(
        impl_->codec_ctx->width, impl_->codec_ctx->height, impl_->pix_fmt,
        impl_->codec_ctx->width, impl_->codec_ctx->height, AV_PIX_FMT_RGBA,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    if (!impl_->sws_ctx) {
        last_error_ = "Failed to create sws context";
        impl_->close();
        return false;
    }

    // Prime the first frame
    impl_->has_frame = false;

    return true;
}

std::optional<RgbaFrame> VideoPlayer::read_frame() {
    if (impl_->has_frame) {
        return impl_->pending_frame;
    }

    if (impl_->paused || impl_->eof) {
        return std::nullopt;
    }

    AVPacket packet;
    av_init_packet(&packet);

    bool decoded_frame = false;
    while (!impl_->eof) {
        int ret = av_read_frame(impl_->format_ctx, &packet);
        if (ret == AVERROR_EOF || ret < 0) {
            impl_->eof = true;
            av_packet_unref(&packet);
            break;
        }

        if (packet.stream_index == impl_->video_stream_index) {
            ret = avcodec_send_packet(impl_->codec_ctx, &packet);
            if (ret >= 0) {
                ret = avcodec_receive_frame(impl_->codec_ctx, impl_->frame);
                if (ret == 0) {
                    decoded_frame = true;
                    break;  // Got a frame
                } else if (ret == AVERROR(EAGAIN) || ret == AVERROR(AVERROR(EAGAIN))) {
                    // Need more packets
                } else if (ret == AVERROR_EOF) {
                    impl_->eof = true;
                }
            }
        }
        av_packet_unref(&packet);
    }

    av_packet_unref(&packet);

    if (!decoded_frame && impl_->eof) {
        // Try draining remaining frames
        int ret = avcodec_send_packet(impl_->codec_ctx, nullptr);
        if (ret >= 0) {
            ret = avcodec_receive_frame(impl_->codec_ctx, impl_->frame);
            if (ret == 0) {
                decoded_frame = true;
            }
        }
    }

    if (!decoded_frame) {
        return std::nullopt;
    }

    // Convert YUV → RGBA using sws_scale
    sws_scale(impl_->sws_ctx,
        impl_->frame->data, impl_->frame->linesize, 0,
        impl_->codec_ctx->height,
        impl_->rgba_frame->data, impl_->rgba_frame->linesize);

    // Copy to our RgbaFrame
    RgbaFrame result;
    result.width = impl_->codec_ctx->width;
    result.height = impl_->codec_ctx->height;
    result.data.resize(result.width * result.height * 4);
    std::memcpy(result.data.data(), impl_->rgba_buffer, result.data.size());

    impl_->pending_frame = std::move(result);
    impl_->has_frame = true;

    return std::nullopt;  // Return empty, caller should call again
}

bool VideoPlayer::is_eof() const {
    return impl_->eof && !impl_->has_frame;
}

bool VideoPlayer::has_pending_frame() const {
    return impl_->has_frame;
}

void VideoPlayer::mark_frame_consumed() {
    impl_->has_frame = false;
    impl_->pending_frame = RgbaFrame{};
}

int VideoPlayer::width() const {
    return impl_->codec_ctx ? impl_->codec_ctx->width : 0;
}

int VideoPlayer::height() const {
    return impl_->codec_ctx ? impl_->codec_ctx->height : 0;
}

double VideoPlayer::frame_rate() const {
    if (!impl_->codec_ctx) return 0.0;

    AVStream* stream = impl_->format_ctx->streams[impl_->video_stream_index];
    if (stream->avg_frame_rate.den && stream->avg_frame_rate.num) {
        return static_cast<double>(stream->avg_frame_rate.num) / stream->avg_frame_rate.den;
    }

    AVRational frame_rate = av_guess_frame_rate(
        impl_->format_ctx,
        impl_->format_ctx->streams[impl_->video_stream_index],
        nullptr
    );
    if (frame_rate.den && frame_rate.num) {
        return static_cast<double>(frame_rate.num) / frame_rate.den;
    }

    return 30.0;  // Default assumption
}

void VideoPlayer::pause() {
    impl_->paused = true;
}

void VideoPlayer::resume() {
    impl_->paused = false;
}

void VideoPlayer::seek(double timestamp_seconds) {
    int64_t ts = static_cast<int64_t>(timestamp_seconds *
        impl_->format_ctx->streams[impl_->video_stream_index]->time_base.den *
        impl_->format_ctx->streams[impl_->video_stream_index]->time_base.num);
    // Actually, use the stream's time base properly
    AVRational time_base = impl_->format_ctx->streams[impl_->video_stream_index]->time_base;
    int64_t seek_target = static_cast<int64_t>(timestamp_seconds *
        static_cast<double>(time_base.den) / time_base.num);

    av_seek_frame(impl_->format_ctx, impl_->video_stream_index, seek_target, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(impl_->codec_ctx);
    impl_->eof = false;
    impl_->has_frame = false;
}

} // namespace entergram
