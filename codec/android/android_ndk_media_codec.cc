/*
 * android_ndk_media_codec.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "android_ndk_media_codec.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include <android/native_window.h>
#include <media/NdkMediaError.h>
#include <media/NdkMediaFormat.h>

#include "base/attributes.h"
#include "base/errors.h"
#include "base/logging.h"
#include "media/NdkMediaCodec.h"
#include "media/audio/channel_layout.h"
#include "media/foundation/media_errors.h"
#include "media/foundation/media_mimes.h"
#include "media/foundation/pixel_format.h"

namespace ave {
namespace media {

namespace {

#if defined(__ANDROID__)
constexpr char kDebugVideoDumpPath[] =
    "/data/user/0/io.github.yoofa.avpdemo/cache/avp_video_input.h264";
constexpr size_t kDebugVideoDumpLimitBytes = 32 * 1024 * 1024;

void DumpDebugVideoInput(MediaType media_type,
                         const std::string& mime,
                         const void* data,
                         size_t size,
                         uint32_t flags) {
  if (media_type != MediaType::VIDEO || mime != MEDIA_MIMETYPE_VIDEO_AVC ||
      !data || size == 0 ||
      (flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) != 0) {
    return;
  }

  static FILE* dump_file = nullptr;
  static size_t dumped_bytes = 0;
  static bool open_failed = false;

  if (dumped_bytes >= kDebugVideoDumpLimitBytes || open_failed) {
    return;
  }

  if (!dump_file) {
    dump_file = std::fopen(kDebugVideoDumpPath, "wb");
    if (!dump_file) {
      open_failed = true;
      AVE_LOG(LS_WARNING) << "Failed to open debug video dump: "
                          << kDebugVideoDumpPath;
      return;
    }
    dumped_bytes = 0;
    AVE_LOG(LS_INFO) << "Debug video dump enabled: " << kDebugVideoDumpPath;
  }

  const size_t write_size =
      std::min(size, kDebugVideoDumpLimitBytes - dumped_bytes);
  const size_t written = std::fwrite(data, 1, write_size, dump_file);
  std::fflush(dump_file);
  dumped_bytes += written;
}
#endif

// Map media_status_t to ave status_t
status_t MapMediaStatus(media_status_t status) {
  switch (status) {
    case AMEDIA_OK:
      return OK;
    case AMEDIA_ERROR_UNKNOWN:
      return UNKNOWN_ERROR;
    case AMEDIA_ERROR_MALFORMED:
      return ERROR_MALFORMED;
    case AMEDIA_ERROR_UNSUPPORTED:
      return ERROR_UNSUPPORTED;
    case AMEDIA_ERROR_INVALID_OBJECT:
      return INVALID_OPERATION;
    case AMEDIA_ERROR_INVALID_PARAMETER:
      return BAD_VALUE;
    default:
      return UNKNOWN_ERROR;
  }
}

// ---------------------------------------------------------------------------
// AVCC → Annex B CSD conversion for H.264/AVC.
//
// FFmpeg's MP4 demuxer stores the avcC configuration record (AVCC format) in
// extradata/private_data.  Android MediaCodec expects Annex B format:
//   csd-0: 00 00 00 01 <SPS NALU>
//   csd-1: 00 00 00 01 <PPS NALU>
//
// Note: The actual access unit packets are already converted to Annex B by the
// h264_mp4toannexb bitstream filter in FFmpegDemuxer — only the CSD needs
// conversion here.
// ---------------------------------------------------------------------------

/**
 * @brief Check if H.264 extradata is in AVCC format (avcC configuration record)
 * @param data Extradata pointer
 * @param size Extradata size in bytes
 * @return true if AVCC format (configurationVersion == 1 and not a start code)
 */
bool IsAvccFormat(const uint8_t* data, size_t size) {
  if (!data || size < 7) {
    return false;
  }
  // AVCC configurationVersion must be 1
  if (data[0] != 0x01) {
    return false;
  }
  // Ensure it's not already Annex B (start code 00 00 00 01 or 00 00 01)
  if (size >= 4 && data[0] == 0x00 && data[1] == 0x00) {
    return false;
  }
  return true;
}

/**
 * @brief Convert AVCC configuration record to Annex B csd-0 (SPS) and csd-1
 *        (PPS) buffers for Android MediaCodec.
 * @param avcc Raw avcC box data from MP4 container
 * @param avcc_size Size of avcc data
 * @param[out] csd0 SPS with Annex B start code(s)
 * @param[out] csd1 PPS with Annex B start code(s)
 * @return true on success, false if parsing fails
 */
