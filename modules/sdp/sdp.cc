/*
 * sdp.cc
 * Copyright (C) 2025
 */

#include "sdp.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <sstream>

namespace ave {
namespace media {
namespace sdp {

// Helpers
namespace {

inline std::string Trim(const std::string& s) {
  size_t b = 0, e = s.size();
  while (b < e && std::isspace(static_cast<unsigned char>(s[b])))
    ++b;
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])))
    --e;
  return s.substr(b, e - b);
}

inline bool StartsWith(const std::string& s, const char* pfx) {
  size_t n = std::char_traits<char>::length(pfx);
  return s.size() >= n && std::equal(pfx, pfx + n, s.begin());
}

inline std::vector<std::string> Split(const std::string& s,
                                      char delim,
                                      bool skip_empty = false) {
  std::vector<std::string> out;
  std::string cur;
  std::istringstream is(s);
  while (std::getline(is, cur, delim)) {
    if (skip_empty && cur.empty())
      continue;
    out.push_back(cur);
  }
  return out;
}

inline std::pair<std::string, std::string> SplitOnce(const std::string& s,
                                                     char delim) {
  auto pos = s.find(delim);
  if (pos == std::string::npos)
    return {s, std::string()};
  return {s.substr(0, pos), s.substr(pos + 1)};
}

inline int ToInt(const std::string& s, int fallback = 0) {
  if (s.empty())
    return fallback;
  char* end = nullptr;
  long v = std::strtol(s.c_str(), &end, 10);
  if (end == s.c_str())
    return fallback;
  if (v > std::numeric_limits<int>::max())
    return fallback;
  if (v < std::numeric_limits<int>::min())
    return fallback;
  return static_cast<int>(v);
}

inline int64_t ToInt64(const std::string& s, int64_t fallback = 0) {
  if (s.empty())
    return fallback;
  char* end = nullptr;
  long long v = std::strtoll(s.c_str(), &end, 10);
  if (end == s.c_str())
    return fallback;
  return static_cast<int64_t>(v);
}

}  // namespace

MediaKind ToMediaKind(const std::string& token) {
  if (token == "audio")
    return MediaKind::kAudio;
  if (token == "video")
    return MediaKind::kVideo;
  if (token == "application")
    return MediaKind::kApplication;
  return MediaKind::kUnknown;
}

const char* ToString(MediaKind kind) {
  switch (kind) {
    case MediaKind::kAudio:
      return "audio";
    case MediaKind::kVideo:
      return "video";
    case MediaKind::kApplication:
      return "application";
    default:
      return "unknown";
  }
}

Direction ToDirection(const std::string& token) {
  if (token == "sendrecv")
    return Direction::kSendRecv;
  if (token == "sendonly")
    return Direction::kSendOnly;
  if (token == "recvonly")
    return Direction::kRecvOnly;
  if (token == "inactive")
    return Direction::kInactive;
  return Direction::kSendRecv;
}

const char* ToString(Direction d) {
  switch (d) {
    case Direction::kSendRecv:
      return "sendrecv";
    case Direction::kSendOnly:
      return "sendonly";
    case Direction::kRecvOnly:
      return "recvonly";
    case Direction::kInactive:
      return "inactive";
  }
  return "sendrecv";
}

static void SerializeSessionLine(const SessionDescription& s,
                                 std::string* out) {
  out->append("v=0\r\n");
  // o=<username> <sess-id> <sess-version> <nettype> <addrtype>
  // <unicast-address>
  out->append("o=");
  out->append(s.origin.username);
  out->push_back(' ');
  out->append(s.origin.session_id);
  out->push_back(' ');
  out->append(s.origin.session_version);
  out->append(" IN ");
  out->append(s.origin.addrtype.empty() ? "IP4" : s.origin.addrtype);
  out->push_back(' ');
  out->append(s.origin.unicast_address);
  out->append("\r\n");

  out->append("s=");
  out->append(s.session_name.empty() ? "-" : s.session_name);
  out->append("\r\n");

  // t=
  {
    std::ostringstream os;
    os << "t=" << s.timing.start_time << ' ' << s.timing.stop_time << "\r\n";
    out->append(os.str());
  }
}

