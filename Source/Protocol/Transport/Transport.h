#pragma once

#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "httplib.h"

#include "../Public/PublicDef.h"

namespace MCP {
class CMCPTransport {
public:
  virtual ~CMCPTransport() {}

  virtual int Connect() = 0;
  virtual int Disconnect() = 0;
  virtual int Read(std::string& strOut) = 0;
  virtual int Write(const std::string& strIn) = 0;
  virtual int Error(const std::string& strIn) = 0;
  virtual int Stop() = 0;
};

class CStdioTransport : public CMCPTransport {
public:
  CStdioTransport();

  int Connect() override;
  int Disconnect() override;
  int Read(std::string& strOut) override;
  int Write(const std::string& strIn) override;
  int Error(const std::string& strIn) override;
  int Stop() override;

private:
  std::recursive_mutex m_mtxStdin;
  std::recursive_mutex m_mtxStdout;
  std::recursive_mutex m_mtxStderr;
  std::atomic<bool> m_running;
};

class CHttpTransport : public CMCPTransport {
public:
  CHttpTransport(const std::string& host = "0.0.0.0", int port = 8080);
  ~CHttpTransport() override;

  int Connect() override;
  int Disconnect() override;
  int Read(std::string& strOut) override;
  int Write(const std::string& strIn) override;
  int Error(const std::string& strIn) override;
  int Stop() override;

private:
  // 连接上下文结构体，用于管理单个连接的状态
  struct ConnectionContext {
    ConnectionContext() : has_request(false), has_response(false) {}

    std::string request_body;
    std::string response_body;
    bool has_request;
    bool has_response;
    std::mutex mutex;
    std::condition_variable request_cond;
    std::condition_variable response_cond;
  };

  using ConnectionId = uint64_t;

  std::string m_strHost;
  int m_nPort;
  std::unique_ptr<httplib::Server> m_server;
  bool m_running;
  std::mutex m_mutex;  // 保护连接映射和ID生成器
  std::condition_variable m_requestCond;  // 通知有新请求
  ConnectionId m_nextConnectionId{ 1 };  // 连接ID生成器（由m_mutex保护）
  std::map<ConnectionId, std::shared_ptr<ConnectionContext>>
    m_connections;  // 活跃连接映射

  // 当前处理的连接ID
  ConnectionId m_currentConnectionId{ 0 };
  std::shared_ptr<ConnectionContext> m_currentConnection;
};

}  // namespace MCP