bool ConvertAvccToAnnexBCsd(const uint8_t* avcc,
                            size_t avcc_size,
                            std::vector<uint8_t>& csd0,
                            std::vector<uint8_t>& csd1) {
  // AVCC record layout:
  //   [0]    configurationVersion (1)
  //   [1]    AVCProfileIndication
  //   [2]    profile_compatibility
  //   [3]    AVCLevelIndication
  //   [4]    0xFC | (lengthSizeMinusOne & 0x03)
  //   [5]    0xE0 | (numSPS & 0x1F)
  //   For each SPS: [2 bytes length] [SPS data]
  //   [1 byte] numPPS
  //   For each PPS: [2 bytes length] [PPS data]
  if (avcc_size < 7) {
    AVE_LOG(LS_ERROR) << "ConvertAvccToAnnexBCsd: too small (" << avcc_size
                      << " bytes)";
    return false;
  }

  static const uint8_t kStartCode[] = {0x00, 0x00, 0x00, 0x01};

  size_t offset = 5;

  // Parse SPS entries
  uint8_t num_sps = avcc[offset] & 0x1F;
  offset++;
  AVE_LOG(LS_INFO) << "ConvertAvccToAnnexBCsd: num_sps=" << (int)num_sps;

  for (uint8_t i = 0; i < num_sps; i++) {
    if (offset + 2 > avcc_size) {
      AVE_LOG(LS_ERROR) << "ConvertAvccToAnnexBCsd: SPS length overflow";
      return false;
    }
    uint16_t sps_len =
        (static_cast<uint16_t>(avcc[offset]) << 8) | avcc[offset + 1];
    offset += 2;
    if (offset + sps_len > avcc_size) {
      AVE_LOG(LS_ERROR) << "ConvertAvccToAnnexBCsd: SPS data overflow, len="
                        << sps_len;
      return false;
    }
    csd0.insert(csd0.end(), kStartCode, kStartCode + 4);
    csd0.insert(csd0.end(), avcc + offset, avcc + offset + sps_len);
    AVE_LOG(LS_INFO) << "ConvertAvccToAnnexBCsd: SPS[" << (int)i
                     << "] len=" << sps_len;
    offset += sps_len;
  }

  // Parse PPS entries
  if (offset >= avcc_size) {
    AVE_LOG(LS_ERROR) << "ConvertAvccToAnnexBCsd: no PPS data";
    return false;
  }
  uint8_t num_pps = avcc[offset];
  offset++;
  AVE_LOG(LS_INFO) << "ConvertAvccToAnnexBCsd: num_pps=" << (int)num_pps;

  for (uint8_t i = 0; i < num_pps; i++) {
    if (offset + 2 > avcc_size) {
      AVE_LOG(LS_ERROR) << "ConvertAvccToAnnexBCsd: PPS length overflow";
      return false;
    }
    uint16_t pps_len =
        (static_cast<uint16_t>(avcc[offset]) << 8) | avcc[offset + 1];
    offset += 2;
    if (offset + pps_len > avcc_size) {
      AVE_LOG(LS_ERROR) << "ConvertAvccToAnnexBCsd: PPS data overflow, len="
                        << pps_len;
      return false;
    }
    csd1.insert(csd1.end(), kStartCode, kStartCode + 4);
    csd1.insert(csd1.end(), avcc + offset, avcc + offset + pps_len);
    AVE_LOG(LS_INFO) << "ConvertAvccToAnnexBCsd: PPS[" << (int)i
                     << "] len=" << pps_len;
    offset += pps_len;
  }

  return !csd0.empty() && !csd1.empty();
}

/**
 * @brief Check if HEVC extradata is in HVCC format (hvcC configuration record)
 * @param data Extradata pointer
 * @param size Extradata size in bytes
 * @return true if HVCC format
 */
bool IsHvccFormat(const uint8_t* data, size_t size) {
  if (!data || size < 23) {
    return false;
  }
  // HVCC configurationVersion must be 1
  if (data[0] != 0x01) {
    return false;
  }
  // Ensure it's not already Annex B
  if (data[0] == 0x00 && data[1] == 0x00) {
    return false;
  }
  return true;
}

/**
 * @brief Convert HVCC configuration record to Annex B csd-0 (VPS+SPS+PPS)
 *        buffer for Android MediaCodec.
 * @param hvcc Raw hvcC box data
 * @param hvcc_size Size of hvcc data
 * @param[out] csd0 All parameter sets (VPS, SPS, PPS) with start codes
 * @return true on success
 */
bool ConvertHvccToAnnexBCsd(const uint8_t* hvcc,
                            size_t hvcc_size,
                            std::vector<uint8_t>& csd0) {
  // HVCC layout:
  //   [0..21] header (configurationVersion + various fields)
  //   [22]    numOfArrays
  //   For each array:
  //     [0]    array_completeness(1) | reserved(1) | NAL_unit_type(6)
  //     [1..2] numNalus
  //     For each NALU: [2 bytes length] [NALU data]
  if (hvcc_size < 23) {
    return false;
  }

  static const uint8_t kStartCode[] = {0x00, 0x00, 0x00, 0x01};

  size_t offset = 22;
  uint8_t num_arrays = hvcc[offset];
  offset++;

  for (uint8_t arr = 0; arr < num_arrays; arr++) {
    if (offset + 3 > hvcc_size) {
      return false;
    }
    // uint8_t nal_type = hvcc[offset] & 0x3F;
    offset++;
    uint16_t num_nalus =
        (static_cast<uint16_t>(hvcc[offset]) << 8) | hvcc[offset + 1];
    offset += 2;

    for (uint16_t n = 0; n < num_nalus; n++) {
      if (offset + 2 > hvcc_size) {
        return false;
      }
      uint16_t nalu_len =
          (static_cast<uint16_t>(hvcc[offset]) << 8) | hvcc[offset + 1];
      offset += 2;
      if (offset + nalu_len > hvcc_size) {
        return false;
      }
      csd0.insert(csd0.end(), kStartCode, kStartCode + 4);
      csd0.insert(csd0.end(), hvcc + offset, hvcc + offset + nalu_len);
      offset += nalu_len;
    }
  }

  return !csd0.empty();
}

// ---------------------------------------------------------------------------
// NDK async callback free functions (AMediaCodecOnAsyncNotifyCallback).
// userdata is AndroidNdkMediaCodec*.  These are called on an internal looper
// thread managed by the AMediaCodec implementation.
// ---------------------------------------------------------------------------

void OnAsyncInputAvailable(AMediaCodec* /*codec*/,
                           void* userdata,
                           int32_t index) {
  AVE_LOG(LS_VERBOSE) << "NDK cb: input available, index=" << index;
  reinterpret_cast<AndroidNdkMediaCodec*>(userdata)->NotifyInputBufferAvailable(
      static_cast<size_t>(index));
}