static void SerializeConnectionBandwidthAttrs(const Connection* c,
                                              const std::vector<Bandwidth>& bws,
                                              std::string* out) {
  if (c) {
    out->append("c=");
    out->append(c->nettype.empty() ? "IN" : c->nettype);
    out->push_back(' ');
    out->append(c->addrtype.empty() ? "IP4" : c->addrtype);
    out->push_back(' ');
    out->append(c->address);
    out->append("\r\n");
  }
  for (const auto& b : bws) {
    std::ostringstream os;
    os << "b=" << (b.type.empty() ? "AS" : b.type) << ':' << b.value_kbps
       << "\r\n";
    out->append(os.str());
  }
}

static void SerializeSessionAttributes(const SessionDescription& s,
                                       std::string* out) {
  // a=group
  for (const auto& g : s.groups) {
    if (g.semantics.empty() || g.mids.empty())
      continue;
    std::ostringstream os;
    os << "a=group:" << g.semantics;
    for (const auto& mid : g.mids)
      os << ' ' << mid;
    os << "\r\n";
    out->append(os.str());
  }
  // a=msid-semantic
  if (!s.msid_semantics.empty()) {
    std::ostringstream os;
    os << "a=msid-semantic: WMS";
    for (const auto& id : s.msid_semantics)
      os << ' ' << id;
    os << "\r\n";
    out->append(os.str());
  }
  if (s.ice_lite)
    out->append("a=ice-lite\r\n");
  if (s.extmap_allow_mixed)
    out->append("a=extmap-allow-mixed\r\n");
}

