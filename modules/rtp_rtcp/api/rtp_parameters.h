/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AVE_MODULES_RTP_RTCP_INCLUDE_RTP_PARAMETERS_H_
#define AVE_MODULES_RTP_RTCP_INCLUDE_RTP_PARAMETERS_H_

#include <stdint.h>

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "media/foundation/media_utils.h"
#include "media/modules/rtp_rtcp/api/priority.h"
#include "media/modules/rtp_rtcp/api/rtp_transceiver_direction.h"

namespace ave {
namespace media {
namespace rtp_rtcp {

using ::ave::media::get_media_type_string;
using ::ave::media::MediaType;

using CodecParameterMap = std::map<std::string, std::string>;

// These structures are intended to mirror those defined by:
// http://draft.ortc.org/#rtcrtpdictionaries*

enum class FecMechanism {
  RED,
  RED_AND_ULPFEC,
  FLEXFEC,
};

// Used in RtcpFeedback struct.
enum class RtcpFeedbackType {
  CCM,
  LNTF,  // "goog-lntf"
  NACK,
  REMB,  // "goog-remb"
  TRANSPORT_CC,
};

// Used in RtcpFeedback struct when type is NACK or CCM.
enum class RtcpFeedbackMessageType {
  // Equivalent to {type: "nack", parameter: undefined} in ORTC.
  GENERIC_NACK,
  PLI,  // Usable with NACK.
  FIR,  // Usable with CCM.
};

enum class DtxStatus {
  DISABLED,
  ENABLED,
};

// Based on the spec in
// https://w3c.github.io/webrtc-pc/#idl-def-rtcdegradationpreference.
enum class DegradationPreference {
  // Don't take any actions based on over-utilization signals.
  DISABLED,
  // On over-use, request lower resolution, possibly causing down-scaling.
  MAINTAIN_FRAMERATE,
  // On over-use, request lower frame rate, possibly causing frame drops.
  MAINTAIN_RESOLUTION,
  // Try to strike a "pleasing" balance between frame rate or resolution.
  BALANCED,
};

const char* DegradationPreferenceToString(
    DegradationPreference degradation_preference);

extern const double kDefaultBitratePriority;

struct RtcpFeedback {
  RtcpFeedbackType type = RtcpFeedbackType::CCM;

  // Equivalent to ORTC "parameter" field with slight differences:
  // 1. It's an enum instead of a string.
  // 2. Generic NACK feedback is represented by a GENERIC_NACK message type,
  //    rather than an unset "parameter" value.
  std::optional<RtcpFeedbackMessageType> message_type;

  // Constructors for convenience.
  RtcpFeedback();
  explicit RtcpFeedback(RtcpFeedbackType type);
  RtcpFeedback(RtcpFeedbackType type, RtcpFeedbackMessageType message_type);
  RtcpFeedback(const RtcpFeedback&);
  ~RtcpFeedback();

  bool operator==(const RtcpFeedback& o) const {
    return type == o.type && message_type == o.message_type;
  }
  bool operator!=(const RtcpFeedback& o) const { return !(*this == o); }
};

struct RtpCodec {
  RtpCodec();
  RtpCodec(const RtpCodec&);
  virtual ~RtpCodec();

  // Build MIME "type/subtype" string from `name` and `kind`.
  std::string mime_type() const {
    return std::string(get_media_type_string(kind)) + "/" + name;
  }

  // Used to identify the codec. Equivalent to MIME subtype.
  std::string name;

  // The media type of this codec. Equivalent to MIME top-level type.
  MediaType kind = MediaType::AUDIO;

  // If unset, the implementation default is used.
  std::optional<int32_t> clock_rate;

  // The number of audio channels used. Unset for video codecs. If unset for
  // audio, the implementation default is used.
  std::optional<int32_t> num_channels;

  // Feedback mechanisms to be used for this codec.
  std::vector<RtcpFeedback> rtcp_feedback;

  // Codec-specific parameters that must be signaled to the remote party.
  //
  // Corresponds to "a=fmtp" parameters in SDP.
  std::map<std::string, std::string> parameters;