void OnAsyncOutputAvailable(AMediaCodec* /*codec*/,
                            void* userdata,
                            int32_t index,
                            AMediaCodecBufferInfo* info) {
  AVE_LOG(LS_INFO) << "NDK cb: output available, index=" << index
                   << ", size=" << (info ? info->size : -1)
                   << ", pts=" << (info ? info->presentationTimeUs : -1)
                   << ", flags=" << (info ? info->flags : -1);
  reinterpret_cast<AndroidNdkMediaCodec*>(userdata)
      ->NotifyOutputBufferAvailable(
          static_cast<size_t>(index), info ? info->offset : 0,
          info ? info->size : 0, info ? info->presentationTimeUs : 0,
          info ? static_cast<uint32_t>(info->flags) : 0);
}

void OnAsyncFormatChanged(AMediaCodec* /*codec*/,
                          void* userdata,
                          AMediaFormat* format) {
  AVE_LOG(LS_INFO) << "NDK cb: format changed: "
                   << (format ? AMediaFormat_toString(format) : "null");
  reinterpret_cast<AndroidNdkMediaCodec*>(userdata)->NotifyOutputFormatChanged(
      format);
}

void OnAsyncError(AMediaCodec* /*codec*/,
                  void* userdata,
                  media_status_t error,
                  int32_t action_code,
                  const char* detail) {
  AVE_LOG(LS_ERROR) << "NDK cb: error=" << error
                    << ", action_code=" << action_code
                    << ", detail=" << (detail ? detail : "none");
  reinterpret_cast<AndroidNdkMediaCodec*>(userdata)->NotifyError(
      MapMediaStatus(error));
}

}  // namespace

AndroidNdkMediaCodec::AndroidNdkMediaCodec(const char* mime_or_name,
                                           CreatedBy type,
                                           bool encoder)
    : is_encoder_(encoder) {
  AVE_LOG(LS_INFO) << "AndroidNdkMediaCodec: creating codec, mime_or_name="
                   << mime_or_name << ", type="
                   << (type == kCreatedByName ? "byName" : "byMime")
                   << ", encoder=" << encoder;

  if (type == kCreatedByName) {
    android_media_codec_ = AMediaCodec_createCodecByName(mime_or_name);
  } else if (encoder) {
    android_media_codec_ = AMediaCodec_createEncoderByType(mime_or_name);
  } else {
    android_media_codec_ = AMediaCodec_createDecoderByType(mime_or_name);
  }

  if (!android_media_codec_) {
    AVE_LOG(LS_ERROR) << "AndroidNdkMediaCodec: failed to create codec for "
                      << mime_or_name;
  } else {
    AVE_LOG(LS_INFO) << "AndroidNdkMediaCodec: codec created successfully";
  }
}

AndroidNdkMediaCodec::~AndroidNdkMediaCodec() {
  AVE_LOG(LS_VERBOSE) << "AndroidNdkMediaCodec: destructor";
  if (android_media_codec_) {
    AMediaCodec_delete(android_media_codec_);
    android_media_codec_ = nullptr;
  }
}

