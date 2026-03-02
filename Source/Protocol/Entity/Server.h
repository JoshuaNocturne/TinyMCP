#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "../Public/Logger.h"
#include "../Public/PublicDef.h"
#include "../Session/Session.h"
#include "../Transport/Transport.h"

namespace MCP {

template <class T>
class CMCPServer {
public:
  CMCPServer(const CMCPServer&) = delete;
  CMCPServer& operator=(const CMCPServer&) = delete;
  static CMCPServer& GetInstance() {
    return T::s_Instance;
  }

  void SetServerInfo(const MCP::Implementation& serverInfo) {
    m_serverInfo = serverInfo;
  }

  void SetTransport(const std::shared_ptr<MCP::CMCPTransport>& spTransport) {
    m_spTransport = spTransport;
  }

  void RegisterServerToolsCapabilities(const MCP::Tools& tools) {
    m_capabilities.tools = tools;
  }

  void RegisterServerResourcesCapabilities(const MCP::Resources& resources) {
    m_capabilities.resources = resources;
  }

  void RegisterServerPromptsCapabilities(const MCP::Prompts& prompts) {
    m_capabilities.prompts = prompts;
  }

  void RegisterServerTools(
    const std::vector<MCP::Tool>& tools, bool bPagination) {
    m_bToolsPagination = bPagination;
    m_tools = tools;
  }

  void RegisterToolsTasks(const std::string& strToolName,
    std::shared_ptr<MCP::ProcessCallToolRequest> spTask) {
    m_hashCallToolsTasks[strToolName] = spTask;
  }

  virtual int Initialize() = 0;

  int Start() {
    if (!m_spTransport)
      m_spTransport = std::make_shared<CStdioTransport>();

    int iErrCode = m_spTransport->Start();
    if (ERRNO_OK != iErrCode) {
      return iErrCode;
    }

    m_bRunning = true;
    m_mainThread = std::make_unique<std::thread>([this]() { ServerLoop(); });
    if (m_mainThread && m_mainThread->joinable())
      m_mainThread->join();

    return ERRNO_OK;
  }

  int Stop() {
    m_bRunning = false;

    if (m_spTransport) {
      m_spTransport->Stop();
    }

    auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < timeout) {
      {
        std::lock_guard<std::mutex> lock(m_threadsMutex);
        if (m_activeThreads.empty()) {
          break;
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    {
      std::lock_guard<std::mutex> lock(m_threadsMutex);
      if (!m_activeThreads.empty()) {
        LOG_WARNING("Stop: {} threads still active during shutdown",
          m_activeThreads.size());
      }
      m_activeThreads.clear();
      m_hashSessions.clear();
    }

    return ERRNO_OK;
  }

private:
  void ServerLoop() {
    while (m_bRunning.load()) {
      auto spChannel = m_spTransport->AcceptChannel();
      if (!spChannel || !m_bRunning.load()) {
        break;
      }

      std::shared_ptr<CMCPSession> spSession;
      auto sessionId = spChannel->GetAttribute(HEADER_SESSION_ID);
      if (!sessionId.empty()) {
        std::lock_guard<std::mutex> lock(m_threadsMutex);
        auto iter = m_hashSessions.find(sessionId);
        if (iter != m_hashSessions.end()) {
          spSession = iter->second;
          spSession->SetChannel(spChannel);
        }
      }

      if (!spSession) {
        spSession = std::make_shared<CMCPSession>(spChannel);
        if (!spSession) {
          continue;
        }
        spSession->SetServerInfo(m_serverInfo);
        spSession->SetServerCapabilities(m_capabilities);
        spSession->SetServerToolsPagination(m_bToolsPagination);
        spSession->SetServerTools(m_tools);
        spSession->SetServerCallToolsTasks(m_hashCallToolsTasks);
      }

      auto spThread = std::make_shared<std::thread>([this, spSession]() {
        spSession->Run();
        std::lock_guard<std::mutex> lock(m_threadsMutex);
        if (spSession->GetSessionState() != CMCPSession::SessionState_Shut) {
          auto& sessionId = spSession->GetSessionId();
          if (sessionId.empty())
            LOG_ERROR("Session::Run: Session ID not set");
          else {
            this->m_hashSessions.emplace(sessionId, spSession);
          }
        } else
          this->m_hashSessions.erase(spSession->GetSessionId());

        m_activeThreads.erase(std::this_thread::get_id());
      });

      if (spThread) {
        std::lock_guard<std::mutex> lock(m_threadsMutex);
        m_activeThreads[spThread->get_id()] = spThread;
        spThread->detach();
      }
    }
  }

protected:
  CMCPServer() = default;
  ~CMCPServer() = default;

  std::shared_ptr<MCP::CMCPTransport> m_spTransport;
  MCP::Implementation m_serverInfo;
  MCP::ServerCapabilities m_capabilities;
  bool m_bToolsPagination{ false };
  std::vector<MCP::Tool> m_tools;
  std::unordered_map<std::string, std::shared_ptr<MCP::ProcessCallToolRequest>>
    m_hashCallToolsTasks;
  std::atomic<bool> m_bRunning{ false };
  std::unordered_map<std::string, std::shared_ptr<CMCPSession>> m_hashSessions;

  std::unique_ptr<std::thread> m_mainThread;
  mutable std::mutex m_threadsMutex;
  std::unordered_map<std::thread::id, std::shared_ptr<std::thread>>
    m_activeThreads;
};

}  // namespace MCP

