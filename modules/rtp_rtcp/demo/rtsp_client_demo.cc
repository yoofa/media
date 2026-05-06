/*
 * rtsp_client_demo.cc - Minimal RTSP client demo
 *
 * Demonstrates RTSP connect and OPTIONS/DESCRIBE/SETUP/PLAY/TEARDOWN flow.
 */

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/net/async_tcp_socket.h"
#include "base/net/physical_socket_server.h"
#include "base/net/socket_address.h"

namespace {

using ave::base::net::AsyncPacketSocket;
using ave::base::net::AsyncTCPSocket;
using ave::base::net::PhysicalSocketServer;
using ave::base::net::Socket;
using ave::base::net::SocketAddress;
constexpr uint16_t kDefaultRtspPort = 8554;
constexpr uint16_t kClientRtpPort = 8000;
constexpr uint16_t kClientRtcpPort = 8001;

std::string Trim(const std::string& value) {
  size_t start = 0;
  while (start < value.size() &&
         std::isspace(static_cast<uint8_t>(value[start]))) {
    ++start;
  }
  size_t end = value.size();
  while (end > start && std::isspace(static_cast<uint8_t>(value[end - 1]))) {
    --end;
  }
  return value.substr(start, end - start);
}

std::string ToLower(std::string value) {
  std::ranges::transform(value, value.begin(), [](uint8_t ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

std::vector<std::string> SplitLines(const std::string& message) {
  std::vector<std::string> lines;
  size_t start = 0;
  while (start < message.size()) {
    size_t end = message.find("\r\n", start);
    if (end == std::string::npos) {
      lines.push_back(message.substr(start));
      break;
    }
    lines.push_back(message.substr(start, end - start));
    start = end + 2;
  }
  return lines;
}

struct RtspResponse {
  int32_t status_code = 0;
  int32_t cseq = 0;
  std::map<std::string, std::string> headers;
  std::string body;
};

bool ParseResponseHeader(const std::string& message, RtspResponse* response) {
  std::vector<std::string> lines = SplitLines(message);
  if (lines.empty()) {
    return false;
  }
  const std::string& status_line = lines.front();
  size_t first_space = status_line.find(' ');
  if (first_space == std::string::npos) {
    return false;
  }
  size_t second_space = status_line.find(' ', first_space + 1);
  if (second_space == std::string::npos) {
    return false;
  }
  response->status_code = std::atoi(
      status_line.substr(first_space + 1, second_space - first_space - 1)
          .c_str());
  for (size_t i = 1; i < lines.size(); ++i) {
    if (lines[i].empty()) {
      break;
    }
    size_t colon = lines[i].find(':');
    if (colon == std::string::npos) {
      continue;
    }
    std::string key = ToLower(Trim(lines[i].substr(0, colon)));
    std::string value = Trim(lines[i].substr(colon + 1));
    response->headers[key] = value;
  }
  auto cseq_it = response->headers.find("cseq");
  if (cseq_it != response->headers.end()) {
    response->cseq = std::atoi(cseq_it->second.c_str());
  }
  return response->status_code != 0;
}

class RtspClient final : public sigslot::has_slots<> {
 public:
  RtspClient(PhysicalSocketServer* server, const SocketAddress& remote_addr)
      : socket_server_(server), remote_addr_(remote_addr) {}

  bool Start() {
    Socket* socket = socket_server_->CreateSocket(AF_INET, SOCK_STREAM);
    if (!socket) {
      printf("Failed to create client socket\n");
      return false;
    }
    SocketAddress bind_addr;
    socket_.reset(AsyncTCPSocket::Create(socket, bind_addr, remote_addr_));
    if (!socket_) {
      printf("Failed to connect to %s\n", remote_addr_.ToString().c_str());
      return false;
    }
    socket_->SignalConnect.connect(this, &RtspClient::OnConnect);
    socket_->SignalReadPacket.connect(this, &RtspClient::OnReadPacket);
    socket_->SignalClose.connect(this, &RtspClient::OnClose);
    running_ = true;
    return true;
  }

  void Run() {
    while (running_) {
      socket_server_->Wait(100);
    }
  }

 private:
  enum class State { kIdle, kOptions, kDescribe, kSetup, kPlay, kTeardown };

  void OnConnect(AsyncPacketSocket* /* socket */) {
    printf("Connected to RTSP server %s\n", remote_addr_.ToString().c_str());
    state_ = State::kOptions;
    SendOptions();
  }

  void OnClose(AsyncPacketSocket* /* socket */, int32_t /* error */) {
    running_ = false;
  }

  void OnReadPacket(AsyncPacketSocket* /* socket */,
                    const uint8_t* data,
                    size_t size,
                    const SocketAddress& /* addr */,
                    int64_t /* timestamp */) {
    buffer_.append(reinterpret_cast<const char*>(data), size);
    for (;;) {
      size_t header_end = buffer_.find("\r\n\r\n");
      if (header_end == std::string::npos) {
        return;
      }
      std::string header = buffer_.substr(0, header_end + 4);
      RtspResponse response;
      if (!ParseResponseHeader(header, &response)) {
        return;
      }
      size_t body_start = header_end + 4;
      size_t content_length = 0;
      auto it = response.headers.find("content-length");
      if (it != response.headers.end()) {
        content_length = static_cast<size_t>(std::atoi(it->second.c_str()));
      }
      if (buffer_.size() < body_start + content_length) {
        return;
      }
      response.body = buffer_.substr(body_start, content_length);
      buffer_.erase(0, body_start + content_length);
      HandleResponse(response);
    }
  }

  void HandleResponse(const RtspResponse& response) {
    if (response.status_code != 200) {
      printf("RTSP error: %d\n", response.status_code);
      running_ = false;
      return;
    }

    if (state_ == State::kOptions) {
      state_ = State::kDescribe;
      SendDescribe();
      return;
    }

    if (state_ == State::kDescribe) {
      printf("Received SDP (%zu bytes)\n", response.body.size());
      state_ = State::kSetup;
      SendSetup();
      return;
    }

    if (state_ == State::kSetup) {
      auto session_it = response.headers.find("session");
      if (session_it != response.headers.end()) {
        session_id_ = session_it->second;
        size_t semi = session_id_.find(';');
        if (semi != std::string::npos) {
          session_id_ = session_id_.substr(0, semi);
        }
        session_id_ = Trim(session_id_);
      }
      state_ = State::kPlay;
      SendPlay();
      return;
    }

    if (state_ == State::kPlay) {
      state_ = State::kTeardown;
      SendTeardown();
      return;
    }

    if (state_ == State::kTeardown) {
      running_ = false;
      if (socket_) {
        socket_->Close();
      }
    }
  }

  void SendRequest(const std::string& method,
                   const std::string& uri,
                   const std::vector<std::string>& headers) {
    std::string request = method + " " + uri + " RTSP/1.0\r\n" +
                          "CSeq: " + std::to_string(++cseq_) + "\r\n" +
                          "User-Agent: ave-rtsp-demo\r\n";
    for (const auto& header : headers) {
      request += header + "\r\n";
    }
    request += "\r\n";
    socket_->Send(request.data(), request.size());
    printf("Sent %s\n", method.c_str());
  }

  std::string BaseUri() const {
    return "rtsp://" + remote_addr_.ToString() + "/demo";
  }

  void SendOptions() { SendRequest("OPTIONS", BaseUri(), {}); }

  void SendDescribe() {
    SendRequest("DESCRIBE", BaseUri(), {"Accept: application/sdp"});
  }

  void SendSetup() {
    std::string transport = "Transport: RTP/AVP;unicast;client_port=" +
                            std::to_string(kClientRtpPort) + "-" +
                            std::to_string(kClientRtcpPort);
    SendRequest("SETUP", BaseUri() + "/streamid=0", {transport});
  }

  void SendPlay() {
    std::vector<std::string> headers;
    if (!session_id_.empty()) {
      headers.push_back("Session: " + session_id_);
    }
    headers.emplace_back("Range: npt=0.000-");
    SendRequest("PLAY", BaseUri(), headers);
  }

  void SendTeardown() {
    std::vector<std::string> headers;
    if (!session_id_.empty()) {
      headers.push_back("Session: " + session_id_);
    }
    SendRequest("TEARDOWN", BaseUri(), headers);
  }

  PhysicalSocketServer* socket_server_;
  SocketAddress remote_addr_;
  std::unique_ptr<AsyncTCPSocket> socket_;
  std::string buffer_;
  std::string session_id_;
  int32_t cseq_ = 0;
  bool running_ = false;
  State state_ = State::kIdle;
};

}  // namespace

int main(int32_t argc, char* argv[]) {
  std::string host = "127.0.0.1";
  uint16_t port = kDefaultRtspPort;
  if (argc > 1) {
    host = argv[1];
  }
  if (argc > 2) {
    port = static_cast<uint16_t>(std::atoi(argv[2]));
  }

  SocketAddress remote_addr(host, port);
  PhysicalSocketServer socket_server;
  RtspClient client(&socket_server, remote_addr);
  if (!client.Start()) {
    return 1;
  }
  client.Run();
  return 0;
}