AMediaFormat* AndroidNdkMediaCodec::MediaMetaToAMediaFormat(
    const std::shared_ptr<MediaMeta>& format) {
  AMediaFormat* ndk_format = AMediaFormat_new();
  if (!ndk_format) {
    AVE_LOG(LS_ERROR) << "MediaMetaToAMediaFormat: AMediaFormat_new failed";
    return nullptr;
  }

  const std::string& mime = format->mime();
  AMediaFormat_setString(ndk_format, AMEDIAFORMAT_KEY_MIME, mime.c_str());
  AVE_LOG(LS_VERBOSE) << "MediaMetaToAMediaFormat: mime=" << mime;

  if (format->stream_type() == MediaType::VIDEO) {
    int32_t width = format->width();
    int32_t height = format->height();
    if (width > 0 && height > 0) {
      AMediaFormat_setInt32(ndk_format, AMEDIAFORMAT_KEY_WIDTH, width);
      AMediaFormat_setInt32(ndk_format, AMEDIAFORMAT_KEY_HEIGHT, height);
      AVE_LOG(LS_VERBOSE) << "MediaMetaToAMediaFormat: video " << width << "x"
                          << height;
    }

    int32_t fps = format->fps();
    if (fps > 0) {
      AMediaFormat_setInt32(ndk_format, AMEDIAFORMAT_KEY_FRAME_RATE, fps);
      AVE_LOG(LS_VERBOSE) << "MediaMetaToAMediaFormat: fps=" << fps;
    }

    // Only set profile/level for encoders. Decoders read profile/level from
    // the bitstream (SPS NALU inside csd-0) and don't need — and may reject —
    // externally supplied values. FFmpeg profile constants (e.g. 578 for
    // Constrained Baseline = 66|512) differ from Android MediaCodec constants
    // (e.g. AVCProfileBaseline=1), so passing FFmpeg values to a hardware
    // decoder can cause silent failures on strict implementations like
    // Qualcomm c2.qti.avc.decoder.
    if (is_encoder_) {
      int32_t profile = format->codec_profile();
      if (profile > 0) {
        if (__builtin_available(android 28, *)) {
          AMediaFormat_setInt32(ndk_format, AMEDIAFORMAT_KEY_PROFILE, profile);
        }
      }

      int32_t level = format->codec_level();
      if (level > 0) {
        if (__builtin_available(android 28, *)) {
          AMediaFormat_setInt32(ndk_format, AMEDIAFORMAT_KEY_LEVEL, level);
        }
      }
    }

    int16_t rotation = format->rotation();
    if (rotation == 0 || rotation == 90 || rotation == 180 || rotation == 270) {
      if (__builtin_available(android 28, *)) {
        AMediaFormat_setInt32(ndk_format, AMEDIAFORMAT_KEY_ROTATION, rotation);
      }
    }

  } else if (format->stream_type() == MediaType::AUDIO) {
    uint32_t sample_rate = format->sample_rate();
    if (sample_rate > 0) {
      AMediaFormat_setInt32(ndk_format, AMEDIAFORMAT_KEY_SAMPLE_RATE,
                            static_cast<int32_t>(sample_rate));
      AVE_LOG(LS_VERBOSE) << "MediaMetaToAMediaFormat: sample_rate="
                          << sample_rate;
    }

    ChannelLayout layout = format->channel_layout();
    int channel_count = ChannelLayoutToChannelCount(layout);
    if (channel_count > 0) {
      AMediaFormat_setInt32(ndk_format, AMEDIAFORMAT_KEY_CHANNEL_COUNT,
                            channel_count);
      AVE_LOG(LS_VERBOSE) << "MediaMetaToAMediaFormat: channels="
                          << channel_count;
    }

    int64_t bitrate = format->bitrate();
    if (bitrate > 0) {
      AMediaFormat_setInt32(ndk_format, AMEDIAFORMAT_KEY_BIT_RATE,
                            static_cast<int32_t>(bitrate));
    }
  }

  // CSD (Codec Specific Data) - SPS/PPS for H.264, AudioSpecificConfig, etc.
  // For H.264, FFmpeg MP4 demuxer stores the raw avcC configuration record
  // (AVCC format), but Android MediaCodec expects Annex B format:
  //   csd-0 = 00 00 00 01 + SPS NALU
  //   csd-1 = 00 00 00 01 + PPS NALU
  // For HEVC, similarly convert hvcC to Annex B with all parameter sets.
  auto csd = format->private_data();
  if (csd && csd->size() > 0) {
    const auto* csd_data = static_cast<const uint8_t*>(csd->data());
    size_t csd_size = csd->size();

    if (mime == "video/avc" && IsAvccFormat(csd_data, csd_size)) {
      // H.264: Convert AVCC → Annex B csd-0 (SPS) + csd-1 (PPS)
      // Also extract NAL length size for access unit conversion.
      nal_length_size_ = (csd_data[4] & 0x03) + 1;
      AVE_LOG(LS_INFO) << "MediaMetaToAMediaFormat: AVCC nal_length_size="
                       << (int)nal_length_size_;
      std::vector<uint8_t> csd0_buf;
      std::vector<uint8_t> csd1_buf;
      if (ConvertAvccToAnnexBCsd(csd_data, csd_size, csd0_buf, csd1_buf)) {
        AMediaFormat_setBuffer(ndk_format, "csd-0", csd0_buf.data(),
                               csd0_buf.size());
        AMediaFormat_setBuffer(ndk_format, "csd-1", csd1_buf.data(),
                               csd1_buf.size());
        AVE_LOG(LS_INFO) << "MediaMetaToAMediaFormat: AVCC→AnnexB csd-0="
                         << csd0_buf.size()
                         << " bytes, csd-1=" << csd1_buf.size() << " bytes";
      } else {
        AVE_LOG(LS_WARNING)
            << "MediaMetaToAMediaFormat: AVCC parse failed, passing raw CSD";
        AMediaFormat_setBuffer(ndk_format, "csd-0", csd->data(), csd->size());
        nal_length_size_ = 0;  // disable access unit conversion
      }
    } else if (mime == "video/hevc" && IsHvccFormat(csd_data, csd_size)) {
      // HEVC: Convert HVCC → Annex B csd-0 (VPS+SPS+PPS)
      nal_length_size_ = (csd_data[21] & 0x03) + 1;
      AVE_LOG(LS_INFO) << "MediaMetaToAMediaFormat: HVCC nal_length_size="
                       << (int)nal_length_size_;
      std::vector<uint8_t> csd0_buf;
      if (ConvertHvccToAnnexBCsd(csd_data, csd_size, csd0_buf)) {
        AMediaFormat_setBuffer(ndk_format, "csd-0", csd0_buf.data(),
                               csd0_buf.size());
        AVE_LOG(LS_INFO) << "MediaMetaToAMediaFormat: HVCC→AnnexB csd-0="
                         << csd0_buf.size() << " bytes";
      } else {
        AVE_LOG(LS_WARNING)
            << "MediaMetaToAMediaFormat: HVCC parse failed, passing raw CSD";
        AMediaFormat_setBuffer(ndk_format, "csd-0", csd->data(), csd->size());
      }
    } else {
      // Audio (AudioSpecificConfig) or already Annex B — pass through as-is
      AMediaFormat_setBuffer(ndk_format, "csd-0", csd->data(), csd->size());
      AVE_LOG(LS_VERBOSE) << "MediaMetaToAMediaFormat: csd-0 passthrough, size="
                          << csd->size();
    }
  }

  AVE_LOG(LS_INFO) << "MediaMetaToAMediaFormat: result="
                   << AMediaFormat_toString(ndk_format);
  return ndk_format;
}