  bool operator==(const RtpCodec& o) const {
    return name == o.name && kind == o.kind && clock_rate == o.clock_rate &&
           num_channels == o.num_channels && rtcp_feedback == o.rtcp_feedback &&
           parameters == o.parameters;
  }
  bool operator!=(const RtpCodec& o) const { return !(*this == o); }
  bool IsResiliencyCodec() const;
  bool IsMediaCodec() const;
};

// RtpCodecCapability is to RtpCodecParameters as RtpCapabilities is to
// RtpParameters. This represents the static capabilities of an endpoint's
// implementation of a codec.
struct RtpCodecCapability : public RtpCodec {
  RtpCodecCapability();
  virtual ~RtpCodecCapability();

  // Default payload type for this codec. Mainly needed for codecs that have
  // statically assigned payload types.
  std::optional<int32_t> preferred_payload_type;

  bool operator==(const RtpCodecCapability& o) const {
    return RtpCodec::operator==(o) &&
           preferred_payload_type == o.preferred_payload_type;
  }
  bool operator!=(const RtpCodecCapability& o) const { return !(*this == o); }
};

// Used in RtpCapabilities and RtpTransceiverInterface's header extensions query
// and setup methods; represents the capabilities/preferences of an
// implementation for a header extension.
struct RtpHeaderExtensionCapability {
  // URI of this extension, as defined in RFC8285.
  std::string uri;

  // Preferred value of ID that goes in the packet.
  std::optional<int32_t> preferred_id;

  // If true, it's preferred that the value in the header is encrypted.
  bool preferred_encrypt = false;

  // The direction of the extension.
  RtpTransceiverDirection direction = RtpTransceiverDirection::kSendRecv;

  // Constructors for convenience.
  RtpHeaderExtensionCapability();
  explicit RtpHeaderExtensionCapability(std::string_view uri);
  RtpHeaderExtensionCapability(std::string_view uri, int32_t preferred_id);
  RtpHeaderExtensionCapability(std::string_view uri,
                               int32_t preferred_id,
                               RtpTransceiverDirection direction);
  RtpHeaderExtensionCapability(std::string_view uri,
                               int32_t preferred_id,
                               bool preferred_encrypt,
                               RtpTransceiverDirection direction);
  ~RtpHeaderExtensionCapability();

  bool operator==(const RtpHeaderExtensionCapability& o) const {
    return uri == o.uri && preferred_id == o.preferred_id &&
           preferred_encrypt == o.preferred_encrypt && direction == o.direction;
  }
  bool operator!=(const RtpHeaderExtensionCapability& o) const {
    return !(*this == o);
  }
};

// RTP header extension, see RFC8285.
struct RtpExtension {
  enum Filter {
    // Encrypted extensions will be ignored and only non-encrypted extensions
    // will be considered.
    kDiscardEncryptedExtension,
    // Encrypted extensions will be preferred but will fall back to
    // non-encrypted extensions if necessary.
    kPreferEncryptedExtension,
    // Encrypted extensions will be required, so any non-encrypted extensions
    // will be discarded.
    kRequireEncryptedExtension,
  };

  RtpExtension();
  RtpExtension(std::string_view uri, int32_t id);
  RtpExtension(std::string_view uri, int32_t id, bool encrypt);
  ~RtpExtension();

  std::string ToString() const;
  bool operator==(const RtpExtension& rhs) const {
    return uri == rhs.uri && id == rhs.id && encrypt == rhs.encrypt;
  }
  static bool IsSupportedForAudio(std::string_view uri);
  static bool IsSupportedForVideo(std::string_view uri);
  // Return "true" if the given RTP header extension URI may be encrypted.
  static bool IsEncryptionSupported(std::string_view uri);

  // Returns the header extension with the given URI or nullptr if not found.
  static const RtpExtension* FindHeaderExtensionByUri(
      const std::vector<RtpExtension>& extensions,
      std::string_view uri,
      Filter filter);

