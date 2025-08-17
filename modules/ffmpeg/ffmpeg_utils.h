/*
 * ffmpeg_codec_utils.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef FFMPEG_CODEC_UTILS_H
#define FFMPEG_CODEC_UTILS_H

extern "C" {
#include <libavcodec/avcodec.h>
#include "third_party/ffmpeg/libavformat/avformat.h"
}

#include "../../codec/codec_id.h"
#include "../../foundation/media_frame.h"
#include "../../foundation/media_meta.h"

namespace ave {
namespace media {

namespace ffmpeg_utils {

// Converts an int64 timestamp in |time_base| units to a base::TimeDelta.
// For example if |timestamp| equals 11025 and |time_base| equals {1, 44100}
// then the return value will be a base::TimeDelta for 0.25 seconds since that
// is how much time 11025/44100ths of a second represents.
int64_t ConvertFromTimeBase(const AVRational& time_base, int64_t pkt_pts);

// Converts a base::TimeDelta into an int64 timestamp in |time_base| units.
// For example if |timestamp| is 0.5 seconds and |time_base| is {1, 44100}, then
// the return value will be 22050 since that is how many 1/44100ths of a second
// represent 0.5 seconds.
int64_t ConvertToTimeBase(const AVRational& time_base, const int64_t time_us);

void ffmpeg_log_default(void* p_unused,
                        int i_level,
                        const char* psz_fmt,
                        va_list arg);

// codec info
CodecId ConvertToAVECodecId(AVCodecID ffmpeg_codec_id);
AVCodecID ConvertToFFmpegCodecId(CodecId codec_id);
const char* AVCodecId2Mime(AVCodecID codec_id);

// pixel format
AVPixelFormat ConvertToFFmpegPixelFormat(PixelFormat pixel_format);
PixelFormat ConvertFromFFmpegPixelFormat(AVPixelFormat pixel_format);

// avstream
void ExtractMetaFromAudioStream(const AVStream* audio_stream,
                                std::shared_ptr<MediaMeta>& meta);
void ExtractMetaFromVideoStream(const AVStream* video_stream,
                                std::shared_ptr<MediaMeta>& meta);
std::shared_ptr<MediaMeta> ExtractMetaFromAVStream(const AVStream* stream);

// avpacket
void ExtractMetaFromAudioPacket(const AVPacket* av_packet,
                                std::shared_ptr<MediaMeta>& meta);
void ExtractMetaFromVideoPacket(const AVPacket* av_packet,
                                std::shared_ptr<MediaMeta>& meta);
std::shared_ptr<MediaFrame> CreateMediaFrameFromAVPacket(
    const AVPacket* av_packet);

// codec
void ConfigureAudioCodec(MediaMeta* format, AVCodecContext* codec_context);
void ConfigureVideoCodec(MediaMeta* format, AVCodecContext* codec_context);

}  // namespace ffmpeg_utils
}  // namespace media
}  // namespace ave

#endif /* !FFMPEG_CODEC_UTILS_H */
