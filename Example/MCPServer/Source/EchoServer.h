#pragma once

#include <Entity/Server.h>

namespace Implementation {

enum class TransportType { kStdio, kHttp };

// A server class for business operations is declared.
// It is used to customize unique logic, but it must be a singleton.
class CEchoServer : public MCP::CMCPServer<CEchoServer> {
public:
  static constexpr const char* SERVER_NAME = "echo_server";
  static constexpr const char* SERVER_VERSION = "1.0.0.1";

  // Set the transport type before calling Initialize()
  void SetTransportType(TransportType type) {
    m_transportType = type;
  }

  // Set HTTP transport parameters (host and port)
  void SetHttpTransportParams(const std::string& host, int port) {
    m_httpHost = host;
    m_httpPort = port;
  }

  // This is the initialization method, which is used to configure the Server.
  // The Server can be started only after the configuration is successful.
  int Initialize() override;

private:
  friend class MCP::CMCPServer<CEchoServer>;
  CEchoServer() = default;
  static CEchoServer s_Instance;

  TransportType m_transportType = TransportType::kStdio;
  std::string m_httpHost = "0.0.0.0";
  int m_httpPort = 8080;
};

}  // namespace Implementation
