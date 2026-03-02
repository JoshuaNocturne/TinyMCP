#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../Message/BasicMessage.h"
#include "../Public/PublicDef.h"
#include "../Task/BasicTask.h"
#include "../Transport/Channel.h"

namespace MCP {
class CMCPSession {
public:
  enum SessionState {
    SessionState_Original,
    SessionState_Initializing,
    SessionState_Initialized,
    SessionState_Shutting,
    SessionState_Shut,
  };

  explicit CMCPSession(std::shared_ptr<IChannel> channel);
  ~CMCPSession();
  CMCPSession(const CMCPSession&) = delete;
  CMCPSession& operator=(const CMCPSession&) = delete;

  int Run();
  int Terminate();

  void SetServerInfo(const MCP::Implementation& impl);
  void SetServerCapabilities(const MCP::ServerCapabilities& capabilities);
  void SetServerToolsPagination(bool bPagination);
  void SetServerTools(const std::vector<MCP::Tool>& tools);
  void SetServerCallToolsTasks(const std::unordered_map<std::string,
    std::shared_ptr<MCP::ProcessCallToolRequest>>& hashCallToolsTasks);
  MCP::Implementation GetServerInfo() const;
  MCP::ServerCapabilities GetServerCapabilities() const;
  bool GetServerToolsPagination() const;
  std::vector<MCP::Tool> GetServerTools() const;
  void SetChannel(std::shared_ptr<IChannel> channel);
  std::shared_ptr<IChannel> GetChannel() const;
  SessionState GetSessionState() const;
  std::shared_ptr<MCP::ProcessRequest> GetServerCallToolsTask(
    const std::string& strToolName);
  void SetSessionId(const std::string& strSessionId);
  const std::string& GetSessionId() const;

private:
  int ParseMessage(
    const std::string& strMsg, std::shared_ptr<MCP::Message>& spMsg);
  int ParseRequest(
    const std::string& strMsg, std::shared_ptr<MCP::Message>& spMsg);
  int ParseResponse(
    const std::string& strMsg, std::shared_ptr<MCP::Message>& spMsg);
  int ParseNotification(
    const std::string& strMsg, std::shared_ptr<MCP::Message>& spMsg);
  int ProcessMessage(int iErrCode, const std::shared_ptr<MCP::Message>& spMsg);
  int ProcessRequest(int iErrCode, const std::shared_ptr<MCP::Message>& spMsg);
  int ProcessResponse(int iErrCode, const std::shared_ptr<MCP::Message>& spMsg);
  int ProcessNotification(
    int iErrCode, const std::shared_ptr<MCP::Message>& spMsg);
  int SwitchState(SessionState eState);

  int CommitAsyncTask(const std::shared_ptr<MCP::CMCPTask>& spTask);
  int CancelAsyncTask(const MCP::RequestId& requestId);
  int StartAsyncTaskThread();
  int StopAsyncTaskThread();
  int AsyncThreadProc();

  SessionState m_eSessionState{ SessionState_Original };
  std::string m_strSessionId;
  std::shared_ptr<IChannel> m_channel;

  MCP::Implementation m_serverInfo;
  MCP::ServerCapabilities m_capabilities;
  std::vector<MCP::Tool> m_tools;
  bool m_bToolsPagination{ false };

  std::unordered_map<MessageCategory,
    std::vector<std::shared_ptr<MCP::Message>>>
    m_hashMessage;
  std::unordered_map<std::string, std::shared_ptr<MCP::ProcessCallToolRequest>>
    m_hashCallToolsTasks;

  std::unique_ptr<std::thread> m_upTaskThread;
  std::atomic_bool m_bRunAsyncTask{ true };
  std::mutex m_mtxAsyncThread;
  std::condition_variable m_cvAsyncThread;
  std::deque<std::shared_ptr<MCP::CMCPTask>> m_deqAsyncTasks;
  std::vector<MCP::RequestId> m_vecCancelledTaskIds;
  std::vector<std::shared_ptr<MCP::CMCPTask>> m_vecAsyncTasksCache;
};

}  // namespace MCP

