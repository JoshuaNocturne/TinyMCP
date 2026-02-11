#include "BasicTask.h"
#include "../Message/Notification.h"
#include "../Public/Logger.h"
#include "../Session/Session.h"

namespace MCP {
////////////////////////////////////////////////////////////////////////////////////////
// ProcessRequest
bool ProcessRequest::IsValid() const {
  if (!m_spRequest)
    return false;

  return m_spRequest->IsValid();
}

bool ProcessRequest::IsFinished() const {
  return true;
}

bool ProcessRequest::IsCancelled() const {
  return false;
}

int ProcessRequest::Execute() {
  return ERRNO_OK;
}

int ProcessRequest::Cancel() {
  return ERRNO_OK;
}

void ProcessRequest::SetRequest(
  const std::shared_ptr<MCP::Request>& spRequest) {
  m_spRequest = spRequest;
}

std::shared_ptr<MCP::Request> ProcessRequest::GetRequest() const {
  return m_spRequest;
}

////////////////////////////////////////////////////////////////////////////////////////
// ProcessErrorRequest
std::shared_ptr<CMCPTask> ProcessErrorRequest::Clone() const {
  return nullptr;
}

int ProcessErrorRequest::Execute() {
  if (!IsValid()) {
    LOG_ERROR("Invalid error request");
    return ERRNO_INTERNAL_ERROR;
  }

  auto spErrorResponse = std::make_shared<ErrorResponse>(true);
  if (!spErrorResponse) {
    LOG_ERROR("Failed to create error response");
    return ERRNO_INTERNAL_ERROR;
  }

  if (m_strMessage.empty()) {
    switch (m_iCode) {
    case ERRNO_PARSE_ERROR: {
      m_strMessage = ERROR_MESSAGE_PARSE_ERROR;
    } break;
    case ERRNO_INVALID_REQUEST: {
      m_strMessage = ERROR_MESSAGE_INVALID_REQUEST;
    } break;
    case ERRNO_METHOD_NOT_FOUND: {
      m_strMessage = ERROR_MESSAGE_METHOD_NOT_FOUND;
    } break;
    case ERRNO_INVALID_PARAMS: {
      m_strMessage = ERROR_MESSAGE_INVALID_PARAMS;
    } break;
    case ERRNO_INTERNAL_ERROR: {
      m_strMessage = ERROR_MESSAGE_INTERNAL_ERROR;
    } break;
    default:
      break;
    }
  }
  if (m_spRequest)
    spErrorResponse->requestId = m_spRequest->requestId;
  spErrorResponse->iCode = m_iCode;
  spErrorResponse->strMesage = m_strMessage;

  LOG_INFO(
    "Sending error response: code={}, message={}", m_iCode, m_strMessage);

  std::string strResponse;
  if (ERRNO_OK != spErrorResponse->Serialize(strResponse)) {
    LOG_ERROR("Failed to serialize error response");
    return ERRNO_INTERNAL_ERROR;
  }
  auto spTransport = CMCPSession::GetInstance().GetTransport();
  if (!spTransport) {
    LOG_ERROR("Transport not available");
    return ERRNO_INTERNAL_ERROR;
  }
  if (ERRNO_OK != spTransport->Write(strResponse)) {
    LOG_ERROR("Failed to write error response");
    return ERRNO_INTERNAL_ERROR;
  }

  return ERRNO_OK;
}

void ProcessErrorRequest::SetErrorCode(int iCode) {
  m_iCode = iCode;
}

void ProcessErrorRequest::SetErrorMessage(const std::string& strMessage) {
  m_strMessage = strMessage;
}

////////////////////////////////////////////////////////////////////////////////////////
// ProcessInitializeRequest
std::shared_ptr<CMCPTask> ProcessInitializeRequest::Clone() const {
  return nullptr;
}

int ProcessInitializeRequest::Execute() {
  if (!IsValid()) {
    LOG_ERROR("Invalid initialize request");
    return ERRNO_INTERNAL_ERROR;
  }

  LOG_INFO("Processing initialize request");

  auto spInitializeResult = std::make_shared<InitializeResult>(true);
  if (!spInitializeResult) {
    LOG_ERROR("Failed to create initialize result");
    return ERRNO_INTERNAL_ERROR;
  }
  spInitializeResult->requestId = m_spRequest->requestId;
  spInitializeResult->strProtocolVersion = PROTOCOL_VER;
  spInitializeResult->capabilities =
    CMCPSession::GetInstance().GetServerCapabilities();
  spInitializeResult->implServerInfo =
    CMCPSession::GetInstance().GetServerInfo();
  std::string strResponse;
  if (ERRNO_OK != spInitializeResult->Serialize(strResponse)) {
    LOG_ERROR("Failed to serialize initialize result");
    return ERRNO_INTERNAL_ERROR;
  }
  auto spTransport = CMCPSession::GetInstance().GetTransport();
  if (!spTransport) {
    LOG_ERROR("Transport not available");
    return ERRNO_INTERNAL_ERROR;
  }
  if (ERRNO_OK != spTransport->Write(strResponse)) {
    LOG_ERROR("Failed to write initialize response");
    return ERRNO_INTERNAL_ERROR;
  }

  LOG_INFO("Initialize request completed");
  return ERRNO_OK;
}

////////////////////////////////////////////////////////////////////////////////////////
// ProcessPingRequest
std::shared_ptr<CMCPTask> ProcessPingRequest::Clone() const {
  return nullptr;
}

int ProcessPingRequest::Execute() {
  if (!IsValid()) {
    LOG_ERROR("Invalid ping request");
    return ERRNO_INTERNAL_ERROR;
  }

  LOG_DEBUG("Processing ping request");

  auto spPingResult = std::make_shared<PingResult>(true);
  if (!spPingResult) {
    LOG_ERROR("Failed to create ping result");
    return ERRNO_INTERNAL_ERROR;
  }
  spPingResult->requestId = m_spRequest->requestId;

  std::string strResponse;
  if (ERRNO_OK != spPingResult->Serialize(strResponse)) {
    LOG_ERROR("Failed to serialize ping result");
    return ERRNO_INTERNAL_ERROR;
  }
  auto spTransport = CMCPSession::GetInstance().GetTransport();
  if (!spTransport) {
    LOG_ERROR("Transport not available");
    return ERRNO_INTERNAL_ERROR;
  }
  if (ERRNO_OK != spTransport->Write(strResponse)) {
    LOG_ERROR("Failed to write ping response");
    return ERRNO_INTERNAL_ERROR;
  }

  return ERRNO_OK;
}

////////////////////////////////////////////////////////////////////////////////////////
// ProcessListToolsRequest
std::shared_ptr<CMCPTask> ProcessListToolsRequest::Clone() const {
  return nullptr;
}

int ProcessListToolsRequest::Execute() {
  if (!IsValid()) {
    LOG_ERROR("Invalid list tools request");
    return ERRNO_INTERNAL_ERROR;
  }

  LOG_INFO("Processing list tools request");

  auto spListToolRequest =
    std::dynamic_pointer_cast<ListToolsRequest>(m_spRequest);
  if (!spListToolRequest) {
    LOG_ERROR("Failed to cast to ListToolsRequest");
    return ERRNO_INTERNAL_ERROR;
  }

  std::shared_ptr<ListToolsResult> spListToolsResult = nullptr;
  std::string strResponse;

  bool bPagination = CMCPSession::GetInstance().GetServerToolsPagination();
  if (bPagination) {
    if (!spListToolRequest->strCursor.empty()) {
      bool bValidCursor = true;
      unsigned nCursor = 0;
      try {
        nCursor = std::stoul(spListToolRequest->strCursor);
      } catch (const std::invalid_argument&) {
        bValidCursor = false;
      } catch (const std::out_of_range&) {
        bValidCursor = false;
      }

      auto vecServerTools = CMCPSession::GetInstance().GetServerTools();
      if (bValidCursor) {
        if (nCursor >= vecServerTools.size()) {
          bValidCursor = false;
        }
      }

      if (!bValidCursor) {
        LOG_WARNING("Invalid cursor in list tools request");
        auto spErrorResponse = std::make_shared<ErrorResponse>(true);
        if (!spErrorResponse) {
          LOG_ERROR("Failed to create error response");
          return ERRNO_INTERNAL_ERROR;
        }
        spErrorResponse->iCode = ERRNO_INVALID_PARAMS;
        spErrorResponse->strMesage = "invalid params";
        if (ERRNO_OK != spErrorResponse->Serialize(strResponse)) {
          LOG_ERROR("Failed to serialize error response");
          return ERRNO_INTERNAL_ERROR;
        }
      } else {
        LOG_DEBUG("Using pagination cursor: {}", nCursor);
        spListToolsResult = std::make_shared<ListToolsResult>(true);
        if (!spListToolsResult) {
          LOG_ERROR("Failed to create list tools result");
          return ERRNO_INTERNAL_ERROR;
        }
        spListToolsResult->requestId = spListToolRequest->requestId;
        spListToolsResult->vecTools.clear();
        spListToolsResult->vecTools.push_back(vecServerTools[nCursor]);
        if (nCursor < vecServerTools.size() - 1) {
          spListToolsResult->strNextCursor = std::to_string(nCursor + 1);
        }
      }
    } else {
      LOG_DEBUG("Starting pagination from beginning");
      spListToolsResult = std::make_shared<ListToolsResult>(true);
      if (!spListToolsResult) {
        LOG_ERROR("Failed to create list tools result");
        return ERRNO_INTERNAL_ERROR;
      }
      spListToolsResult->requestId = spListToolRequest->requestId;
      auto vecServerTools = CMCPSession::GetInstance().GetServerTools();
      spListToolsResult->vecTools.clear();
      if (vecServerTools.size() > 0)
        spListToolsResult->vecTools.push_back(vecServerTools[0]);
      if (vecServerTools.size() > 1)
        spListToolsResult->strNextCursor = std::to_string(1);
    }
  } else {
    LOG_DEBUG("Returning all tools without pagination");
    spListToolsResult = std::make_shared<ListToolsResult>(true);
    if (!spListToolsResult) {
      LOG_ERROR("Failed to create list tools result");
      return ERRNO_INTERNAL_ERROR;
    }
    spListToolsResult->requestId = spListToolRequest->requestId;
    spListToolsResult->vecTools = CMCPSession::GetInstance().GetServerTools();
  }

  if (spListToolsResult) {
    if (ERRNO_OK != spListToolsResult->Serialize(strResponse)) {
      LOG_ERROR("Failed to serialize list tools result");
      return ERRNO_INTERNAL_ERROR;
    }
  }

  if (!strResponse.empty()) {
    auto spTransport = CMCPSession::GetInstance().GetTransport();
    if (!spTransport) {
      LOG_ERROR("Transport not available");
      return ERRNO_INTERNAL_ERROR;
    }
    if (ERRNO_OK != spTransport->Write(strResponse)) {
      LOG_ERROR("Failed to write list tools response");
      return ERRNO_INTERNAL_ERROR;
    }
  }

  LOG_INFO("List tools request completed");
  return ERRNO_OK;
}

////////////////////////////////////////////////////////////////////////////////////////
// ProcessCallToolRequest
bool ProcessCallToolRequest::IsFinished() const {
  return m_bFinished;
}

bool ProcessCallToolRequest::IsCancelled() const {
  return m_bCancelled;
}

std::shared_ptr<MCP::CallToolResult> ProcessCallToolRequest::BuildResult() {
  if (!IsValid()) {
    LOG_ERROR("Invalid call tool request");
    return nullptr;
  }

  auto spCallToolResult = std::make_shared<CallToolResult>(true);
  if (!spCallToolResult) {
    LOG_ERROR("Failed to create call tool result");
    return nullptr;
  }
  spCallToolResult->requestId = m_spRequest->requestId;

  return spCallToolResult;
}

int ProcessCallToolRequest::NotifyProgress(int iProgress, int iTotal) {
  if (!m_spRequest) {
    LOG_ERROR("Request not available for progress notification");
    return ERRNO_INTERNAL_ERROR;
  }

  if (m_spRequest->progressToken.IsValid()) {
    LOG_DEBUG("Notifying progress: {}/{}", iProgress, iTotal);
    MCP::ProgressNotification progressNotification(false);
    progressNotification.strMethod = METHOD_NOTIFICATION_PROGRESS;
    progressNotification.progressToken = m_spRequest->progressToken;
    progressNotification.iProgress = iProgress;
    progressNotification.iTotal = iTotal;

    std::string strNotification;
    if (ERRNO_OK != progressNotification.Serialize(strNotification)) {
      LOG_ERROR("Failed to serialize progress notification");
      return ERRNO_INTERNAL_ERROR;
    }
    auto spTransport = CMCPSession::GetInstance().GetTransport();
    if (!spTransport) {
      LOG_ERROR("Transport not available");
      return ERRNO_INTERNAL_ERROR;
    }
    if (ERRNO_OK != spTransport->Write(strNotification)) {
      LOG_ERROR("Failed to write progress notification");
      return ERRNO_INTERNAL_ERROR;
    }
  }

  return ERRNO_OK;
}

int ProcessCallToolRequest::NotifyResult(
  std::shared_ptr<MCP::CallToolResult> spResult) {
  m_bFinished = true;

  if (!spResult) {
    LOG_ERROR("Result not available for notification");
    return ERRNO_INTERNAL_ERROR;
  }

  LOG_INFO("Notifying call tool result");

  std::string strResponse;
  if (ERRNO_OK != spResult->Serialize(strResponse)) {
    LOG_ERROR("Failed to serialize call tool result");
    return ERRNO_INTERNAL_ERROR;
  }
  auto spTransport = CMCPSession::GetInstance().GetTransport();
  if (!spTransport) {
    LOG_ERROR("Transport not available");
    return ERRNO_INTERNAL_ERROR;
  }
  if (ERRNO_OK != spTransport->Write(strResponse)) {
    LOG_ERROR("Failed to write call tool response");
    return ERRNO_INTERNAL_ERROR;
  }

  return ERRNO_OK;
}
}  // namespace MCP
