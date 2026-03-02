#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "Channel.h"
#include "httplib.h"

namespace MCP {

class CMCPTransport {
public:
  virtual ~CMCPTransport() = default;

  virtual int Start() = 0;
  virtual int Stop() = 0;
  virtual std::shared_ptr<IChannel> AcceptChannel() = 0;
};

class CStdioTransport : public CMCPTransport {
public:
  CStdioTransport();
  ~CStdioTransport() override = default;

  int Start() override;
  int Stop() override;
  std::shared_ptr<IChannel> AcceptChannel() override;

private:
  bool m_channelCreated;
};

struct ConnectionContext;

class CMCPSession;

class CHttpTransport : public CMCPTransport {
public:
  CHttpTransport(const std::string& host = "0.0.0.0", int port = 8080);
  ~CHttpTransport() override;

  int Start() override;
  int Stop() override;
  std::shared_ptr<IChannel> AcceptChannel() override;

private:
  std::string m_strHost;
  int m_nPort;
  std::unique_ptr<httplib::Server> m_server;
  std::unique_ptr<std::thread> m_serverThread;
  bool m_running;
  std::mutex m_mutex;
  std::condition_variable m_channelCond;
  std::queue<std::shared_ptr<IChannel>> m_pendingChannels;
};

}  // namespace MCP

