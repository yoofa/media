/*
 * sdp.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_SDP_H
#define AVE_MEDIA_SDP_H

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ave {
namespace media {
namespace sdp {

// Implements a minimal SDP model and RFC 8866 compliant
// parser/serializer tailored to AVE needs. It does not depend on WebRTC
// types. Extend incrementally as needed.

// NOTE: This module focuses on common sections/attributes used in RTP/ICE
// sessions: v=, o=, s=, t=, a=group:BUNDLE, a=msid-semantic, a=ice-lite,
// a=extmap-allow-mixed, m=, c=, b=, a=mid, a=rtcp-mux, a=rtcp-rsize,
// a=sendrecv/sendonly/recvonly/inactive, a=rtpmap, a=fmtp, a=candidate.

enum class MediaKind { kAudio, kVideo, kApplication, kUnknown };

enum class Direction { kSendRecv, kSendOnly, kRecvOnly, kInactive };

struct Origin {
  std::string username = "-";         // RFC 8866 section 5.2
  std::string session_id = "0";       // numeric string
  std::string session_version = "0";  // numeric string
  std::string nettype = "IN";
  std::string addrtype = "IP4";
  std::string unicast_address = "0.0.0.0";
};

struct Timing {  // RFC 8866 section 5.9
  // NTP format as decimal integers. Commonly 0 0 for permanent sessions.
  int64_t start_time = 0;
  int64_t stop_time = 0;
};

struct Group {            // RFC 5888
  std::string semantics;  // e.g., "BUNDLE"
  std::vector<std::string> mids;
};

struct Bandwidth {   // RFC 8866 section 5.8
  std::string type;  // e.g., "AS", "CT", "TIAS"
  int value_kbps = 0;
};

struct Connection {              // RFC 8866 section 5.7
  std::string nettype = "IN";    // IN
  std::string addrtype = "IP4";  // IP4 or IP6
  std::string address;           // connection-address (may include TTL/num)
};

struct RtpMap {  // a=rtpmap:<pt> <enc>/<clock>[/<ch>]
  int payload_type = -1;
  std::string encoding;
  int clock_rate_hz = 0;
  int channels = 0;  // 0 if not specified
};

struct Fmtp {  // a=fmtp:<pt> <params>
  int payload_type = -1;
  std::map<std::string, std::string> parameters;  // key -> value
};

struct Candidate {  // ICE candidate line. Stored raw for now.
  std::string raw;  // candidate:<...> (no leading "a=")
};

// Structured ICE candidate (RFC 5245/8445). Parsed in addition to `raw`.
struct IceCandidate {
  std::string foundation;
  int component_id = 1;
  std::string transport;  // udp or tcp
  uint32_t priority = 0;
  std::string ip;
  int port = 0;
  std::string type;  // host|srflx|prflx|relay
  // Optional attributes (if present in SDP)
  std::map<std::string, std::string>
      extensions;   // raddr, rport, tcptype, generation, ufrag, network-id,
                    // network-cost, etc.
  std::string raw;  // original (without leading a=)
};

enum class DtlsSetup {
  kActpass,
  kActive,
  kPassive,
  kHoldconn,
  kUnknown,
};

struct MediaDescription {
  MediaKind kind = MediaKind::kUnknown;
  int port = 9;                      // default for UDP/TLS/RTP/SAVPF
  std::string protocol;              // e.g., "UDP/TLS/RTP/SAVPF"
  std::vector<std::string> formats;  // e.g., payload types ["96", "97"]

  std::optional<Connection> connection;  // media-level c=
  std::vector<Bandwidth> bandwidths;     // media-level b=

  std::string mid;  // a=mid
  Direction direction = Direction::kSendRecv;
  bool rtcp_mux = false;    // a=rtcp-mux
  bool rtcp_rsize = false;  // a=rtcp-rsize

  std::vector<RtpMap> rtp_maps;  // a=rtpmap
  std::vector<Fmtp> fmtps;       // a=fmtp
  // a=candidate (structured)
  std::vector<IceCandidate> ice_candidates;

  // ICE/DTLS
  std::string ice_ufrag;                       // a=ice-ufrag
  std::string ice_pwd;                         // a=ice-pwd
  std::string dtls_fingerprint_algo;           // a=fingerprint:<algo> <fp>
  std::string dtls_fingerprint_value;          // hex with ':' separators
  DtlsSetup dtls_setup = DtlsSetup::kUnknown;  // a=setup:

  // MSID and SSRC signaling
  struct Msid {
    std::string stream_id;
    std::string track_id;
  };
  std::vector<Msid> msids;  // a=msid: <stream> <track>

  struct SsrcEntry {
    uint32_t ssrc = 0;
    std::string cname;           // from a=ssrc:<ssrc> cname:<cname>
    std::string msid_stream_id;  // from a=ssrc:<ssrc> msid:<stream> <track>
    std::string msid_track_id;
  };
  std::vector<SsrcEntry> ssrcs;  // a=ssrc:

  struct SsrcGroup {
    std::string semantics;
    std::vector<uint32_t> ssrcs;
  };
  std::vector<SsrcGroup> ssrc_groups;  // a=ssrc-group:<semantics> ssrc1 ssrc2
};

struct SessionDescription {
  // Session-level
  int version = 0;                 // v=, must be 0 per RFC 8866
  Origin origin;                   // o=
  std::string session_name = "-";  // s=
  Timing timing;                   // t=

  std::optional<Connection> connection;  // session-level c=
  std::vector<Bandwidth> bandwidths;     // session-level b=

  // Attributes
  std::vector<Group> groups;                // a=group:SEM mid1 mid2
  std::vector<std::string> msid_semantics;  // from a=msid-semantic: WMS <id>...
  bool ice_lite = false;                    // a=ice-lite
  bool extmap_allow_mixed = false;          // a=extmap-allow-mixed

  // Media sections
  std::vector<MediaDescription> media;
};

// Utilities
MediaKind ToMediaKind(const std::string& token);
const char* ToString(MediaKind kind);

Direction ToDirection(const std::string& token);
const char* ToString(Direction dir);

// RFC 8866 SDP parsing and serialization
bool Parse(const std::string& sdp_text,
           SessionDescription* out,
           std::string* error_message);

std::string Serialize(const SessionDescription& sdp);

}  // namespace sdp
}  // namespace media
}  // namespace ave

#endif  // AVE_MEDIA_SDP_H