status_t AndroidNdkMediaCodec::Configure(
    const std::shared_ptr<CodecConfig>& config) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!android_media_codec_) {
    AVE_LOG(LS_ERROR) << "Configure: codec not created";
    return NO_INIT;
  }

  if (!config || !config->format) {
    AVE_LOG(LS_ERROR) << "Configure: invalid config or format";
    return BAD_VALUE;
  }

  mime_ = config->info.mime;
  media_type_ = config->info.media_type;
  AVE_LOG(LS_INFO) << "Configure: mime=" << mime_
                   << ", media_type=" << static_cast<int>(media_type_)
                   << ", is_encoder=" << is_encoder_;

  AMediaFormat* ndk_format = MediaMetaToAMediaFormat(config->format);
  if (!ndk_format) {
    AVE_LOG(LS_ERROR) << "Configure: failed to create AMediaFormat";
    return UNKNOWN_ERROR;
  }

  // For video decoders, check if the video_render provides an ANativeWindow
  // for hardware surface rendering.  Surface mode lets the codec decode
  // directly into the display pipeline, bypassing CPU pixel copies.
  ANativeWindow* surface = nullptr;
  surface_mode_ = false;
  if (!is_encoder_ && media_type_ == MediaType::VIDEO && config->video_render) {
    surface = config->video_render->android_native_window();
    if (surface) {
      surface_mode_ = true;
      ANativeWindow_acquire(surface);
      surface_ = surface;
      AVE_LOG(LS_INFO) << "Configure: surface mode, window=" << surface;
    }
  }
  if (!surface_mode_) {
    AVE_LOG(LS_INFO) << "Configure: buffer mode";
  }

  // Register NDK async callbacks BEFORE AMediaCodec_configure.
  // Per the NDK API contract, AMediaCodec_setAsyncNotifyCallback must be
  // called before configure (or start) to put the codec into async mode.
  if (__builtin_available(android 28, *)) {
    AMediaCodecOnAsyncNotifyCallback async_cb{};
    async_cb.onAsyncInputAvailable = OnAsyncInputAvailable;
    async_cb.onAsyncOutputAvailable = OnAsyncOutputAvailable;
    async_cb.onAsyncFormatChanged = OnAsyncFormatChanged;
    async_cb.onAsyncError = OnAsyncError;

    media_status_t cb_status = AMediaCodec_setAsyncNotifyCallback(
        android_media_codec_, async_cb, this);
    if (cb_status != AMEDIA_OK) {
      AVE_LOG(LS_ERROR) << "Configure: setAsyncNotifyCallback failed, status="
                        << cb_status;
      AMediaFormat_delete(ndk_format);
      if (surface_) {
        ANativeWindow_release(surface_);
        surface_ = nullptr;
      }
      return MapMediaStatus(cb_status);
    }
    AVE_LOG(LS_INFO) << "Configure: async callbacks registered";
  } else {
    AVE_LOG(LS_ERROR) << "Configure: async callbacks require API 28+";
    AMediaFormat_delete(ndk_format);
    return ERROR_UNSUPPORTED;
  }

  media_status_t status = AMediaCodec_configure(
      android_media_codec_, ndk_format,
      surface,  // ANativeWindow* for surface mode (nullptr = buffer mode)
      nullptr,  // crypto
      is_encoder_ ? AMEDIACODEC_CONFIGURE_FLAG_ENCODE : 0);

  AMediaFormat_delete(ndk_format);

  if (status != AMEDIA_OK) {
    AVE_LOG(LS_ERROR) << "Configure: AMediaCodec_configure failed, status="
                      << status;
    return MapMediaStatus(status);
  }

  // Initialize output format from input config as fallback
  if (media_type_ == MediaType::AUDIO) {
    output_sample_rate_ = static_cast<int32_t>(config->format->sample_rate());
    output_channel_count_ =
        ChannelLayoutToChannelCount(config->format->channel_layout());
    audio_output_pts_valid_ = false;
    audio_output_pts_us_ = 0;
    audio_output_buffer_duration_us_ = 0;
  } else if (media_type_ == MediaType::VIDEO) {
    output_width_ = config->format->width();
    output_height_ = config->format->height();
  }

  AVE_LOG(LS_INFO) << "Configure: success";
  return OK;
}

status_t AndroidNdkMediaCodec::SetCallback(CodecCallback* callback) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!android_media_codec_) {
    AVE_LOG(LS_ERROR) << "SetCallback: codec not created";
    return NO_INIT;
  }

  callback_ = callback;
  AVE_LOG(LS_INFO) << "SetCallback: callback stored, codec="
                   << android_media_codec_;
  return OK;
}

status_t AndroidNdkMediaCodec::Start() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!android_media_codec_) {
    AVE_LOG(LS_ERROR) << "Start: codec not created";
    return NO_INIT;
  }

  AVE_LOG(LS_INFO) << "Start: starting AMediaCodec";
  media_status_t status = AMediaCodec_start(android_media_codec_);
  if (status != AMEDIA_OK) {
    AVE_LOG(LS_ERROR) << "Start: AMediaCodec_start failed, status=" << status;
    return MapMediaStatus(status);
  }

  AVE_LOG(LS_INFO) << "Start: success";
  return OK;
}

status_t AndroidNdkMediaCodec::Stop() {
  if (!android_media_codec_) {
    AVE_LOG(LS_WARNING) << "Stop: codec not created";
    return NO_INIT;
  }

  AVE_LOG(LS_INFO) << "Stop: stopping AMediaCodec";
  media_status_t status = AMediaCodec_stop(android_media_codec_);
  if (status != AMEDIA_OK) {
    AVE_LOG(LS_ERROR) << "Stop: AMediaCodec_stop failed, status=" << status;
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    input_buffers_.clear();
    output_buffers_.clear();
    output_buffer_infos_.clear();
    audio_output_pts_valid_ = false;
    audio_output_pts_us_ = 0;
    audio_output_buffer_duration_us_ = 0;

    if (surface_) {
      ANativeWindow_release(surface_);
      surface_ = nullptr;
    }
    surface_mode_ = false;
  }

  return MapMediaStatus(status);
}

status_t AndroidNdkMediaCodec::Flush() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!android_media_codec_) {
    return NO_INIT;
  }

  AVE_LOG(LS_INFO) << "Flush: flushing AMediaCodec";
  media_status_t status = AMediaCodec_flush(android_media_codec_);

  input_buffers_.clear();
  output_buffers_.clear();
  output_buffer_infos_.clear();
  audio_output_pts_valid_ = false;
  audio_output_pts_us_ = 0;
  audio_output_buffer_duration_us_ = 0;

  return MapMediaStatus(status);
}

