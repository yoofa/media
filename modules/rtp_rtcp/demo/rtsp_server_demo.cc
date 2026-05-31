/*
 * rtsp_server_demo.cc - Minimal RTSP server demo
 *
 * Demonstrates RTSP listen/accept and OPTIONS/DESCRIBE/SETUP/PLAY/TEARDOWN.
 */

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/net/async_tcp_socket.h"
#include "base/net/physical_socket_server.h"
#include "base/net/socket_address.h"
#include "base/net/socket_dispatcher.h"
#include "media/modules/rtp_rtcp/api/rtp_rtcp_defines.h"

namespace {

using ave::base::net::AsyncPacketSocket;
using ave::base::net::AsyncTCPSocket;
using ave::base::net::PhysicalSocketServer;
using ave::base::net::Socket;
using ave::base::net::SocketAddress;
using ave::base::net::SocketDispatcher;
using ave::media::rtp_rtcp::kVideoPayloadTypeFrequency;

constexpr uint16_t kDefaultRtspPort = 8554;
constexpr int kPayloadType = 96;
constexpr uint16_t kServerRtpPort = 9000;
constexpr uint16_t kServerRtcpPort = 9001;

std::string Trim(const std::string& value) {
  size_t start = 0;
  while (start < value.size() &&
         std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }
  size_t end = value.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }
  return value.substr(start, end - start);
}

std::string ToLower(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
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

struct RtspRequest {
  std::string method;
  std::string uri;
  int cseq = 0;
  std::map<std::string, std::string> headers;
};

bool ParseRequest(const std::string& message, RtspRequest* request) {
  std::vector<std::string> lines = SplitLines(message);
  if (lines.empty()) {
    return false;
  }
  const std::string& request_line = lines.front();
  size_t first_space = request_line.find(' ');
  if (first_space == std::string::npos) {
    return false;
  }
  size_t second_space = request_line.find(' ', first_space + 1);
  if (second_space == std::string::npos) {
    return false;
  }
  request->method = request_line.substr(0, first_space);
  request->uri =
      request_line.substr(first_space + 1, second_space - first_space - 1);

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
    request->headers[key] = value;
  }

  auto cseq_it = request->headers.find("cseq");
  if (cseq_it != request->headers.end()) {
    request->cseq = std::atoi(cseq_it->second.c_str());
  }

  return !request->method.empty();
}

bool ParseClientPorts(const std::string& transport,
                      uint16_t* rtp_port,
                      uint16_t* rtcp_port) {
  size_t pos = transport.find("client_port=");
  if (pos == std::string::npos) {
    return false;
  }
  pos += strlen("client_port=");
  size_t end = transport.find(';', pos);
  std::string ports = transport.substr(pos, end - pos);
  size_t dash = ports.find('-');
  if (dash == std::string::npos) {
    return false;
  }
  *rtp_port = static_cast<uint16_t>(std::atoi(ports.substr(0, dash).c_str()));
  *rtcp_port = static_cast<uint16_t>(std::atoi(ports.substr(dash + 1).c_str()));
  return *rtp_port != 0 && *rtcp_port != 0;
}

std::string BuildSdp() {
  char buffer[512];
  std::snprintf(buffer, sizeof(buffer),
                "v=0\r\n"
                "o=- 0 0 IN IP4 127.0.0.1\r\n"
                "s=AVE RTSP Demo\r\n"
                "t=0 0\r\n"
                "m=video 0 RTP/AVP %d\r\n"
                "a=rtpmap:%d H264/%d\r\n",
                kPayloadType, kPayloadType, kVideoPayloadTypeFrequency);
  return std::string(buffer);
}

std::string BuildResponse(int code,
                          const std::string& status,
                          int cseq,
                          const std::vector<std::string>& headers,
                          const std::string& body) {
  std::string response =
      "RTSP/1.0 " + std::to_string(code) + " " + status + "\r\n";
  if (cseq > 0) {
    response += "CSeq: " + std::to_string(cseq) + "\r\n";
  }
  for (const auto& header : headers) {
    response += header + "\r\n";
  }
  if (!body.empty()) {
    response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
  }
  response += "\r\n";
  response += body;
  return response;
}

class RtspServer;

class RtspConnection final : public sigslot::has_slots<> {
 public:
  RtspConnection(RtspServer* server, Socket* socket, const SocketAddress& peer)
      : server_(server), peer_(peer) {
    tcp_socket_ = std::make_unique<AsyncTCPSocket>(socket);
    tcp_socket_->SignalReadPacket.connect(this, &RtspConnection::OnReadPacket);
    tcp_socket_->SignalClose.connect(this, &RtspConnection::OnClose);
  }

  void Send(const std::string& data) {
    if (!tcp_socket_) {
      return;
    }
    tcp_socket_->Send(data.data(), data.size());
  }

  const SocketAddress& peer() const { return peer_; }