  // Returns the header extension with the given URI and encrypt parameter,
  // if found, otherwise nullptr.
  static const RtpExtension* FindHeaderExtensionByUriAndEncryption(
      const std::vector<RtpExtension>& extensions,
      std::string_view uri,
      bool encrypt);

  // Returns a list of extensions where any extension URI is unique.
  static std::vector<RtpExtension> DeduplicateHeaderExtensions(
      const std::vector<RtpExtension>& extensions,
      Filter filter);

  // Encryption of Header Extensions, see RFC 6904 for details:
  // https://tools.ietf.org/html/rfc6904
  static constexpr char kEncryptHeaderExtensionsUri[] =
      "urn:ietf:params:rtp-hdrext:encrypt";

  // Header extension for audio levels, as defined in:
  // https://tools.ietf.org/html/rfc6464
  static constexpr char kAudioLevelUri[] =
      "urn:ietf:params:rtp-hdrext:ssrc-audio-level";

  // Header extension for RTP timestamp offset, see RFC 5450 for details:
  // http://tools.ietf.org/html/rfc5450
  static constexpr char kTimestampOffsetUri[] =
      "urn:ietf:params:rtp-hdrext:toffset";

  // Header extension for absolute send time, see url for details:
  // http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
  static constexpr char kAbsSendTimeUri[] =
      "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time";

  // Header extension for absolute capture time, see url for details:
  // http://www.webrtc.org/experiments/rtp-hdrext/abs-capture-time
  static constexpr char kAbsoluteCaptureTimeUri[] =
      "http://www.webrtc.org/experiments/rtp-hdrext/abs-capture-time";

  // Header extension for coordination of video orientation, see url for
  // details:
  // http://www.etsi.org/deliver/etsi_ts/126100_126199/126114/12.07.00_60/ts_126114v120700p.pdf
  static constexpr char kVideoRotationUri[] = "urn:3gpp:video-orientation";

  // Header extension for video content type. E.g. default or screenshare.
  static constexpr char kVideoContentTypeUri[] =
      "http://www.webrtc.org/experiments/rtp-hdrext/video-content-type";

  // Header extension for video timing.
  static constexpr char kVideoTimingUri[] =
      "http://www.webrtc.org/experiments/rtp-hdrext/video-timing";

  // Experimental codec agnostic frame descriptor.
  static constexpr char kGenericFrameDescriptorUri00[] =
      "http://www.webrtc.org/experiments/rtp-hdrext/"
      "generic-frame-descriptor-00";
  static constexpr char kDependencyDescriptorUri[] =
      "https://aomediacodec.github.io/av1-rtp-spec/"
      "#dependency-descriptor-rtp-header-extension";

  // Experimental extension for signalling target bitrate per layer.
  static constexpr char kVideoLayersAllocationUri[] =
      "http://www.webrtc.org/experiments/rtp-hdrext/video-layers-allocation00";

  // Header extension for transport sequence number, see url for details:
  // http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions
  static constexpr char kTransportSequenceNumberUri[] =
      "http://www.ietf.org/id/"
      "draft-holmer-rmcat-transport-wide-cc-extensions-01";
  static constexpr char kTransportSequenceNumberV2Uri[] =
      "http://www.webrtc.org/experiments/rtp-hdrext/transport-wide-cc-02";

  // This extension allows applications to adaptively limit the playout delay
  // on frames as per the current needs.
  static constexpr char kPlayoutDelayUri[] =
      "http://www.webrtc.org/experiments/rtp-hdrext/playout-delay";

  // Header extension for color space information.
  static constexpr char kColorSpaceUri[] =
      "http://www.webrtc.org/experiments/rtp-hdrext/color-space";

  // Header extension for identifying media section within a transport.
  // https://tools.ietf.org/html/draft-ietf-mmusic-sdp-bundle-negotiation-49#section-15
  static constexpr char kMidUri[] = "urn:ietf:params:rtp-hdrext:sdes:mid";

  // Header extension for RIDs and Repaired RIDs
  // https://tools.ietf.org/html/draft-ietf-avtext-rid-09
  // https://tools.ietf.org/html/draft-ietf-mmusic-rid-15
  static constexpr char kRidUri[] =
      "urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id";
  static constexpr char kRepairedRidUri[] =
      "urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id";

