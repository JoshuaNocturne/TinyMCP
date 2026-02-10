#include "Session.h"
#include "../Message/BasicMessage.h"
#include "../Message/Notification.h"
#include "../Message/Request.h"
#include "../Message/Response.h"
#include "../Public/Logger.h"
#include "../Public/PublicDef.h"
#include "../Task/BasicTask.h"

#include <algorithm>
#include <json/json.h>
#include <memory>

namespace MCP {
CMCPSession CMCPSession::s_Instance;

CMCPSession& CMCPSession::GetInstance() {
  return s_Instance;
}

int CMCPSession::Ready() {
  LOG_INFO("Session ready started");

  if (!m_spTransport) {
    LOG_ERROR("Transport not set");
    return ERRNO_INTERNAL_ERROR;
  }

  int iErrCode = ERRNO_OK;
  iErrCode = m_spTransport->Connect();
  if (ERRNO_OK != iErrCode) {
    LOG_ERROR("Transport connection failed, error: {}", iErrCode);
    return iErrCode;
  }

  LOG_INFO("Session ready completed");
  return iErrCode;
}

int CMCPSession::Run() {
  LOG_INFO("Session message loop started");

  if (!m_spTransport) {
    LOG_ERROR("Transport not set");
    return ERRNO_INTERNAL_ERROR;
  }

  int iErrCode = ERRNO_OK;

  while (true) {
    std::string strIncomingMsg;
    iErrCode = m_spTransport->Read(strIncomingMsg);
    if (ERRNO_OK == iErrCode) {
      std::shared_ptr<MCP::Message> spMsg;
      iErrCode = ParseMessage(strIncomingMsg, spMsg);
      iErrCode = ProcessMessage(iErrCode, spMsg);
    } else {
      LOG_WARNING("Message loop exiting, error: {}", iErrCode);
      break;
    }
  }

  LOG_INFO("Session message loop ended");
  return iErrCode;
}

int CMCPSession::Terminate() {
  LOG_INFO("Session terminating");

  StopAsyncTaskThread();
  if (m_upTaskThread && m_upTaskThread->joinable()) {
    m_upTaskThread->join();
  }

  if (!m_spTransport) {
    LOG_ERROR("Transport not set");
    return ERRNO_INTERNAL_ERROR;
  }

  int iErrCode = ERRNO_OK;
  iErrCode = m_spTransport->Disconnect();
  if (ERRNO_OK != iErrCode) {
    LOG_ERROR("Transport disconnection failed, error: {}", iErrCode);
    return iErrCode;
  }

  LOG_INFO("Session terminated");
  return iErrCode;
}

int CMCPSession::ProcessMessage(
  int iErrCode, const std::shared_ptr<MCP::Message>& spMsg) {
  if (!spMsg || !spMsg->IsValid()) {
    LOG_ERROR("Invalid message");
    return ERRNO_INTERNAL_ERROR;
  }

  switch (spMsg->eMessageCategory) {
  case MessageCategory_Request: {
    return ProcessRequest(iErrCode, spMsg);
  } break;
  case MessageCategory_Response: {
    return ProcessResponse(iErrCode, spMsg);
  } break;
  case MessageCategory_Notification: {
    return ProcessNotification(iErrCode, spMsg);
  } break;
  default:
    LOG_ERROR("Unknown message category: {}",
      static_cast<int>(spMsg->eMessageCategory));
    break;
  }

  return ERRNO_INTERNAL_ERROR;
}

int CMCPSession::ProcessRequest(
  int iErrCode, const std::shared_ptr<MCP::Message>& spMsg) {
  std::shared_ptr<MCP::Request> spRequest{ nullptr };
  std::string strMessage;

  if (ERRNO_OK != iErrCode) {
    goto PROC_END;
  }
  if (!spMsg || !spMsg->IsValid()) {
    LOG_ERROR("Invalid request message");
    iErrCode = ERRNO_INTERNAL_ERROR;
    goto PROC_END;
  }
  spRequest = std::dynamic_pointer_cast<MCP::Request>(spMsg);
  if (!spRequest) {
    LOG_ERROR("Cannot cast to Request type");
    iErrCode = ERRNO_INTERNAL_ERROR;
    goto PROC_END;
  }
  m_hashMessage[MessageCategory_Request].push_back(spMsg);

  LOG_INFO("Processing request: {}", spRequest->strMethod);

  switch (spRequest->eMessageType) {
  case MessageType_InitializeRequest: {
    if (CMCPSession::SessionState_Original !=
        CMCPSession::GetInstance().GetSessionState()) {
      LOG_ERROR("InitializeRequest in invalid state");
      strMessage = ERROR_MESSAGE_INVALID_REQUEST;
      iErrCode = ERRNO_INVALID_REQUEST;
      goto PROC_END;
    }

    auto spTask = std::make_shared<ProcessInitializeRequest>(spRequest);
    if (!spTask) {
      LOG_ERROR("Failed to create InitializeRequest task");
      iErrCode = ERRNO_INTERNAL_ERROR;
      goto PROC_END;
    }
    iErrCode = spTask->Execute();
    if (ERRNO_OK != iErrCode) {
      LOG_ERROR("InitializeRequest failed, error: {}", iErrCode);
      goto PROC_END;
    }

    iErrCode = SwitchState(SessionState_Initializing);

  } break;
  case MessageType_PingRequest: {
    auto spTask = std::make_shared<ProcessPingRequest>(spRequest);
    if (!spTask) {
      LOG_ERROR("Failed to create PingRequest task");
      iErrCode = ERRNO_INTERNAL_ERROR;
      goto PROC_END;
    }
    iErrCode = spTask->Execute();
    if (ERRNO_OK != iErrCode) {
      LOG_ERROR("PingRequest failed, error: {}", iErrCode);
      goto PROC_END;
    }
    break;
  }
  case MessageType_ListToolsRequest: {
    if (CMCPSession::SessionState_Initialized !=
        CMCPSession::GetInstance().GetSessionState()) {
      LOG_ERROR("ListToolsRequest in invalid state");
      strMessage = ERROR_MESSAGE_INVALID_REQUEST;
      iErrCode = ERRNO_INVALID_REQUEST;
      goto PROC_END;
    }

    auto spTask = std::make_shared<ProcessListToolsRequest>(spRequest);
    if (!spTask) {
      LOG_ERROR("Failed to create ListToolsRequest task");
      iErrCode = ERRNO_INTERNAL_ERROR;
      goto PROC_END;
    }

    iErrCode = spTask->Execute();
    if (ERRNO_OK != iErrCode) {
      LOG_ERROR("ListToolsRequest failed, error: {}", iErrCode);
    }

  } break;
  case MessageType_CallToolRequest: {
    if (CMCPSession::SessionState_Initialized !=
        CMCPSession::GetInstance().GetSessionState()) {
      LOG_ERROR("CallToolRequest in invalid state");
      iErrCode = ERRNO_INVALID_REQUEST;
      goto PROC_END;
    }

    auto spCallToolRequest =
      std::dynamic_pointer_cast<MCP::CallToolRequest>(spRequest);
    if (!spCallToolRequest) {
      LOG_ERROR("Cannot cast to CallToolRequest");
      iErrCode = ERRNO_INTERNAL_ERROR;
      goto PROC_END;
    }

    LOG_INFO("Calling tool: {}", spCallToolRequest->strName);

    auto spProcessCallToolRequest =
      CMCPSession::GetInstance().GetServerCallToolsTask(
        spCallToolRequest->strName);
    if (!spProcessCallToolRequest) {
      LOG_ERROR("Tool not found: {}", spCallToolRequest->strName);
      strMessage = ERROR_MESSAGE_INVALID_PARAMS;
      iErrCode = ERRNO_INVALID_PARAMS;
      goto PROC_END;
    }
    auto spNewTask = spProcessCallToolRequest->Clone();
    if (!spNewTask) {
      LOG_ERROR("Failed to clone task");
      iErrCode = ERRNO_INTERNAL_ERROR;
      goto PROC_END;
    }
    auto spNewProcessCallToolRequest =
      std::dynamic_pointer_cast<MCP::ProcessCallToolRequest>(spNewTask);
    if (!spNewProcessCallToolRequest) {
      LOG_ERROR("Cannot cast to ProcessCallToolRequest");
      iErrCode = ERRNO_INTERNAL_ERROR;
      goto PROC_END;
    }
    spNewProcessCallToolRequest->SetRequest(spRequest);
    iErrCode = CommitAsyncTask(spNewProcessCallToolRequest);
    if (ERRNO_OK != iErrCode) {
      LOG_ERROR("Failed to commit async task, error: {}", iErrCode);
    }

  } break;
  default:
    break;
  }

PROC_END: {
  auto spTask = std::make_shared<ProcessErrorRequest>(spRequest);
  if (spTask) {
    spTask->SetErrorCode(iErrCode);
    spTask->SetErrorMessage(strMessage);
    spTask->Execute();
  }
}

  return iErrCode;
}

int CMCPSession::ProcessResponse(
  int iErrCode, const std::shared_ptr<MCP::Message>& spMsg) {
  if (!spMsg || !spMsg->IsValid()) {
    LOG_ERROR("Invalid response message");
    return ERRNO_INTERNAL_ERROR;
  }
  auto spResponse = std::dynamic_pointer_cast<MCP::Response>(spMsg);
  if (!spResponse) {
    LOG_ERROR("Cannot cast to Response type");
    return ERRNO_INTERNAL_ERROR;
  }
  m_hashMessage[MessageCategory_Response].push_back(spMsg);

  return ERRNO_INTERNAL_ERROR;
}

int CMCPSession::ProcessNotification(
  int iErrCode, const std::shared_ptr<MCP::Message>& spMsg) {
  if (!spMsg || !spMsg->IsValid()) {
    LOG_ERROR("Invalid notification message");
    return ERRNO_INTERNAL_ERROR;
  }
  auto spNotification = std::dynamic_pointer_cast<MCP::Notification>(spMsg);
  if (!spNotification) {
    LOG_ERROR("Cannot cast to Notification type");
    return ERRNO_INTERNAL_ERROR;
  }
  m_hashMessage[MessageCategory_Notification].push_back(spMsg);

  if (ERRNO_OK != iErrCode) {
    return ERRNO_OK;
  }

  LOG_INFO("Notification: {}", spNotification->strMethod);

  switch (spNotification->eMessageType) {
  case MessageType_InitializedNotification: {
    int iErrCode = SwitchState(SessionState_Initialized);
    if (ERRNO_OK == iErrCode) {
      return StartAsyncTaskThread();
    }
    LOG_ERROR("State switch failed, error: {}", iErrCode);
    return iErrCode;

  } break;
  case MessageType_CancelledNotification: {
    auto spCancelledNotification =
      std::dynamic_pointer_cast<MCP::CancelledNotification>(spNotification);
    if (spCancelledNotification && spCancelledNotification->IsValid()) {
      return CancelAsyncTask(spCancelledNotification->requestId);
    }
    LOG_ERROR("Invalid CancelledNotification");

  } break;
  default:
    break;
  }

  return ERRNO_INTERNAL_ERROR;
}

int CMCPSession::ParseMessage(
  const std::string& strMsg, std::shared_ptr<MCP::Message>& spMsg) {
  if (strMsg.empty()) {
    LOG_ERROR("Empty message");
    return ERRNO_PARSE_ERROR;
  }

  LOG_TRACE("Parsing message: {}", strMsg);
  Json::Reader reader;
  Json::Value jVal;
  if (!reader.parse(strMsg, jVal) || !jVal.isObject()) {
    LOG_ERROR("JSON parsing failed");
    return ERRNO_PARSE_ERROR;
  }

  MessageCategory eCategory{ MessageCategory_Unknown };
  if (jVal.isMember(MSG_KEY_ID)) {
    if (jVal.isMember(MSG_KEY_METHOD)) {
      eCategory = MessageCategory_Request;
    } else {
      eCategory = MessageCategory_Response;
    }
  } else {
    if (jVal.isMember(MSG_KEY_METHOD)) {
      eCategory = MessageCategory_Notification;
    }
  }

  if (MessageCategory_Unknown == eCategory) {
    LOG_ERROR("Unknown message category");
    return ERRNO_PARSE_ERROR;
  }

  switch (eCategory) {
  case MessageCategory_Request: {
    return ParseRequest(strMsg, spMsg);
  } break;
  case MessageCategory_Response: {
    return ParseResponse(strMsg, spMsg);
  } break;
  case MessageCategory_Notification: {
    return ParseNotification(strMsg, spMsg);
  } break;
  default:
    break;
  }

  return ERRNO_INTERNAL_ERROR;
}

int CMCPSession::ParseRequest(
  const std::string& strMsg, std::shared_ptr<MCP::Message>& spMsg) {
  auto spRequest = std::make_shared<MCP::Request>(MessageType_Unknown, false);
  if (!spRequest)
    return ERRNO_PARSE_ERROR;

  int iErrCode = ERRNO_OK;
  iErrCode = spRequest->Deserialize(strMsg);
  if (ERRNO_OK != iErrCode)
    return iErrCode;
  if (!spRequest->IsValid())
    return ERRNO_INVALID_REQUEST;

  if (spRequest->strMethod.compare(METHOD_INITIALIZE) == 0) {
    auto spInitializeRequest = std::make_shared<MCP::InitializeRequest>(true);
    if (!spInitializeRequest)
      return ERRNO_PARSE_ERROR;

    iErrCode = spInitializeRequest->Deserialize(strMsg);
    if (ERRNO_OK != iErrCode)
      return ERRNO_INVALID_REQUEST;

    spMsg = spInitializeRequest;

    return ERRNO_OK;
  } else if (spRequest->strMethod.compare(METHOD_PING) == 0) {
    auto spPingRequest = std::make_shared<MCP::PingRequest>(true);
    if (!spPingRequest)
      return ERRNO_PARSE_ERROR;

    iErrCode = spPingRequest->Deserialize(strMsg);
    if (ERRNO_OK != iErrCode)
      return ERRNO_INVALID_REQUEST;

    spMsg = spPingRequest;

    return ERRNO_OK;
  } else if (spRequest->strMethod.compare(METHOD_TOOLS_LIST) == 0) {
    auto spListToolsRequest = std::make_shared<MCP::ListToolsRequest>(true);
    if (!spListToolsRequest)
      return ERRNO_PARSE_ERROR;

    iErrCode = spListToolsRequest->Deserialize(strMsg);
    if (ERRNO_OK != iErrCode)
      return ERRNO_INVALID_REQUEST;

    spMsg = spListToolsRequest;

    return ERRNO_OK;
  } else if (spRequest->strMethod.compare(METHOD_TOOLS_CALL) == 0) {
    auto spCallToolRequest = std::make_shared<MCP::CallToolRequest>(true);
    if (!spCallToolRequest)
      return ERRNO_PARSE_ERROR;

    iErrCode = spCallToolRequest->Deserialize(strMsg);
    if (ERRNO_OK != iErrCode)
      return ERRNO_INVALID_REQUEST;

    spMsg = spCallToolRequest;

    return ERRNO_OK;
  }

  return ERRNO_INTERNAL_ERROR;
}

int CMCPSession::ParseResponse(
  const std::string& strMsg, std::shared_ptr<MCP::Message>& spMsg) {
  return ERRNO_INTERNAL_ERROR;
}

int CMCPSession::ParseNotification(
  const std::string& strMsg, std::shared_ptr<MCP::Message>& spMsg) {
  auto spNotification =
    std::make_shared<MCP::Notification>(MessageType_Unknown, false);
  if (!spNotification)
    return ERRNO_PARSE_ERROR;

  int iErrCode = ERRNO_OK;
  iErrCode = spNotification->Deserialize(strMsg);
  if (ERRNO_OK != iErrCode)
    return iErrCode;
  if (!spNotification->IsValid())
    return ERRNO_INVALID_NOTIFICATION;

  if (spNotification->strMethod.compare(METHOD_NOTIFICATION_INITIALIZED) == 0) {
    auto spInitializedNotification =
      std::make_shared<MCP::InitializedNotification>(true);
    if (!spInitializedNotification)
      return ERRNO_PARSE_ERROR;

    iErrCode = spInitializedNotification->Deserialize(strMsg);
    if (ERRNO_OK != iErrCode)
      return ERRNO_INVALID_NOTIFICATION;

    spMsg = spInitializedNotification;

    return ERRNO_OK;
  } else if (spNotification->strMethod.compare(METHOD_NOTIFICATION_CANCELLED) ==
             0) {
    auto spCancelledNotification =
      std::make_shared<MCP::CancelledNotification>(true);
    if (!spCancelledNotification)
      return ERRNO_PARSE_ERROR;

    iErrCode = spCancelledNotification->Deserialize(strMsg);
    if (ERRNO_OK != iErrCode)
      return ERRNO_INVALID_NOTIFICATION;

    spMsg = spCancelledNotification;

    return ERRNO_OK;
  }

  return ERRNO_INTERNAL_ERROR;
}

void CMCPSession::SetTransport(
  const std::shared_ptr<CMCPTransport>& spTransport) {
  m_spTransport = spTransport;
}

void CMCPSession::SetServerInfo(const MCP::Implementation& impl) {
  m_serverInfo = impl;
}

void CMCPSession::SetServerCapabilities(
  const MCP::ServerCapabilities& capabilities) {
  m_capabilities = capabilities;
}

void CMCPSession::SetServerToolsPagination(bool bPagination) {
  m_bToolsPagination = bPagination;
}

void CMCPSession::SetServerTools(const std::vector<MCP::Tool>& tools) {
  m_tools = tools;
}

void CMCPSession::SetServerCallToolsTasks(const std::unordered_map<std::string,
  std::shared_ptr<MCP::ProcessCallToolRequest>>& hashCallToolsTasks) {
  m_hashCallToolsTasks = hashCallToolsTasks;
}

MCP::Implementation CMCPSession::GetServerInfo() const {
  return m_serverInfo;
}

MCP::ServerCapabilities CMCPSession::GetServerCapabilities() const {
  return m_capabilities;
}

bool CMCPSession::GetServerToolsPagination() const {
  return m_bToolsPagination;
}

std::vector<MCP::Tool> CMCPSession::GetServerTools() const {
  return m_tools;
}

std::shared_ptr<CMCPTransport> CMCPSession::GetTransport() const {
  return m_spTransport;
}

int CMCPSession::SwitchState(SessionState eState) {
  LOG_INFO("State transition: {} -> {}", static_cast<int>(m_eSessionState),
    static_cast<int>(eState));

  if (SessionState_Initializing == eState) {
    if (SessionState_Original != m_eSessionState) {
      LOG_ERROR("Invalid state transition to Initializing from {}",
        static_cast<int>(m_eSessionState));
      return ERRNO_INTERNAL_ERROR;
    }
  }

  if (SessionState_Initialized == eState) {
    if (SessionState_Initializing != m_eSessionState) {
      LOG_ERROR("Invalid state transition to Initialized from {}",
        static_cast<int>(m_eSessionState));
      return ERRNO_INTERNAL_ERROR;
    }
  }

  m_eSessionState = eState;

  return ERRNO_OK;
}

CMCPSession::SessionState CMCPSession::GetSessionState() const {
  return m_eSessionState;
}

std::shared_ptr<MCP::ProcessRequest> CMCPSession::GetServerCallToolsTask(
  const std::string& strToolName) {
  if (m_hashCallToolsTasks.count(strToolName) > 0)
    return m_hashCallToolsTasks[strToolName];

  return nullptr;
}

int CMCPSession::CommitAsyncTask(const std::shared_ptr<MCP::CMCPTask>& spTask) {
  if (!spTask) {
    LOG_ERROR("Task is null");
    return ERRNO_INTERNAL_ERROR;
  }

  if (m_bRunAsyncTask) {
    std::unique_lock<std::mutex> _lock(m_mtxAsyncThread);

    if (!m_bRunAsyncTask) {
      _lock.unlock();
      return ERRNO_OK;
    }

    m_deqAsyncTasks.push_back(spTask);

    _lock.unlock();
    m_cvAsyncThread.notify_one();
  }

  return ERRNO_OK;
}

int CMCPSession::CancelAsyncTask(const MCP::RequestId& requestId) {
  if (!requestId.IsValid()) {
    LOG_ERROR("Invalid RequestId");
    return ERRNO_INVALID_NOTIFICATION;
  }

  if (m_bRunAsyncTask) {
    std::unique_lock<std::mutex> _lock(m_mtxAsyncThread);

    if (!m_bRunAsyncTask) {
      _lock.unlock();
      return ERRNO_OK;
    }

    m_vecCancelledTaskIds.push_back(requestId);

    _lock.unlock();
    m_cvAsyncThread.notify_one();
  }

  return ERRNO_OK;
}

int CMCPSession::StartAsyncTaskThread() {
  LOG_INFO("Async task thread starting");

  m_upTaskThread =
    std::make_unique<std::thread>(&CMCPSession::AsyncThreadProc, this);
  if (!m_upTaskThread) {
    LOG_ERROR("Failed to create thread");
    return ERRNO_INTERNAL_ERROR;
  }

  return ERRNO_OK;
}

int CMCPSession::StopAsyncTaskThread() {
  std::unique_lock<std::mutex> _lock(m_mtxAsyncThread);
  m_bRunAsyncTask = false;
  _lock.unlock();

  m_cvAsyncThread.notify_all();

  return ERRNO_OK;
}

int CMCPSession::AsyncThreadProc() {
  LOG_INFO("Async task thread started");

  while (m_bRunAsyncTask) {
    std::unique_lock<std::mutex> _lock(m_mtxAsyncThread);

    // Wait for tasks
    m_cvAsyncThread.wait(_lock, [this]() {
      return !m_deqAsyncTasks.empty() || !m_vecCancelledTaskIds.empty() ||
             !m_bRunAsyncTask;
    });

    // Break the loop and clean up tasks
    if (!m_bRunAsyncTask) {
      _lock.unlock();

      std::for_each(m_vecAsyncTasksCache.begin(), m_vecAsyncTasksCache.end(),
        [this](auto& spTask) {
          if (spTask) {
            spTask->Cancel();
          }
        });

      break;
    }

    // Process new pending tasks
    std::vector<std::shared_ptr<MCP::CMCPTask>> vecTasks;
    while (!m_deqAsyncTasks.empty()) {
      auto spTask = m_deqAsyncTasks.front();
      vecTasks.push_back(spTask);
      m_deqAsyncTasks.pop_front();
    }

    //  Process task cancellation requests
    std::for_each(m_vecAsyncTasksCache.begin(), m_vecAsyncTasksCache.end(),
      [this](auto& spTask) {
        if (spTask) {
          auto spProcessRequestTask =
            std::dynamic_pointer_cast<MCP::ProcessRequest>(spTask);
          if (spProcessRequestTask) {
            auto spRequest = spProcessRequestTask->GetRequest();
            if (spRequest) {
              auto itrFound = std::find_if(m_vecCancelledTaskIds.begin(),
                m_vecCancelledTaskIds.end(), [&spRequest](auto& requestId) {
                  if (requestId.IsEqual(spRequest->requestId))
                    return true;
                  return false;
                });
              if (itrFound != m_vecCancelledTaskIds.end())
                spTask->Cancel();
            }
          }
        }
      });
    m_vecCancelledTaskIds.clear();
    _lock.unlock();

    // Clean up completed tasks
    m_vecAsyncTasksCache.erase(
      std::remove_if(m_vecAsyncTasksCache.begin(), m_vecAsyncTasksCache.end(),
        [](auto spTask) {
          if (!spTask)
            return true;
          if (spTask->IsFinished() || spTask->IsCancelled())
            return true;
          return false;
        }),
      m_vecAsyncTasksCache.end());

    // Cache new tasks
    for (auto& spTask : vecTasks) {
      if (spTask) {
        int iResult = spTask->Execute();
        if (ERRNO_OK == iResult) {
          m_vecAsyncTasksCache.push_back(spTask);
        } else {
          LOG_ERROR("Task execution failed, error: {}", iResult);
        }
      }
    }
  }

  LOG_INFO("Async task thread terminated");
  return ERRNO_OK;
}
}  // namespace MCP