status_t AndroidNdkMediaCodec::Reset() {
  AVE_LOG(LS_INFO) << "Reset";
  Stop();
  return OK;
}

status_t AndroidNdkMediaCodec::Release() {
  AVE_LOG(LS_INFO) << "Release";
  Stop();
  return OK;
}

status_t AndroidNdkMediaCodec::GetInputBuffer(
    size_t index,
    std::shared_ptr<CodecBuffer>& buffer) {
  // Do NOT hold mutex_ during AMediaCodec_getInputBuffer — same deadlock
  // avoidance as QueueInputBuffer / ReleaseOutputBuffer.
  AMediaCodec* codec = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!android_media_codec_) {
      return NO_INIT;
    }
    codec = android_media_codec_;
  }

  size_t size = 0;
  auto* addr = AMediaCodec_getInputBuffer(codec, index, &size);
  if (!addr) {
    AVE_LOG(LS_ERROR) << "GetInputBuffer: null buffer for index=" << index;
    return ERROR_RETRY;
  }

  AVE_LOG(LS_VERBOSE) << "GetInputBuffer: index=" << index
                      << ", capacity=" << size;

  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (input_buffers_.find(index) == input_buffers_.end()) {
      auto codec_buffer = std::make_shared<CodecBuffer>(addr, size);
      input_buffers_[index] = codec_buffer;
    } else {
      auto& codec_buffer = input_buffers_[index];
      auto new_buffer = std::make_shared<media::Buffer>(addr, size);
      codec_buffer->ResetBuffer(new_buffer);
    }
    buffer = input_buffers_[index];
  }
  return OK;
}

status_t AndroidNdkMediaCodec::GetOutputBuffer(
    size_t index,
    std::shared_ptr<CodecBuffer>& buffer) {
  // Snapshot state under lock, then do the NDK call without holding mutex_
  // to avoid deadlock with async callbacks (same reason as QueueInputBuffer).
  AMediaCodec* codec = nullptr;
  bool is_surface = false;
  OutputBufferInfo buf_info{};
  bool has_buf_info = false;
  int32_t out_width = 0, out_height = 0, out_stride = 0;
  int32_t out_sample_rate = 0, out_channel_count = 0, out_color_format = 0;
  MediaType mtype = MediaType::UNKNOWN;

  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!android_media_codec_) {
      return NO_INIT;
    }
    codec = android_media_codec_;
    is_surface = surface_mode_;
    mtype = media_type_;
    out_width = output_width_;
    out_height = output_height_;
    out_stride = output_stride_;
    out_sample_rate = output_sample_rate_;
    out_channel_count = output_channel_count_;
    out_color_format = output_color_format_;
    auto info_it = output_buffer_infos_.find(index);
    if (info_it != output_buffer_infos_.end()) {
      buf_info = info_it->second;
      has_buf_info = true;
    }
  }

  // In surface mode, the codec renders directly to the ANativeWindow.
  // Output buffer data is not accessible; create a metadata-only buffer.
  if (is_surface) {
    auto codec_buffer = std::make_shared<CodecBuffer>(0);  // empty buffer

    auto meta =
        MediaMeta::CreatePtr(MediaType::VIDEO, MediaMeta::FormatType::kSample);
    meta->SetWidth(out_width);
    meta->SetHeight(out_height);
    meta->SetStride(out_stride > 0 ? out_stride : out_width);
    if (has_buf_info) {
      meta->SetPts(base::Timestamp::Micros(buf_info.presentation_time_us));
    }
    codec_buffer->format() = meta;

    std::lock_guard<std::mutex> lock(mutex_);
    output_buffers_[index] = codec_buffer;
    buffer = codec_buffer;
    return OK;
  }

  // Buffer mode: get the raw buffer pointer without holding mutex_
  size_t total_size = 0;
  auto* addr = AMediaCodec_getOutputBuffer(codec, index, &total_size);
  if (!addr) {
    AVE_LOG(LS_ERROR) << "GetOutputBuffer: null buffer for index=" << index;
    return UNKNOWN_ERROR;
  }

  // Use the actual data range from bufferInfo
  int32_t data_offset = 0;
  int32_t data_size = static_cast<int32_t>(total_size);
  if (has_buf_info) {
    data_offset = buf_info.offset;
    data_size = buf_info.size;
  } else {
    AVE_LOG(LS_WARNING) << "GetOutputBuffer: no bufferInfo for index=" << index
                        << ", using raw capacity=" << total_size;
  }

  AVE_LOG(LS_INFO) << "GetOutputBuffer: index=" << index
                   << ", total_size=" << total_size
                   << ", data_offset=" << data_offset
                   << ", data_size=" << data_size
                   << ", has_buf_info=" << has_buf_info;

  auto codec_buffer =
      std::make_shared<CodecBuffer>(addr + data_offset, data_size);

  // Build rich metadata matching FFmpegCodec's output format
  if (mtype == MediaType::AUDIO) {
    auto meta =
        MediaMeta::CreatePtr(MediaType::AUDIO, MediaMeta::FormatType::kSample);
    if (out_sample_rate > 0) {
      meta->SetSampleRate(static_cast<uint32_t>(out_sample_rate));
    }
    if (out_channel_count > 0) {
      meta->SetChannelLayout(GuessChannelLayout(out_channel_count));
    }
    // Android MediaCodec always outputs 16-bit PCM
    meta->SetBitsPerSample(16);
    meta->SetCodec(CodecId::AVE_CODEC_ID_PCM_S16LE);
    // Compute samples per channel: data_size / (channels * 2 bytes)
    int32_t samples_per_ch = 0;
    if (out_channel_count > 0) {
      samples_per_ch = data_size / (out_channel_count * 2);
      meta->SetSamplesPerChannel(samples_per_ch);
    }

    int64_t audio_pts_us = has_buf_info ? buf_info.presentation_time_us : -1;
    if (out_sample_rate > 0 && samples_per_ch > 0) {
      const int64_t buffer_duration_us =
          static_cast<int64_t>(samples_per_ch) * 1000000LL / out_sample_rate;
      if (audio_output_pts_valid_) {
        const int64_t expected_pts_us =
            audio_output_pts_us_ + audio_output_buffer_duration_us_;
        const int64_t tolerance_us =
            std::max<int64_t>(100000, audio_output_buffer_duration_us_ * 2);
        if (audio_pts_us <= 0 ||
            std::llabs(audio_pts_us - expected_pts_us) > tolerance_us) {
          AVE_LOG(LS_VERBOSE)
              << "GetOutputBuffer[AUDIO]: replacing codec pts=" << audio_pts_us
              << " with synthesized pts=" << expected_pts_us
              << " expected=" << expected_pts_us
              << " tolerance=" << tolerance_us;
          audio_pts_us = expected_pts_us;
        }
      }
      if (audio_pts_us >= 0) {
        audio_output_pts_valid_ = true;
        audio_output_pts_us_ = audio_pts_us;
        audio_output_buffer_duration_us_ = buffer_duration_us;
      }
    }
    if (audio_pts_us >= 0) {
      meta->SetPts(base::Timestamp::Micros(audio_pts_us));
    }
    codec_buffer->format() = meta;
  } else if (mtype == MediaType::VIDEO) {
    auto meta =
        MediaMeta::CreatePtr(MediaType::VIDEO, MediaMeta::FormatType::kSample);
    meta->SetWidth(out_width);
    meta->SetHeight(out_height);
    meta->SetStride(out_stride > 0 ? out_stride : out_width);
    // MediaCodec buffer mode typically outputs NV12
    // (COLOR_FormatYUV420SemiPlanar) Map to our pixel format. 21 =
    // COLOR_FormatYUV420SemiPlanar (NV12)
    if (out_color_format == 21) {
      meta->SetPixelFormat(PixelFormat::AVE_PIX_FMT_NV12);
    } else if (out_color_format == 19) {
      // 19 = COLOR_FormatYUV420Planar (I420)
      meta->SetPixelFormat(PixelFormat::AVE_PIX_FMT_YUV420P);
    } else {
      // Default assumption: NV12 (most common on modern Android devices)
      meta->SetPixelFormat(PixelFormat::AVE_PIX_FMT_NV12);
    }
    if (has_buf_info) {
      meta->SetPts(base::Timestamp::Micros(buf_info.presentation_time_us));
    }
    codec_buffer->format() = meta;
  } else {
    // Fallback: just PTS
    if (has_buf_info) {
      auto meta = MediaMeta::CreatePtr(MediaType::UNKNOWN,
                                       MediaMeta::FormatType::kSample);
      meta->SetPts(base::Timestamp::Micros(buf_info.presentation_time_us));
      codec_buffer->format() = meta;
    }
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    output_buffers_[index] = codec_buffer;
  }
  buffer = codec_buffer;
  return OK;
}