  // Header extension to propagate webrtc::VideoFrame id field
  static constexpr char kVideoFrameTrackingIdUri[] =
      "http://www.webrtc.org/experiments/rtp-hdrext/video-frame-tracking-id";

  // Header extension for Mixer-to-Client audio levels per CSRC as defined in
  // https://tools.ietf.org/html/rfc6465
  static constexpr char kCsrcAudioLevelsUri[] =
      "urn:ietf:params:rtp-hdrext:csrc-audio-level";

  // Header extension for automatic corruption detection.
  static constexpr char kCorruptionDetectionUri[] =
      "http://www.webrtc.org/experiments/rtp-hdrext/corruption-detection";

  // Inclusive min and max IDs for two-byte header extensions and one-byte
  // header extensions, per RFC8285 Section 4.2-4.3.
  static constexpr int32_t kMinId = 1;
  static constexpr int32_t kMaxId = 255;
  static constexpr int32_t kMaxValueSize = 255;
  static constexpr int32_t kOneByteHeaderExtensionMaxId = 14;
  static constexpr int32_t kOneByteHeaderExtensionMaxValueSize = 16;

  std::string uri;
  int32_t id = 0;
  bool encrypt = false;
};

struct RtpFecParameters {
  // If unset, a value is chosen by the implementation.
  std::optional<uint32_t> ssrc;

  FecMechanism mechanism = FecMechanism::RED;

  // Constructors for convenience.
  RtpFecParameters();
  explicit RtpFecParameters(FecMechanism mechanism);
  RtpFecParameters(FecMechanism mechanism, uint32_t ssrc);
  RtpFecParameters(const RtpFecParameters&);
  ~RtpFecParameters();

  bool operator==(const RtpFecParameters& o) const {
    return ssrc == o.ssrc && mechanism == o.mechanism;
  }
  bool operator!=(const RtpFecParameters& o) const { return !(*this == o); }
};

struct RtpRtxParameters {
  // If unset, a value is chosen by the implementation.
  std::optional<uint32_t> ssrc;

  // Constructors for convenience.
  RtpRtxParameters();
  explicit RtpRtxParameters(uint32_t ssrc);
  RtpRtxParameters(const RtpRtxParameters&);
  ~RtpRtxParameters();

  bool operator==(const RtpRtxParameters& o) const { return ssrc == o.ssrc; }
  bool operator!=(const RtpRtxParameters& o) const { return !(*this == o); }
};

struct RtpEncodingParameters {
  RtpEncodingParameters();
  RtpEncodingParameters(const RtpEncodingParameters&);
  ~RtpEncodingParameters();

  // If unset, a value is chosen by the implementation.
  std::optional<uint32_t> ssrc;

  // The relative bitrate priority of this encoding.
  double bitrate_priority = kDefaultBitratePriority;

  // The relative DiffServ Code Point priority for this encoding.
  Priority network_priority = Priority::kLow;

  // If set, this represents the Transport Independent Application Specific
  // maximum bandwidth defined in RFC3890.
  std::optional<int32_t> max_bitrate_bps;

  // Specifies the minimum bitrate in bps for video.
  std::optional<int32_t> min_bitrate_bps;

  // Specifies the maximum framerate in fps for video.
  std::optional<double> max_framerate;

  // Specifies the number of temporal layers for video.
  std::optional<int32_t> num_temporal_layers;

  // For video, scale the resolution down by this factor.
  std::optional<double> scale_resolution_down_by;

  // https://w3c.github.io/webrtc-svc/#rtcrtpencodingparameters
  std::optional<std::string> scalability_mode;

  // For an RtpSender, set to true to cause this encoding to be encoded and
  // sent, and false for it not to be encoded and sent.
  bool active = true;

  // Value to use for RID RTP header extension.
  std::string rid;
  bool request_key_frame = false;

  // Allow dynamic frame length changes for audio:
  bool adaptive_ptime = false;

  // Allow changing the used codec for this encoding.
  std::optional<RtpCodec> codec;

