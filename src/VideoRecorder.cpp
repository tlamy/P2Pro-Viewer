#include "VideoRecorder.hpp"
#include "P2Pro.hpp" // For dprintf
#include "ColorConversion.hpp"
#include <ctime>
#include <iomanip>
#include <sstream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

VideoRecorder::VideoRecorder() {}

VideoRecorder::~VideoRecorder() {
    stop();
}

std::string VideoRecorder::generateFilename() const {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S") << ".mp4";
    return oss.str();
}

bool VideoRecorder::start(int w, int h, double f) {
    if (recording) return false;

    filename = generateFilename();
    width = w;
    height = h;
    fps = f;
    frame_count = 0;

    // 1. Allocate output context
    if (avformat_alloc_output_context2(&fmt_ctx, NULL, NULL, filename.c_str()) < 0) {
        dprintf("VideoRecorder::start() - Could not allocate output context\n");
        return false;
    }

    // 2. Find H.264 encoder
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        dprintf("VideoRecorder::start() - H.264 encoder not found, trying default\n");
        codec = avcodec_find_encoder(fmt_ctx->oformat->video_codec);
    }
    if (!codec) {
        dprintf("VideoRecorder::start() - No video encoder found\n");
        cleanup();
        return false;
    }

    // 3. Create stream
    stream = avformat_new_stream(fmt_ctx, NULL);
    if (!stream) {
        dprintf("VideoRecorder::start() - Could not create stream\n");
        cleanup();
        return false;
    }
    stream->id = fmt_ctx->nb_streams - 1;

    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        dprintf("VideoRecorder::start() - Could not allocate codec context\n");
        cleanup();
        return false;
    }

    codec_ctx->codec_id = codec->id;
    codec_ctx->bit_rate = 400000;
    codec_ctx->width = width;
    codec_ctx->height = height;
    stream->time_base = (AVRational){1, (int)fps};
    codec_ctx->time_base = stream->time_base;
    codec_ctx->gop_size = 12;
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

    if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    // Set options
    av_opt_set(codec_ctx->priv_data, "preset", "ultrafast", 0);
    av_opt_set(codec_ctx->priv_data, "tune", "zerolatency", 0);

    // 4. Open codec
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        dprintf("VideoRecorder::start() - Could not open codec\n");
        cleanup();
        return false;
    }

    if (avcodec_parameters_from_context(stream->codecpar, codec_ctx) < 0) {
        dprintf("VideoRecorder::start() - Could not copy codec parameters\n");
        cleanup();
        return false;
    }

    // 5. Open output file
    if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&fmt_ctx->pb, filename.c_str(), AVIO_FLAG_WRITE) < 0) {
            dprintf("VideoRecorder::start() - Could not open '%s'\n", filename.c_str());
            cleanup();
            return false;
        }
    }

    // 6. Write header
    if (avformat_write_header(fmt_ctx, NULL) < 0) {
        dprintf("VideoRecorder::start() - Error occurred when opening output file\n");
        cleanup();
        return false;
    }

    // 7. Allocate frame
    frame = av_frame_alloc();
    if (!frame) {
        dprintf("VideoRecorder::start() - Could not allocate video frame\n");
        cleanup();
        return false;
    }
    frame->format = codec_ctx->pix_fmt;
    frame->width = codec_ctx->width;
    frame->height = codec_ctx->height;

    if (av_frame_get_buffer(frame, 0) < 0) {
        dprintf("VideoRecorder::start() - Could not allocate the video frame data\n");
        cleanup();
        return false;
    }

    // 8. Initialize SWS context for RGB to YUV420P conversion
    sws_ctx = sws_getContext(width, height, AV_PIX_FMT_RGB24,
                             width, height, AV_PIX_FMT_YUV420P,
                             SWS_BICUBIC, NULL, NULL, NULL);

    dprintf("VideoRecorder::start() - Recording started (FFmpeg): %s (%dx%d @ %.1f FPS)\n", filename.c_str(), width, height, fps);
    recording = true;
    return true;
}

void VideoRecorder::stop() {
    if (!recording) return;

    // Flush encoder
    writeFrame({});

    av_write_trailer(fmt_ctx);
    cleanup();

    recording = false;
    dprintf("VideoRecorder::stop() - Recording stopped: %s\n", filename.c_str());
}

void VideoRecorder::cleanup() {
    if (codec_ctx) avcodec_free_context(&codec_ctx);
    if (frame) av_frame_free(&frame);
    if (sws_ctx) sws_freeContext(sws_ctx);
    if (fmt_ctx) {
        if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&fmt_ctx->pb);
        avformat_free_context(fmt_ctx);
    }
    fmt_ctx = nullptr;
    codec_ctx = nullptr;
    frame = nullptr;
    sws_ctx = nullptr;
    stream = nullptr;
}

void VideoRecorder::writeFrame(const std::vector<uint8_t>& rgb_data) {
    if (!recording) return;

    if (!rgb_data.empty()) {
        if (rgb_data.size() != static_cast<size_t>(width * height * 3)) return;

        // Convert RGB24 to YUV420P
        const uint8_t* inData[1] = { rgb_data.data() };
        int inLinesize[1] = { 3 * width };
        sws_scale(sws_ctx, inData, inLinesize, 0, height, frame->data, frame->linesize);

        frame->pts = frame_count++;
    }

    // Encode
    int ret = avcodec_send_frame(codec_ctx, rgb_data.empty() ? NULL : frame);
    if (ret < 0) return;

    while (ret >= 0) {
        AVPacket* pkt = av_packet_alloc();
        ret = avcodec_receive_packet(codec_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_packet_free(&pkt);
            break;
        } else if (ret < 0) {
            av_packet_free(&pkt);
            break;
        }

        av_packet_rescale_ts(pkt, codec_ctx->time_base, stream->time_base);
        pkt->stream_index = stream->index;
        av_interleaved_write_frame(fmt_ctx, pkt);
        av_packet_free(&pkt);
    }
}