ssize_t AndroidNdkMediaCodec::DequeueInputBuffer(int64_t timeoutUs) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!android_media_codec_) {
    return -1;
  }

  auto index = AMediaCodec_dequeueInputBuffer(android_media_codec_, timeoutUs);
  AVE_LOG(LS_VERBOSE) << "DequeueInputBuffer: index=" << index;
  return static_cast<ssize_t>(index);
}

status_t AndroidNdkMediaCodec::QueueInputBuffer(size_t index) {
  // Do NOT hold the mutex_ here — AMediaCodec_queueInputBuffer may trigger
  // async callbacks that also try to lock mutex_.
  std::shared_ptr<CodecBuffer> input_buffer;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!android_media_codec_) {
      return NO_INIT;
    }
    auto it = input_buffers_.find(index);
    if (it == input_buffers_.end()) {
      AVE_LOG(LS_ERROR) << "QueueInputBuffer: no buffer for index=" << index;
      return INVALID_OPERATION;
    }
    input_buffer = it->second;
  }

  auto offset = input_buffer->offset();
  auto size = input_buffer->size();
  int64_t pts = 0;
  if (input_buffer->format()) {
    pts = input_buffer->format()->pts().us_or(0);
  }

  uint32_t flags = 0;
  if (input_buffer->format() && input_buffer->format()->eos()) {
    flags = AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM;
  }

  // Convert AVCC (4-byte NAL length prefix) → Annex B (start code) in-place.
  // Both use exactly 4 bytes, so the buffer size doesn't change.
  if (nal_length_size_ == 4 && size >= 4 && input_buffer->data()) {
    auto* p = static_cast<uint8_t*>(input_buffer->data());
    size_t pos = 0;
    while (pos + 4 <= size) {
      uint32_t nal_size = (static_cast<uint32_t>(p[pos]) << 24) |
                          (static_cast<uint32_t>(p[pos + 1]) << 16) |
                          (static_cast<uint32_t>(p[pos + 2]) << 8) |
                          static_cast<uint32_t>(p[pos + 3]);
      // Replace length with Annex B start code
      p[pos] = 0x00;
      p[pos + 1] = 0x00;
      p[pos + 2] = 0x00;
      p[pos + 3] = 0x01;
      pos += 4 + nal_size;
    }
    // Log first few bytes after conversion to confirm Annex B start code
    AVE_LOG(LS_INFO) << "QueueInputBuffer[VIDEO]: index=" << index
                     << ", size=" << size << ", pts=" << pts << ", first4=["
                     << static_cast<int>(p[0]) << " " << static_cast<int>(p[1])
                     << " " << static_cast<int>(p[2]) << " "
                     << static_cast<int>(p[3]) << "]";
  }

  AVE_LOG(LS_VERBOSE) << "QueueInputBuffer: index=" << index
                      << ", offset=" << offset << ", size=" << size
                      << ", pts=" << pts << ", flags=" << flags;