  bool operator==(const RtpEncodingParameters& o) const {
    return ssrc == o.ssrc && bitrate_priority == o.bitrate_priority &&
           network_priority == o.network_priority &&
           max_bitrate_bps == o.max_bitrate_bps &&
           min_bitrate_bps == o.min_bitrate_bps &&
           max_framerate == o.max_framerate &&
           num_temporal_layers == o.num_temporal_layers &&
           scale_resolution_down_by == o.scale_resolution_down_by &&
           active == o.active && rid == o.rid &&
           adaptive_ptime == o.adaptive_ptime && codec == o.codec;
  }
  bool operator!=(const RtpEncodingParameters& o) const {
    return !(*this == o);
  }
};

struct RtpCodecParameters : public RtpCodec {
  RtpCodecParameters();
  RtpCodecParameters(const RtpCodecParameters&);
  virtual ~RtpCodecParameters();

  // Payload type used to identify this codec in RTP packets.
  int32_t payload_type = 0;

  bool operator==(const RtpCodecParameters& o) const {
    return RtpCodec::operator==(o) && payload_type == o.payload_type;
  }
  bool operator!=(const RtpCodecParameters& o) const { return !(*this == o); }
};

// RtpCapabilities is used to represent the static capabilities of an endpoint.
struct RtpCapabilities {
  RtpCapabilities();
  ~RtpCapabilities();

  // Supported codecs.
  std::vector<RtpCodecCapability> codecs;

  // Supported RTP header extensions.
  std::vector<RtpHeaderExtensionCapability> header_extensions;

  // Supported Forward Error Correction (FEC) mechanisms.
  std::vector<FecMechanism> fec;

  bool operator==(const RtpCapabilities& o) const {
    return codecs == o.codecs && header_extensions == o.header_extensions &&
           fec == o.fec;
  }
  bool operator!=(const RtpCapabilities& o) const { return !(*this == o); }
};

struct RtcpParameters final {
  RtcpParameters();
  RtcpParameters(const RtcpParameters&);
  ~RtcpParameters();

  // The SSRC to be used in the "SSRC of packet sender" field.
  std::optional<uint32_t> ssrc;

  // The Canonical Name (CNAME) used by RTCP (e.g. in SDES messages).
  std::string cname;

  // Send reduced-size RTCP?
  bool reduced_size = false;

  // Send RTCP multiplexed on the RTP transport?
  bool mux = true;

  bool operator==(const RtcpParameters& o) const {
    return ssrc == o.ssrc && cname == o.cname &&
           reduced_size == o.reduced_size && mux == o.mux;
  }
  bool operator!=(const RtcpParameters& o) const { return !(*this == o); }
};

struct RtpParameters {
  RtpParameters();
  RtpParameters(const RtpParameters&);
  ~RtpParameters();

  // Used when calling getParameters/setParameters with a PeerConnection
  // RtpSender, to ensure that outdated parameters are not unintentionally
  // applied successfully.
  std::string transaction_id;

  // Value to use for MID RTP header extension.
  std::string mid;

  std::vector<RtpCodecParameters> codecs;

  std::vector<RtpExtension> header_extensions;

  std::vector<RtpEncodingParameters> encodings;

  // Only available with a Peerconnection RtpSender.
  RtcpParameters rtcp;

  // When bandwidth is constrained and the RtpSender needs to choose between
  // degrading resolution or degrading framerate, degradationPreference
  // indicates which is preferred. Only for video tracks.
  std::optional<DegradationPreference> degradation_preference;

  bool operator==(const RtpParameters& o) const {
    return mid == o.mid && codecs == o.codecs &&
           header_extensions == o.header_extensions &&
           encodings == o.encodings && rtcp == o.rtcp &&
           degradation_preference == o.degradation_preference;
  }
  bool operator!=(const RtpParameters& o) const { return !(*this == o); }
};

}  // namespace rtp_rtcp
}  // namespace media
}  // namespace ave

#endif  // AVE_MODULES_RTP_RTCP_INCLUDE_RTP_PARAMETERS_H_