static void SerializeRtpSection(const MediaDescription& m, std::string* out) {
  // m=<media> <port> <proto> <fmt list>
  std::ostringstream mline;
  mline << "m=" << ToString(m.kind) << ' ' << m.port << ' ' << m.protocol;
  for (const auto& f : m.formats)
    mline << ' ' << f;
  mline << "\r\n";
  out->append(mline.str());

  SerializeConnectionBandwidthAttrs(m.connection ? &*m.connection : nullptr,
                                    m.bandwidths, out);

  if (!m.mid.empty()) {
    out->append("a=mid:");
    out->append(m.mid);
    out->append("\r\n");
  }
  // direction
  out->append("a=");
  out->append(ToString(m.direction));
  out->append("\r\n");

  if (m.rtcp_mux)
    out->append("a=rtcp-mux\r\n");
  if (m.rtcp_rsize)
    out->append("a=rtcp-rsize\r\n");

  // ICE and DTLS
  if (!m.ice_ufrag.empty()) {
    out->append("a=ice-ufrag:");
    out->append(m.ice_ufrag);
    out->append("\r\n");
  }
  if (!m.ice_pwd.empty()) {
    out->append("a=ice-pwd:");
    out->append(m.ice_pwd);
    out->append("\r\n");
  }
  if (!m.dtls_fingerprint_algo.empty() && !m.dtls_fingerprint_value.empty()) {
    out->append("a=fingerprint:");
    out->append(m.dtls_fingerprint_algo);
    out->push_back(' ');
    out->append(m.dtls_fingerprint_value);
    out->append("\r\n");
  }
  switch (m.dtls_setup) {
    case DtlsSetup::kActpass:
      out->append("a=setup:actpass\r\n");
      break;
    case DtlsSetup::kActive:
      out->append("a=setup:active\r\n");
      break;
    case DtlsSetup::kPassive:
      out->append("a=setup:passive\r\n");
      break;
    case DtlsSetup::kHoldconn:
      out->append("a=setup:holdconn\r\n");
      break;
    case DtlsSetup::kUnknown:
    default:
      break;
  }

  // rtpmap/fmtp
  for (const auto& rm : m.rtp_maps) {
    std::ostringstream os;
    os << "a=rtpmap:" << rm.payload_type << ' ' << rm.encoding << '/'
       << rm.clock_rate_hz;
    if (rm.channels > 0)
      os << '/' << rm.channels;
    os << "\r\n";
    out->append(os.str());
  }
  for (const auto& fp : m.fmtps) {
    std::ostringstream os;
    os << "a=fmtp:" << fp.payload_type << ' ';
    bool first = true;
    for (const auto& kv : fp.parameters) {
      if (!first)
        os << ';';
      os << kv.first;
      if (!kv.second.empty())
        os << '=' << kv.second;
      first = false;
    }
    os << "\r\n";
    out->append(os.str());
  }

  // msid lines
  for (const auto& ms : m.msids) {
    out->append("a=msid:");
    out->append(ms.stream_id);
    if (!ms.track_id.empty()) {
      out->push_back(' ');
      out->append(ms.track_id);
    }
    out->append("\r\n");
  }

  // ssrc-group lines
  for (const auto& grp : m.ssrc_groups) {
    std::ostringstream os;
    os << "a=ssrc-group:" << grp.semantics;
    for (auto sid : grp.ssrcs)
      os << ' ' << sid;
    os << "\r\n";
    out->append(os.str());
  }

  // ssrc attribute lines
  for (const auto& s : m.ssrcs) {
    if (s.ssrc == 0)
      continue;
    if (!s.cname.empty()) {
      std::ostringstream os;
      os << "a=ssrc:" << s.ssrc << " cname:" << s.cname << "\r\n";
      out->append(os.str());
    }
    if (!s.msid_stream_id.empty()) {
      std::ostringstream os;
      os << "a=ssrc:" << s.ssrc << " msid:" << s.msid_stream_id;
      if (!s.msid_track_id.empty())
        os << ' ' << s.msid_track_id;
      os << "\r\n";
      out->append(os.str());
    }
  }

  // ICE candidates
  for (const auto& c : m.ice_candidates) {
    out->append("a=");
    if (!c.raw.empty()) {
      out->append(c.raw);
    } else {
      std::ostringstream os;
      os << "candidate:" << c.foundation << ' ' << c.component_id << ' '
         << c.transport << ' ' << c.priority << ' ' << c.ip << ' ' << c.port
         << " typ " << c.type;
      for (const auto& kv : c.extensions) {
        os << ' ' << kv.first;
        if (!kv.second.empty())
          os << ' ' << kv.second;
      }
      out->append(os.str());
    }
    out->append("\r\n");
  }
}

std::string Serialize(const SessionDescription& sdp) {
  std::string out;
  SerializeSessionLine(sdp, &out);
  SerializeConnectionBandwidthAttrs(sdp.connection ? &*sdp.connection : nullptr,
                                    sdp.bandwidths, &out);
  SerializeSessionAttributes(sdp, &out);

  for (const auto& m : sdp.media)
    SerializeRtpSection(m, &out);

  return out;
}

static bool ParseConnectionLine(const std::string& line, Connection* out) {
  // c=<nettype> <addrtype> <connection-address>
  auto rest = line.substr(2);
  auto parts = Split(Trim(rest), ' ', true);
  if (parts.size() < 3)
    return false;
  out->nettype = parts[0];
  out->addrtype = parts[1];
  out->address = parts[2];
  return true;
}

static void ParseBandwidthLine(const std::string& line, Bandwidth* out) {
  // b=<bwtype>:<bandwidth>
  auto rest = line.substr(2);
  auto kv = SplitOnce(Trim(rest), ':');
  out->type = Trim(kv.first);
  out->value_kbps = ToInt(Trim(kv.second), 0);
}