#if defined(__ANDROID__)
  DumpDebugVideoInput(media_type_, mime_, input_buffer->data(), size, flags);
#endif

  media_status_t ret = AMediaCodec_queueInputBuffer(android_media_codec_, index,
                                                    static_cast<off_t>(offset),
                                                    size, pts, flags);

  if (ret != AMEDIA_OK) {
    AVE_LOG(LS_ERROR) << "QueueInputBuffer: failed, status=" << ret
                      << ", index=" << index << ", size=" << size;
  }

  return MapMediaStatus(ret);
}

ssize_t AndroidNdkMediaCodec::DequeueOutputBuffer(int64_t timeout_us) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!android_media_codec_) {
    return -1;
  }

  AMediaCodecBufferInfo info{};
  auto index =
      AMediaCodec_dequeueOutputBuffer(android_media_codec_, &info, timeout_us);
  if (index >= 0) {
    output_buffer_infos_[static_cast<size_t>(index)] = {
        info.offset, info.size, info.presentationTimeUs,
        static_cast<uint32_t>(info.flags)};
    AVE_LOG(LS_VERBOSE) << "DequeueOutputBuffer: index=" << index
                        << ", size=" << info.size
                        << ", pts=" << info.presentationTimeUs;
  }

  return static_cast<ssize_t>(index);
}

status_t AndroidNdkMediaCodec::ReleaseOutputBuffer(size_t index, bool render) {
  // Do NOT hold mutex_ during AMediaCodec_releaseOutputBuffer — it may
  // trigger async callbacks that also try to lock mutex_, causing deadlock
  // (same pattern as QueueInputBuffer).
  AMediaCodec* codec = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!android_media_codec_) {
      return NO_INIT;
    }
    codec = android_media_codec_;
    // Erase map entries BEFORE the NDK call. The NDK call may immediately
    // recycle the buffer and trigger OnAsyncOutputAvailable for the same
    // index, which stores a new entry.  If we erased AFTER the NDK call,
    // we would delete the freshly-stored new entry — causing the next
    // GetOutputBuffer to fall back to raw buffer capacity (wrong size).
    output_buffers_.erase(index);
    output_buffer_infos_.erase(index);
  }

  AVE_LOG(LS_INFO) << "ReleaseOutputBuffer: index=" << index
                   << ", render=" << render;

  media_status_t ret = AMediaCodec_releaseOutputBuffer(codec, index, render);

  if (ret != AMEDIA_OK) {
    AVE_LOG(LS_ERROR) << "ReleaseOutputBuffer: failed, status=" << ret;
  }

  return MapMediaStatus(ret);
}

void AndroidNdkMediaCodec::NotifyInputBufferAvailable(size_t index) {
  // Do NOT lock mutex_ here — callback_ methods may post messages that
  // eventually call back into codec methods that acquire the lock.
  CodecCallback* cb = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    cb = callback_;
  }
  if (cb) {
    cb->OnInputBufferAvailable(index);
  }
}

void AndroidNdkMediaCodec::NotifyOutputBufferAvailable(
    size_t index,
    int32_t offset,
    int32_t size,
    int64_t presentation_time_us,
    uint32_t flags) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    output_buffer_infos_[index] = {offset, size, presentation_time_us, flags};
  }
  CodecCallback* cb = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    cb = callback_;
  }
  if (cb) {
    cb->OnOutputBufferAvailable(index);
  }
}

void AndroidNdkMediaCodec::NotifyOutputFormatChanged(AMediaFormat* format) {
  auto meta = MediaMeta::CreatePtr();
  if (format) {
    // Extract key fields from the new format
    int32_t width = 0;
    int32_t height = 0;
    int32_t sample_rate = 0;
    int32_t channel_count = 0;
    int32_t color_format = 0;
    int32_t stride = 0;

    if (AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &width) &&
        AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &height)) {
      meta->SetWidth(width);
      meta->SetHeight(height);
      output_width_ = width;
      output_height_ = height;
      AVE_LOG(LS_INFO) << "OutputFormatChanged: video " << width << "x"
                       << height;
    }
    if (AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_SAMPLE_RATE,
                              &sample_rate)) {
      meta->SetSampleRate(static_cast<uint32_t>(sample_rate));
      output_sample_rate_ = sample_rate;
      AVE_LOG(LS_INFO) << "OutputFormatChanged: sample_rate=" << sample_rate;
    }
    if (AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_CHANNEL_COUNT,
                              &channel_count)) {
      meta->SetChannelLayout(GuessChannelLayout(channel_count));
      output_channel_count_ = channel_count;
      AVE_LOG(LS_INFO) << "OutputFormatChanged: channels=" << channel_count;
    }
    if (AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT,
                              &color_format)) {
      output_color_format_ = color_format;
      AVE_LOG(LS_INFO) << "OutputFormatChanged: color_format=" << color_format;
    }
    if (AMediaFormat_getInt32(format, "stride", &stride)) {
      output_stride_ = stride;
      AVE_LOG(LS_INFO) << "OutputFormatChanged: stride=" << stride;
    }
  }

  CodecCallback* cb = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    cb = callback_;
  }
  if (cb) {
    cb->OnOutputFormatChanged(meta);
  }
}

void AndroidNdkMediaCodec::NotifyError(status_t error) {
  CodecCallback* cb = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    cb = callback_;
  }
  if (cb) {
    cb->OnError(error);
  }
}

void AndroidNdkMediaCodec::NotifyFrameRendered(
    std::shared_ptr<Message> message) {
  CodecCallback* cb = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    cb = callback_;
  }
  if (cb) {
    cb->OnFrameRendered(message);
  }
}

}  // namespace media
}  // namespace ave