 private:
  void OnReadPacket(AsyncPacketSocket* /* socket */,
                    const uint8_t* data,
                    size_t size,
                    const SocketAddress& /* addr */,
                    int64_t /* timestamp */) {
    buffer_.append(reinterpret_cast<const char*>(data), size);
    for (;;) {
      size_t end = buffer_.find("\r\n\r\n");
      if (end == std::string::npos) {
        return;
      }
      std::string message = buffer_.substr(0, end + 4);
      buffer_.erase(0, end + 4);
      RtspRequest request;
      if (ParseRequest(message, &request)) {
        HandleRequest(request);
      }
    }
  }

  void OnClose(AsyncPacketSocket* /* socket */, int32_t /* error */);

  void HandleRequest(const RtspRequest& request);

  RtspServer* server_;
  SocketAddress peer_;
  std::unique_ptr<AsyncTCPSocket> tcp_socket_;
  std::string buffer_;
  std::string session_id_ = "12345678";
};

class RtspServer final : public sigslot::has_slots<> {
 public:
  explicit RtspServer(uint16_t port) : port_(port), running_(false) {}

  bool Start() {
    listen_socket_.reset(socket_server_.CreateSocket(AF_INET, SOCK_STREAM));
    if (!listen_socket_) {
      printf("Failed to create listen socket\n");
      return false;
    }
    SocketAddress bind_addr("0.0.0.0", port_);
    if (listen_socket_->Bind(bind_addr) < 0) {
      printf("Bind failed on %s\n", bind_addr.ToString().c_str());
      return false;
    }
    if (listen_socket_->Listen(1) < 0) {
      printf("Listen failed on %s\n", bind_addr.ToString().c_str());
      return false;
    }
    socket_server_.Update(static_cast<SocketDispatcher*>(listen_socket_.get()));
    listen_socket_->SignalReadEvent.connect(this, &RtspServer::OnAccept);
    printf("RTSP server listening on %s\n", bind_addr.ToString().c_str());
    return true;
  }

  void Run() {
    running_ = true;
    while (running_) {
      socket_server_.Wait(100);
    }
  }

  void Stop() { running_ = false; }

  void OnConnectionClosed() { Stop(); }

 private:
  void OnAccept(Socket* /* socket */) {
    if (connection_) {
      return;
    }
    SocketAddress peer;
    Socket* client_socket = listen_socket_->Accept(&peer);
    if (!client_socket) {
      return;
    }
    connection_ = std::make_unique<RtspConnection>(this, client_socket, peer);
    printf("Accepted RTSP client %s\n", peer.ToString().c_str());
  }

  uint16_t port_;
  bool running_;
  PhysicalSocketServer socket_server_;
  std::unique_ptr<Socket> listen_socket_;
  std::unique_ptr<RtspConnection> connection_;
};

void RtspConnection::OnClose(AsyncPacketSocket* /* socket */,
                             int32_t /* error */) {
  printf("RTSP client disconnected\n");
  if (server_) {
    server_->OnConnectionClosed();
  }
}

void RtspConnection::HandleRequest(const RtspRequest& request) {
  const std::string method = request.method;
  const int cseq = request.cseq;
  std::vector<std::string> headers;
  std::string body;

  if (method == "OPTIONS") {
    headers.push_back("Public: OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN");
    Send(BuildResponse(200, "OK", cseq, headers, body));
    return;
  }

  if (method == "DESCRIBE") {
    body = BuildSdp();
    headers.push_back("Content-Type: application/sdp");
    headers.push_back("Content-Base: " + request.uri);
    Send(BuildResponse(200, "OK", cseq, headers, body));
    return;
  }

  if (method == "SETUP") {
    auto transport_it = request.headers.find("transport");
    uint16_t client_rtp = 0;
    uint16_t client_rtcp = 0;
    if (transport_it == request.headers.end() ||
        !ParseClientPorts(transport_it->second, &client_rtp, &client_rtcp)) {
      Send(BuildResponse(461, "Unsupported Transport", cseq, headers, body));
      return;
    }
    std::string transport =
        "Transport: RTP/AVP;unicast;client_port=" + std::to_string(client_rtp) +
        "-" + std::to_string(client_rtcp) +
        ";server_port=" + std::to_string(kServerRtpPort) + "-" +
        std::to_string(kServerRtcpPort);
    headers.push_back(transport);
    headers.push_back("Session: " + session_id_);
    Send(BuildResponse(200, "OK", cseq, headers, body));
    return;
  }

  if (method == "PLAY") {
    headers.push_back("Session: " + session_id_);
    headers.push_back("RTP-Info: url=" + request.uri + ";seq=0;rtptime=0");
    Send(BuildResponse(200, "OK", cseq, headers, body));
    return;
  }

  if (method == "TEARDOWN") {
    headers.push_back("Session: " + session_id_);
    Send(BuildResponse(200, "OK", cseq, headers, body));
    return;
  }

  Send(BuildResponse(501, "Not Implemented", cseq, headers, body));
}

}  // namespace

int main(int argc, char* argv[]) {
  uint16_t port = kDefaultRtspPort;
  if (argc > 1) {
    port = static_cast<uint16_t>(std::atoi(argv[1]));
  }
  RtspServer server(port);
  if (!server.Start()) {
    return 1;
  }
  server.Run();
  return 0;
}