static void ParseGroupAttr(const std::string& line, Group* out) {
  // a=group:<semantics> mid1 mid2 ...
  auto rest = line.substr(8);  // after "a=group:"
  auto parts = Split(Trim(rest), ' ', true);
  if (!parts.empty()) {
    out->semantics = parts[0];
    out->mids.assign(parts.begin() + 1, parts.end());
  }
}

static void ParseMsidSemantic(const std::string& line,
                              std::vector<std::string>* out_ids) {
  // a=msid-semantic: WMS <id> ...
  auto pos = line.find(':');
  if (pos == std::string::npos)
    return;
  auto rest = Trim(line.substr(pos + 1));
  auto parts = Split(rest, ' ', true);
  // parts[0] should be WMS; keep following ids
  if (parts.size() > 1) {
    out_ids->insert(out_ids->end(), parts.begin() + 1, parts.end());
  }
}

static void ParseRtpMap(const std::string& line, RtpMap* out) {
  // a=rtpmap:<pt> <enc>/<clock>[/<ch>]
  auto rest = line.substr(9);  // after "a=rtpmap:"
  auto kv = SplitOnce(Trim(rest), ' ');
  out->payload_type = ToInt(kv.first, -1);
  auto enc = Split(kv.second, '/', true);
  if (!enc.empty())
    out->encoding = enc[0];
  if (enc.size() >= 2)
    out->clock_rate_hz = ToInt(enc[1]);
  if (enc.size() >= 3)
    out->channels = ToInt(enc[2]);
}

static void ParseFmtp(const std::string& line, Fmtp* out) {
  // a=fmtp:<pt> key=val;key2=val2
  auto rest = line.substr(7);  // after "a=fmtp:"
  auto kv = SplitOnce(Trim(rest), ' ');
  out->payload_type = ToInt(kv.first, -1);
  auto parts = Split(kv.second, ';', true);
  for (auto& p : parts) {
    auto kv2 = SplitOnce(Trim(p), '=');
    auto key = Trim(kv2.first);
    auto val = Trim(kv2.second);
    out->parameters[key] = val;
  }
}

// Legacy: keep for potential compatibility. Currently unused.
[[maybe_unused]] static void ParseCandidate(const std::string& line,
                                            Candidate* out) {
  // a=candidate:...
  out->raw = line.substr(2);  // keep without leading a=
}

static IceCandidate ParseIceCandidate(const std::string& line) {
  IceCandidate c;
  c.raw = line.substr(2);
  auto rest = c.raw;
  auto parts = Split(Trim(rest), ' ', true);
  if (parts.size() < 8)
    return c;
  auto p0 = parts[0];
  auto col = p0.find(':');
  c.foundation = (col == std::string::npos) ? p0 : p0.substr(col + 1);
  c.component_id = ToInt(parts[1], 1);
  c.transport = parts[2];
  c.priority = static_cast<uint32_t>(ToInt64(parts[3]));
  c.ip = parts[4];
  c.port = ToInt(parts[5], 0);
  // find typ
  size_t i = 6;
  for (; i + 1 < parts.size(); ++i) {
    if (parts[i] == "typ") {
      c.type = parts[i + 1];
      i += 2;
      break;
    }
  }
  while (i < parts.size()) {
    std::string key = parts[i++];
    std::string val;
    if (i < parts.size()) {
      val = parts[i];
      if (key == "raddr" || key == "rport" || key == "tcptype" ||
          key == "ufrag" || key == "generation" || key == "network-id" ||
          key == "network-cost") {
        ++i;
      } else {
        val.clear();
      }
    }
    c.extensions[key] = val;
  }
  return c;
}

static bool ParseMediaLine(const std::string& line, MediaDescription* out) {
  // m=<media> <port> <proto> <fmt> ...
  auto rest = line.substr(2);
  auto parts = Split(Trim(rest), ' ', true);
  if (parts.size() < 3)
    return false;
  out->kind = ToMediaKind(parts[0]);
  out->port = ToInt(parts[1], 0);
  out->protocol = parts[2];
  out->formats.assign(parts.begin() + 3, parts.end());
  return true;
}

