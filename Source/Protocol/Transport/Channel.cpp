#include "Channel.h"

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define STDIN_FILENO _fileno(stdin)
#else
#include <poll.h>
#include <unistd.h>
#endif

#include <chrono>
#include <iostream>

#include "../Public/Logger.h"
#include "../Public/PublicDef.h"

namespace MCP {

CStdioChannel::CStdioChannel() : m_active(true) {}

int CStdioChannel::Read(std::string& data) {
  while (m_active) {
#ifdef _WIN32
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdin == INVALID_HANDLE_VALUE) {
      LOG_ERROR("CStdioChannel::Read: Failed to get stdin handle");
      return ERRNO_INTERNAL_INPUT_ERROR;
    }

    DWORD waitResult = WaitForSingleObject(hStdin, 50);

    if (waitResult == WAIT_FAILED) {
      LOG_ERROR("CStdioChannel::Read: Wait for input failed");
      return ERRNO_INTERNAL_INPUT_ERROR;
    }

    if (!m_active) {
      return ERRNO_INTERNAL_INPUT_TERMINATE;
    }

    if (waitResult == WAIT_OBJECT_0) {
      DWORD numEvents = 0;
      if (!GetNumberOfConsoleInputEvents(hStdin, &numEvents) ||
          numEvents == 0) {
        continue;
      }

      if (std::getline(std::cin, data)) {
        return ERRNO_OK;
      } else {
        if (std::cin.eof()) {
          return ERRNO_INTERNAL_INPUT_TERMINATE;
        } else {
          LOG_ERROR("CStdioChannel::Read: Failed to read input");
          return ERRNO_INTERNAL_INPUT_ERROR;
        }
      }
    }
#else
    struct pollfd fds[1];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    int ret = poll(fds, 1, 50);

    if (ret < 0) {
      LOG_ERROR("CStdioChannel::Read: poll call failed, error code: {}", errno);
      return ERRNO_INTERNAL_INPUT_ERROR;
    }

    if (!m_active) {
      return ERRNO_INTERNAL_INPUT_TERMINATE;
    }

    if (ret > 0 && (fds[0].revents & POLLIN)) {
      if (std::getline(std::cin, data)) {
        return ERRNO_OK;
      } else {
        if (std::cin.eof()) {
          return ERRNO_INTERNAL_INPUT_TERMINATE;
        } else {
          LOG_ERROR("CStdioChannel::Read: Failed to read input");
          return ERRNO_INTERNAL_INPUT_ERROR;
        }
      }
    }
#endif
  }

  return ERRNO_INTERNAL_INPUT_TERMINATE;
}

int CStdioChannel::Write(const std::string& data) {
  if (!m_active) {
    return ERRNO_INTERNAL_ERROR;
  }

  std::cout << data << std::endl;
  return ERRNO_OK;
}

int CStdioChannel::Close() {
  m_active = false;
  return ERRNO_OK;
}

bool CStdioChannel::IsActive() {
  return m_active;
}

int CStdioChannel::SetAttribute(
  const std::string& key, const std::string& value) {
  return ERRNO_OK;
}

std::string CStdioChannel::GetAttribute(const std::string& key) {
  return "";
}

CHttpChannel::CHttpChannel(std::shared_ptr<ConnectionContext> context)
  : m_context(context) {}

int CHttpChannel::Read(std::string& data) {
  if (!m_context) {
    LOG_ERROR("CHttpChannel::Read: Invalid context");
    return ERRNO_INTERNAL_ERROR;
  }

  std::unique_lock<std::mutex> lock(m_context->mutex);

  if (!m_context->has_request) {
    LOG_ERROR("CHttpChannel::Read: No request available");
    return ERRNO_INTERNAL_ERROR;
  }

  data = m_context->request_body;
  m_context->request_body.clear();
  m_context->has_request = false;

  LOG_TRACE("CHttpChannel::Read: Data received, size: {}", data.size());
  return ERRNO_OK;
}

int CHttpChannel::Write(const std::string& data) {
  if (!m_context) {
    LOG_ERROR("CHttpChannel::Write: Invalid context");
    return ERRNO_INTERNAL_ERROR;
  }

  std::lock_guard<std::mutex> lock(m_context->mutex);

  m_context->response_body = data;
  m_context->has_response = true;
  m_context->response_cond.notify_one();

  LOG_TRACE("CHttpChannel::Write: Data sent, size: {}", data.size());
  return ERRNO_OK;
}

int CHttpChannel::Close() {
  if (!m_context) {
    return ERRNO_INTERNAL_ERROR;
  }

  std::lock_guard<std::mutex> lock(m_context->mutex);
  m_context->response_cond.notify_one();

  return ERRNO_OK;
}

bool CHttpChannel::IsActive() {
  if (!m_context) {
    return false;
  }

  return true;
}

int CHttpChannel::SetAttribute(
  const std::string& key, const std::string& value) {
  if (!m_context) {
    LOG_ERROR("CHttpChannel::SetAttribute: Invalid context");
    return ERRNO_INTERNAL_ERROR;
  }

  std::lock_guard<std::mutex> lock(m_context->mutex);
  m_context->response_header[key] = value;

  LOG_TRACE("CHttpChannel::SetAttribute: Set header: {} = {}", key, value);
  return ERRNO_OK;
}

std::string CHttpChannel::GetAttribute(const std::string& key) {
  if (!m_context) {
    LOG_ERROR("CHttpChannel::GetAttribute: Invalid context");
    return "";
  }

  std::lock_guard<std::mutex> lock(m_context->mutex);
  auto it = m_context->request_header.find(key);
  if (it != m_context->request_header.end()) {
    LOG_TRACE(
      "CHttpChannel::GetAttribute: Get header: {} = {}", key, it->second);
    return it->second;
  }

  LOG_TRACE("CHttpChannel::GetAttribute: Header not found: {}", key);
  return "";
}

}  // namespace MCP

