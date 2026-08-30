#include "player.hpp"

// FFmpeg includes (C API)
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
// swresample is included when available (installed via vcpkg)
#ifdef HAVE_SWRESAMPLE
#include <libswresample/swresample.h>
#endif
}

#include <cstring>
#include <cstdio>

namespace entergram {

struct VideoPlayer::Impl {
    AVFormatContext* format_ctx = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    AVFrame* frame = nullptr;
    AVFrame* rgba_frame = nullptr;
    uint8_t* rgba_buffer = nullptr;
    int video_stream_index = -1;
    int audio_stream_index = -1;
    SwsContext* sws_ctx = nullptr;
    enum AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;

    bool eof = false;
    bool paused = false;
    bool has_frame = false;
    RgbaFrame pending_frame;

    ~Impl() { close(); }

    void close() {
        if (sws_ctx) { sws_freeContext(sws_ctx); sws_ctx = nullptr; }
        if (rgba_buffer) { av_free(rgba_buffer); rgba_buffer = nullptr; }
        if (rgba_frame) { av_frame_free(&rgba_frame); rgba_frame = nullptr; }
        if (frame) { av_frame_free(&frame); frame = nullptr; }
        if (codec_ctx) { avcodec_free_context(&codec_ctx); codec_ctx = nullptr; }
        if (format_ctx) { avformat_close_input(&format_ctx); format_ctx = nullptr; }
        video_stream_index = -1;
        audio_stream_index = -1;
        eof = false;
        paused = false;
        has_frame = false;
        pix_fmt = AV_PIX_FMT_NONE;
    }
};

VideoPlayer::VideoPlayer() : impl_(std::make_unique<Impl>()) {}
VideoPlayer::~VideoPlayer() = default;
void VideoPlayer::close() { impl_->close(); }

bool VideoPlayer::open(const std::string& file_path) {
    impl_->close();

    if (avformat_open_input(&impl_->format_ctx, file_path.c_str(), nullptr, nullptr) != 0) {
        last_error_ = "Failed to open video file: " + file_path;
        return false;
    }

    if (avformat_find_stream_info(impl_->format_ctx, nullptr) < 0) {
        last_error_ = "Failed to find stream info";
        impl_->close();
        return false;
    }

    impl_->video_stream_index = av_find_best_stream(
        impl_->format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (impl_->video_stream_index < 0) {
        last_error_ = "No video stream found";
        impl_->close();
        return false;
    }

    impl_->audio_stream_index = av_find_best_stream(
        impl_->format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

    AVStream* video_stream = impl_->format_ctx->streams[impl_->video_stream_index];

    const AVCodec* codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
    if (!codec) {
        last_error_ = "Unsupported video codec";
        impl_->close();
        return false;
    }

    impl_->codec_ctx = avcodec_alloc_context3(codec);
    if (!impl_->codec_ctx) {
        last_error_ = "Failed to allocate codec context";
        impl_->close();
        return false;
    }

    if (avcodec_parameters_to_context(impl_->codec_ctx, video_stream->codecpar) < 0) {
        last_error_ = "Failed to copy codec parameters";
        avcodec_free_context(&impl_->codec_ctx);
        impl_->close();
        return false;
    }

    // Don't force LOW_DELAY - can cause issues with some H.264 streams
    // impl_->codec_ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;

    if (avcodec_open2(impl_->codec_ctx, codec, nullptr) < 0) {
        last_error_ = "Failed to open codec";
        avcodec_free_context(&impl_->codec_ctx);
        impl_->close();
        return false;
    }

    // Use codec context's actual output format (more reliable than codecpar)
    impl_->pix_fmt = impl_->codec_ctx->pix_fmt;
    if (impl_->pix_fmt == AV_PIX_FMT_NONE) {
        impl_->pix_fmt = AV_PIX_FMT_YUV420P;
    }
    printf("Video stream: %dx%d, pix_fmt=%d, codec=%d\n",
           impl_->codec_ctx->width, impl_->codec_ctx->height,
           (int)impl_->pix_fmt, (int)video_stream->codecpar->codec_id);

    impl_->frame = av_frame_alloc();
    impl_->rgba_frame = av_frame_alloc();

    int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA,
        impl_->codec_ctx->width, impl_->codec_ctx->height, 1);
    impl_->rgba_buffer = (uint8_t*)av_malloc(num_bytes);
    av_image_fill_arrays(impl_->rgba_frame->data, impl_->rgba_frame->linesize,
        impl_->rgba_buffer, AV_PIX_FMT_RGBA,
        impl_->codec_ctx->width, impl_->codec_ctx->height, 1);

    impl_->sws_ctx = sws_getContext(
        impl_->codec_ctx->width, impl_->codec_ctx->height, impl_->pix_fmt,
        impl_->codec_ctx->width, impl_->codec_ctx->height, AV_PIX_FMT_RGBA,
        SWS_BILINEAR, nullptr, nullptr, nullptr);

    if (!impl_->sws_ctx) {
        last_error_ = "Failed to create sws context";
        impl_->close();
        return false;
    }

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
    bool got_packet = false;
    // Process up to 10 packets per call (avoids consuming entire file)
    // Each call tries to find a video frame; if not found, returns nullopt
    // and the caller will retry next frame
    int packets_tried = 0;
    const int max_packets = 10;
    while (!impl_->eof && !decoded_frame && packets_tried < max_packets) {
        packets_tried++;
        int ret = av_read_frame(impl_->format_ctx, &packet);
            if (ret == AVERROR_EOF || ret < 0) {
                impl_->eof = true;
            av_packet_unref(&packet);
            break;
        }
        got_packet = true;
        if (packet.stream_index == impl_->video_stream_index) {
            ret = avcodec_send_packet(impl_->codec_ctx, &packet);
            if (ret >= 0) {
                ret = avcodec_receive_frame(impl_->codec_ctx, impl_->frame);
                if (ret == 0) {
                    decoded_frame = true;
                } else if (ret == AVERROR(EAGAIN)) {
                    break;
                } else if (ret == AVERROR_EOF) {
                    impl_->eof = true;
                }
            }
        }
        if (got_packet) av_packet_unref(&packet);
    }

    if (!decoded_frame) {
        return std::nullopt;
    }

    // Convert YUV → RGBA using sws_scale
    int dst_slice = sws_scale(impl_->sws_ctx,
        impl_->frame->data, impl_->frame->linesize, 0,
        impl_->codec_ctx->height,
        impl_->rgba_frame->data, impl_->rgba_frame->linesize);
    printf("  sws_scale: %d slices converted\n", dst_slice);

    RgbaFrame result;
    result.width = impl_->codec_ctx->width;
    result.height = impl_->codec_ctx->height;
    result.data.resize(result.width * result.height * 4);
    // Flip Y: copy rows in reverse order (FFmpeg outputs top-to-bottom,
    // OpenGL textures are bottom-to-top, so flip to get upright video)
    size_t row_size = result.width * 4;
    for (int y = 0; y < result.height; y++) {
        const uint8_t* src_row = impl_->rgba_buffer +
            (result.height - 1 - y) * row_size;
        std::memcpy(result.data.data() + y * row_size, src_row, row_size);
    }

    impl_->pending_frame = std::move(result);
    impl_->has_frame = true;

    printf("  Frame decoded: %dx%d\n", result.width, result.height);

    return std::nullopt;
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
    if (!impl_->codec_ctx || !impl_->format_ctx) return 0.0;

    AVStream* stream = impl_->format_ctx->streams[impl_->video_stream_index];
    if (stream->avg_frame_rate.den && stream->avg_frame_rate.num) {
        return static_cast<double>(stream->avg_frame_rate.num) / stream->avg_frame_rate.den;
    }

    AVRational fr = av_guess_frame_rate(
        impl_->format_ctx,
        impl_->format_ctx->streams[impl_->video_stream_index],
        nullptr);
    if (fr.den && fr.num) {
        return static_cast<double>(fr.num) / fr.den;
    }

    return 30.0;
}

double VideoPlayer::duration_seconds() const {
    if (!impl_->format_ctx || impl_->video_stream_index < 0) return 0.0;
    AVStream* stream = impl_->format_ctx->streams[impl_->video_stream_index];
    
    // Method 1: stream duration
    if (stream->duration != AV_NOPTS_VALUE) {
        AVRational tb = stream->time_base;
        return static_cast<double>(stream->duration) * tb.num / tb.den;
    }
    
    // Method 2: container duration
    if (impl_->format_ctx->duration != AV_NOPTS_VALUE && impl_->format_ctx->duration > 0) {
        return static_cast<double>(impl_->format_ctx->duration) / AV_TIME_BASE;
    }
    
    // Method 3: estimate from nb_frames (if available via metadata)
    if (stream->nb_frames > 0) {
        AVRational fps = av_guess_frame_rate(
            impl_->format_ctx, stream, nullptr);
        if (fps.den > 0 && fps.num > 0) {
            return static_cast<double>(stream->nb_frames) * fps.den / fps.num;
        }
    }
    
    return 0.0;
}

void VideoPlayer::pause() { impl_->paused = true; }
void VideoPlayer::resume() { impl_->paused = false; }

void VideoPlayer::seek(double timestamp_seconds) {
    if (!impl_->format_ctx || impl_->video_stream_index < 0) return;
    AVRational time_base = impl_->format_ctx->streams[impl_->video_stream_index]->time_base;
    int64_t seek_target = static_cast<int64_t>(timestamp_seconds *
        static_cast<double>(time_base.den) / time_base.num);
    // Use av_seek_frame without AVSEEK_FLAG_BACKWARD to seek to exact position
    // AVSEEK_FLAG_BACKWARD can cause issues after audio extraction consumed packets
    int ret = av_seek_frame(impl_->format_ctx, impl_->video_stream_index, 
                            seek_target, 0);
    if (ret < 0) {
        // Fallback: try with BACKWARD flag
        av_seek_frame(impl_->format_ctx, impl_->video_stream_index, 
                      seek_target, AVSEEK_FLAG_BACKWARD);
    }
    avcodec_flush_buffers(impl_->codec_ctx);
    impl_->eof = false;
    impl_->has_frame = false;
    impl_->pending_frame = RgbaFrame{};
}

bool VideoPlayer::has_audio_stream() const {
    return impl_->audio_stream_index >= 0;
}

int VideoPlayer::audio_sample_rate() const {
    if (impl_->audio_stream_index < 0 || !impl_->format_ctx) return 0;
    return impl_->format_ctx->streams[impl_->audio_stream_index]->codecpar->sample_rate;
}

std::vector<int16_t> VideoPlayer::extract_audio(double target_sr, int target_ch) {
    std::vector<int16_t> result;

#ifndef HAVE_SWRESAMPLE
    last_error_ = "swresample not available - audio extraction disabled";
    return result;
#endif

    if (impl_->audio_stream_index < 0 || !impl_->format_ctx) {
        return result;
    }

    AVStream* audio_stream = impl_->format_ctx->streams[impl_->audio_stream_index];
    const AVCodec* audio_codec = avcodec_find_decoder(audio_stream->codecpar->codec_id);
    if (!audio_codec) {
        last_error_ = "Unsupported audio codec in video";
        return result;
    }

    AVCodecContext* audio_ctx = avcodec_alloc_context3(audio_codec);
    if (!audio_ctx || avcodec_parameters_to_context(audio_ctx, audio_stream->codecpar) < 0) {
        if (audio_ctx) avcodec_free_context(&audio_ctx);
        last_error_ = "Failed to setup audio context";
        return result;
    }

    if (avcodec_open2(audio_ctx, audio_codec, nullptr) < 0) {
        avcodec_free_context(&audio_ctx);
        last_error_ = "Failed to open audio codec";
        return result;
    }

    // NOTE: We seek audio stream to beginning but do NOT touch video codec state.
    // The video read_frame() will seek back to start as needed.
    // Just read audio packets from current position forward.

    // FFmpeg 6.x: use AVChannelLayout from codecpar->ch_layout
    AVChannelLayout in_ch_layout;
    if (audio_stream->codecpar->ch_layout.nb_channels > 0) {
        in_ch_layout = audio_stream->codecpar->ch_layout;
    } else {
        av_channel_layout_default(&in_ch_layout, 1);
    }

    AVChannelLayout out_ch_layout;
    if (target_ch == 1) {
        av_channel_layout_default(&out_ch_layout, 1);
    } else {
        av_channel_layout_default(&out_ch_layout, 2);
    }

    SwrContext* swr_ctx = swr_alloc();
    if (!swr_ctx) {
        last_error_ = "Failed to allocate resampler";
        av_channel_layout_uninit(&in_ch_layout);
        av_channel_layout_uninit(&out_ch_layout);
        avcodec_free_context(&audio_ctx);
        return result;
    }

    int swr_ret = swr_alloc_set_opts2(&swr_ctx,
        &out_ch_layout, AV_SAMPLE_FMT_S16, (int)target_sr,
        &in_ch_layout, audio_ctx->sample_fmt, audio_ctx->sample_rate,
        0, nullptr);
    if (swr_ret < 0) {
        swr_free(&swr_ctx);
        last_error_ = "Failed to set resampler options";
        av_channel_layout_uninit(&in_ch_layout);
        av_channel_layout_uninit(&out_ch_layout);
        avcodec_free_context(&audio_ctx);
        return result;
    }

    if (swr_init(swr_ctx) < 0) {
        swr_free(&swr_ctx);
        last_error_ = "Failed to init resampler";
        av_channel_layout_uninit(&in_ch_layout);
        av_channel_layout_uninit(&out_ch_layout);
        avcodec_free_context(&audio_ctx);
        return result;
    }

    AVFrame* af = av_frame_alloc();
    AVPacket packet;

    printf("  Audio: %d Hz, %d ch, format=%d\n",
           audio_stream->codecpar->sample_rate,
           audio_ctx->ch_layout.nb_channels,
           (int)audio_stream->codecpar->format);

    while (true) {
        int ret = av_read_frame(impl_->format_ctx, &packet);
        if (ret < 0) break;

        if (packet.stream_index == impl_->audio_stream_index) {
            if (avcodec_send_packet(audio_ctx, &packet) >= 0) {
                while (avcodec_receive_frame(audio_ctx, af) == 0) {
                    // Calculate output buffer size
                    int max_samples = (int)av_rescale_rnd(
                        swr_get_delay(swr_ctx, audio_ctx->sample_rate) + af->nb_samples,
                        target_sr, audio_ctx->sample_rate, AV_ROUND_UP);
                    std::vector<uint8_t> out_buf(max_samples * target_ch * 2);
                    uint8_t* out_ptrs[2];
                    if (target_ch == 1) {
                        out_ptrs[0] = out_buf.data();
                        out_ptrs[1] = nullptr;
                    } else {
                        out_ptrs[0] = out_buf.data();
                        out_ptrs[1] = out_buf.data() + max_samples * 2;
                    }

                    int samples_out = swr_convert(swr_ctx,
                        out_ptrs, max_samples,
                        (const uint8_t**)af->data, af->nb_samples);

                    if (samples_out > 0) {
                        const int16_t* pcm = reinterpret_cast<const int16_t*>(out_buf.data());
                        result.insert(result.end(), pcm, pcm + samples_out * target_ch);
                    }
                }
            }
        }
        av_packet_unref(&packet);
    }

    // Drain
    avcodec_send_packet(audio_ctx, nullptr);
    while (avcodec_receive_frame(audio_ctx, af) == 0) {
        int max_samples = (int)av_rescale_rnd(
            swr_get_delay(swr_ctx, audio_ctx->sample_rate) + af->nb_samples,
            target_sr, audio_ctx->sample_rate, AV_ROUND_UP);
        std::vector<uint8_t> out_buf(max_samples * target_ch * 2);
        uint8_t* out_ptrs[2];
        if (target_ch == 1) {
            out_ptrs[0] = out_buf.data();
            out_ptrs[1] = nullptr;
        } else {
            out_ptrs[0] = out_buf.data();
            out_ptrs[1] = out_buf.data() + max_samples * 2;
        }

        int samples_out = swr_convert(swr_ctx,
            out_ptrs, max_samples,
            (const uint8_t**)af->data, af->nb_samples);

        if (samples_out > 0) {
            const int16_t* pcm = reinterpret_cast<const int16_t*>(out_buf.data());
            result.insert(result.end(), pcm, pcm + samples_out * target_ch);
        }
    }

    av_frame_free(&af);
    swr_free(&swr_ctx);
    avcodec_free_context(&audio_ctx);
    av_channel_layout_uninit(&in_ch_layout);
    av_channel_layout_uninit(&out_ch_layout);

    // Reset state for continued video playback
    impl_->eof = false;
    impl_->has_frame = false;

    printf("  Audio extracted: %zu samples (%zu bytes)\n",
           result.size(), result.size() * sizeof(int16_t));
    return result;
}

} // namespace entergram