bool Parse(const std::string& sdp_text,
           SessionDescription* out,
           std::string* error_message) {
  if (!out)
    return false;
  *out = SessionDescription{};  // reset to defaults

  std::istringstream is(sdp_text);
  std::string line;
  MediaDescription* current_m = nullptr;

  auto set_error = [&](const std::string& why) {
    if (error_message)
      *error_message = why;
  };

  while (std::getline(is, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.size() < 2 || line[1] != '=')
      continue;  // skip invalid

    switch (line[0]) {
      case 'v': {
        out->version = ToInt(line.substr(2), 0);
        break;
      }
      case 'o': {
        // o=<username> <sess-id> <sess-version> <nettype> <addrtype> <addr>
        auto rest = Split(Trim(line.substr(2)), ' ', true);
        if (rest.size() >= 6) {
          out->origin.username = rest[0];
          out->origin.session_id = rest[1];
          out->origin.session_version = rest[2];
          out->origin.nettype = rest[3];
          out->origin.addrtype = rest[4];
          out->origin.unicast_address = rest[5];
        }
        break;
      }
      case 's': {
        out->session_name = line.substr(2);
        break;
      }
      case 't': {
        auto parts = Split(Trim(line.substr(2)), ' ', true);
        if (parts.size() >= 2) {
          out->timing.start_time = ToInt64(parts[0]);
          out->timing.stop_time = ToInt64(parts[1]);
        }
        break;
      }
      case 'c': {
        Connection c;
        if (!ParseConnectionLine(line, &c)) {
          set_error("Invalid c= line");
          return false;
        }
        if (current_m)
          current_m->connection = c;
        else
          out->connection = c;
        break;
      }
      case 'b': {
        Bandwidth b;
        ParseBandwidthLine(line, &b);
        if (current_m)
          current_m->bandwidths.push_back(b);
        else
          out->bandwidths.push_back(b);
        break;
      }
      case 'm': {
        MediaDescription m;
        if (!ParseMediaLine(line, &m)) {
          set_error("Invalid m= line");
          return false;
        }
        out->media.push_back(std::move(m));
        current_m = &out->media.back();
        break;
      }
      case 'a': {
        if (StartsWith(line, "a=group:")) {
          Group g;
          ParseGroupAttr(line, &g);
          out->groups.push_back(std::move(g));
          break;
        }
        if (StartsWith(line, "a=msid-semantic:")) {
          ParseMsidSemantic(line, &out->msid_semantics);
          break;
        }
        if (line == "a=ice-lite") {
          out->ice_lite = true;
          break;
        }
        if (line == "a=extmap-allow-mixed") {
          out->extmap_allow_mixed = true;
          break;
        }

        if (!current_m) {
          // session-level unknown attributes are ignored for now
          break;
        }
        if (StartsWith(line, "a=mid:")) {
          current_m->mid = Trim(line.substr(6));
          break;
        }
        if (line == "a=rtcp-mux") {
          current_m->rtcp_mux = true;
          break;
        }
        if (line == "a=rtcp-rsize") {
          current_m->rtcp_rsize = true;
          break;
        }
        if (line == "a=sendrecv") {
          current_m->direction = Direction::kSendRecv;
          break;
        }
        if (line == "a=sendonly") {
          current_m->direction = Direction::kSendOnly;
          break;
        }
        if (line == "a=recvonly") {
          current_m->direction = Direction::kRecvOnly;
          break;
        }
        if (line == "a=inactive") {
          current_m->direction = Direction::kInactive;
          break;
        }
        if (StartsWith(line, "a=rtpmap:")) {
          RtpMap rm;
          ParseRtpMap(line, &rm);
          current_m->rtp_maps.push_back(rm);
          break;
        }
        if (StartsWith(line, "a=fmtp:")) {
          Fmtp fp;
          ParseFmtp(line, &fp);
          current_m->fmtps.push_back(fp);
          break;
        }
        if (StartsWith(line, "a=candidate:")) {
          current_m->ice_candidates.push_back(ParseIceCandidate(line));
          break;
        }
        if (StartsWith(line, "a=ice-ufrag:")) {
          current_m->ice_ufrag = Trim(line.substr(12));
          break;
        }
        if (StartsWith(line, "a=ice-pwd:")) {
          current_m->ice_pwd = Trim(line.substr(10));
          break;
        }
        if (StartsWith(line, "a=fingerprint:")) {
          auto rest = Trim(line.substr(14));
          auto kv = SplitOnce(rest, ' ');
          current_m->dtls_fingerprint_algo = Trim(kv.first);
          current_m->dtls_fingerprint_value = Trim(kv.second);
          break;
        }
        if (StartsWith(line, "a=setup:")) {
          auto v = Trim(line.substr(8));
          if (v == "actpass")
            current_m->dtls_setup = DtlsSetup::kActpass;
          else if (v == "active")
            current_m->dtls_setup = DtlsSetup::kActive;
          else if (v == "passive")
            current_m->dtls_setup = DtlsSetup::kPassive;
          else if (v == "holdconn")
            current_m->dtls_setup = DtlsSetup::kHoldconn;
          else
            current_m->dtls_setup = DtlsSetup::kUnknown;
          break;
        }
        if (StartsWith(line, "a=msid:")) {
          auto rest = Trim(line.substr(7));
          auto parts = Split(rest, ' ', true);
          MediaDescription::Msid m;
          if (!parts.empty())
            m.stream_id = parts[0];
          if (parts.size() > 1)
            m.track_id = parts[1];
          current_m->msids.push_back(std::move(m));
          break;
        }
        if (StartsWith(line, "a=ssrc-group:")) {
          auto rest = Trim(line.substr(13));
          auto parts = Split(rest, ' ', true);
          if (!parts.empty()) {
            MediaDescription::SsrcGroup grp;
            grp.semantics = parts[0];
            for (size_t i = 1; i < parts.size(); ++i)
              grp.ssrcs.push_back(static_cast<uint32_t>(ToInt64(parts[i])));
            current_m->ssrc_groups.push_back(std::move(grp));
          }
          break;
        }
        if (StartsWith(line, "a=ssrc:")) {
          auto rest = Trim(line.substr(7));
          auto space = rest.find(' ');
          if (space != std::string::npos) {
            auto id_str = rest.substr(0, space);
            auto attr = Trim(rest.substr(space + 1));
            uint32_t sid = static_cast<uint32_t>(ToInt64(id_str));
            MediaDescription::SsrcEntry entry;
            entry.ssrc = sid;
            auto kv = SplitOnce(attr, ':');
            auto key = Trim(kv.first);
            auto val = Trim(kv.second);
            if (key == "cname") {
              entry.cname = val;
            } else if (key == "msid") {
              auto p = Split(val, ' ', true);
              if (!p.empty())
                entry.msid_stream_id = p[0];
              if (p.size() > 1)
                entry.msid_track_id = p[1];
            }
            current_m->ssrcs.push_back(std::move(entry));
          }
          break;
        }
        // Unknown a= lines at media-level are ignored for now
        break;
      }
      default: {
        // Ignore other sections: u=, e=, p=, k=, r=, z=, etc.
        break;
      }
    }
  }

  // Basic validation according to RFC 8866
  if (out->version != 0) {
    set_error("Unsupported SDP version (v!=0)");
    return false;
  }
  if (out->session_name.empty())
    out->session_name = "-";
  return true;
}

}  // namespace sdp
}  // namespace media
}  // namespace ave
