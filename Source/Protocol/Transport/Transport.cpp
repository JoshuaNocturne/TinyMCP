#include "Transport.h"

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
#include <thread>

#include "../Public/Logger.h"

namespace MCP {

CStdioTransport::CStdioTransport() : m_running(true) {}

int CStdioTransport::Connect() {
  m_running = true;
  return ERRNO_OK;
}

int CStdioTransport::Disconnect() {
  m_running = false;
  return ERRNO_OK;
}

int CStdioTransport::Stop() {
  m_running = false;
  return ERRNO_OK;
}

int CStdioTransport::Read(std::string& strOut) {
  const std::lock_guard<std::recursive_mutex> _lock(m_mtxStdin);

  while (m_running) {
#ifdef _WIN32
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdin == INVALID_HANDLE_VALUE) {
      LOG_ERROR("CStdioTransport::Read: Failed to get stdin handle");
      return ERRNO_INTERNAL_INPUT_ERROR;
    }

    // Timeout set to 50ms to balance responsiveness and CPU usage
    DWORD waitResult = WaitForSingleObject(hStdin, 50);

    if (waitResult == WAIT_FAILED) {
      LOG_ERROR("CStdioTransport::Read: Wait for input failed");
      return ERRNO_INTERNAL_INPUT_ERROR;
    }

    if (!m_running) {
      return ERRNO_INTERNAL_INPUT_TERMINATE;
    }

    if (waitResult == WAIT_OBJECT_0) {
      DWORD numEvents = 0;
      if (!GetNumberOfConsoleInputEvents(hStdin, &numEvents) ||
          numEvents == 0) {
        continue;
      }

      if (std::getline(std::cin, strOut)) {
        return ERRNO_OK;
      } else {
        if (std::cin.eof()) {
          return ERRNO_INTERNAL_INPUT_TERMINATE;
        } else {
          LOG_ERROR("CStdioTransport::Read: Failed to read input");
          return ERRNO_INTERNAL_INPUT_ERROR;
        }
      }
    }
#else
    struct pollfd fds[1];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    // Timeout set to 50ms to balance responsiveness and CPU usage
    int ret = poll(fds, 1, 50);

    if (ret < 0) {
      LOG_ERROR(
        "CStdioTransport::Read: poll call failed, error code: {}", errno);
      return ERRNO_INTERNAL_INPUT_ERROR;
    }

    if (!m_running) {
      return ERRNO_INTERNAL_INPUT_TERMINATE;
    }

    if (ret > 0 && (fds[0].revents & POLLIN)) {
      if (std::getline(std::cin, strOut)) {
        return ERRNO_OK;
      } else {
        if (std::cin.eof()) {
          return ERRNO_INTERNAL_INPUT_TERMINATE;
        } else {
          LOG_ERROR("CStdioTransport::Read: Failed to read input");
          return ERRNO_INTERNAL_INPUT_ERROR;
        }
      }
    }
#endif
  }

  return ERRNO_INTERNAL_INPUT_TERMINATE;
}

int CStdioTransport::Write(const std::string& strIn) {
  const std::lock_guard<std::recursive_mutex> _lock(m_mtxStdout);
  std::cout << strIn << std::endl;

  return ERRNO_OK;
}

int CStdioTransport::Error(const std::string& strIn) {
  return ERRNO_OK;
}

CHttpTransport::CHttpTransport(const std::string& host, int port)
  : m_strHost(host), m_nPort(port), m_server(nullptr), m_running(false) {}

int CHttpTransport::Stop() {
  m_running = false;
  m_requestCond.notify_all();
  return ERRNO_OK;
}

CHttpTransport::~CHttpTransport() {
  Disconnect();
}

int CHttpTransport::Connect() {
  LOG_INFO(
    "CHttpTransport::Connect: Starting HTTP server {}:{}", m_strHost, m_nPort);
  try {
    m_server = std::make_unique<httplib::Server>();

    // Set up POST handler
    m_server->Post("/", [this](
                          const httplib::Request& req, httplib::Response& res) {
      LOG_INFO("CHttpTransport::Connect: POST request received");
      // Create new connection context
      auto connContext = std::make_shared<ConnectionContext>();
      ConnectionId connId;

      // Assign ID and add to connection map under lock protection
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        connId = m_nextConnectionId++;
        m_connections[connId] = connContext;

        // Set request data under connection lock protection
        std::lock_guard<std::mutex> connLock(connContext->mutex);
        connContext->request_body = req.body;
        connContext->has_request = true;
        m_requestCond.notify_one();  // Notify Read function of new request
      }

      {  // Wait for response
        std::unique_lock<std::mutex> lock(connContext->mutex);
        // Wait until response is ready or server stops
        connContext->response_cond.wait(lock, [this, connContext]() {
          return !m_running || connContext->has_response;
        });

        if (!m_running) {
          res.set_content("{\"error\":\"Server stopped\"}", "application/json");
          res.status = 503;
        } else {
          res.set_content(connContext->response_body, "application/json");
          res.status = 200;
        }
      }

      // Clean up connection externally (without holding connection mutex)
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_connections.erase(connId);
      }
    });

    // Start server thread
    m_running = true;
    std::thread server_thread(
      [this]() { m_server->listen(m_strHost, m_nPort); });
    server_thread.detach();

    // Wait for server startup and verify
    for (int i = 0; i < 50; ++i) {  // Wait up to 500ms
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      if (m_server && m_server->is_running()) {
        LOG_INFO(
          "CHttpTransport::Connect: HTTP server started successfully {}:{}",
          m_strHost, m_nPort);
        return ERRNO_OK;
      }
    }

    // Startup timeout or failure
    LOG_ERROR("CHttpTransport::Connect: HTTP server startup timeout {}:{}",
      m_strHost, m_nPort);
    m_running = false;
    return ERRNO_INTERNAL_ERROR;
  } catch (const std::exception& e) {
    LOG_ERROR(
      "CHttpTransport::Connect: HTTP server startup exception: {}", e.what());
    m_running = false;
    return ERRNO_INTERNAL_ERROR;
  }
}

int CHttpTransport::Disconnect() {
  try {
    if (m_running && m_server) {
      m_running = false;

      // Notify all active connections that server is stopping
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& pair : m_connections) {
          std::lock_guard<std::mutex> connLock(pair.second->mutex);
          pair.second->response_cond.notify_one();
        }
        m_connections.clear();
      }

      m_server->stop();
      m_server.reset();
    }
    return ERRNO_OK;
  } catch (const std::exception& e) {
    LOG_ERROR(
      "CHttpTransport::Disconnect: HTTP server stop exception: {}", e.what());
    return ERRNO_INTERNAL_ERROR;
  }
}

int CHttpTransport::Read(std::string& strOut) {
  try {
    if (!m_running || !m_server) {
      LOG_ERROR("CHttpTransport::Read: Server not running or not initialized");
      return ERRNO_INTERNAL_ERROR;
    }

    // Use wait with timeout to respond to stop requests
    std::unique_lock<std::mutex> lock(m_mutex);
    while (m_running) {
      // Wait up to 100ms, then recheck conditions
      bool hasData =
        m_requestCond.wait_for(lock, std::chrono::milliseconds(100), [this]() {
          if (!m_running)
            return true;
          // Check if any connection has a request (needs lock protection)
          for (const auto& pair : m_connections) {
            std::lock_guard<std::mutex> connLock(pair.second->mutex);
            if (pair.second->has_request) {
              return true;
            }
          }
          return false;
        });

      // If stop signal received, return immediately
      if (!m_running) {
        return ERRNO_INTERNAL_INPUT_TERMINATE;
      }

      // If data arrived, break loop to continue processing
      if (hasData) {
        break;
      }
    }

    // Check running state again
    if (!m_running) {
      return ERRNO_INTERNAL_INPUT_TERMINATE;
    }

    // Find first connection with a request (use shared_ptr for safety)
    std::shared_ptr<ConnectionContext> selectedContext;
    ConnectionId selectedConnId = 0;
    for (const auto& pair : m_connections) {
      std::lock_guard<std::mutex> connLock(pair.second->mutex);
      if (pair.second->has_request) {
        selectedConnId = pair.first;
        selectedContext = pair.second;
        break;
      }
    }

    if (!selectedContext) {
      LOG_ERROR("CHttpTransport::Read: No valid request connection found");
      return ERRNO_INTERNAL_ERROR;
    }

    // Save currently processing connection
    m_currentConnectionId = selectedConnId;
    m_currentConnection = selectedContext;

    // Get request body (needs connection lock protection)
    {
      std::lock_guard<std::mutex> connLock(selectedContext->mutex);
      strOut = selectedContext->request_body;
      selectedContext->request_body.clear();
      selectedContext->has_request = false;
    }

    return ERRNO_OK;
  } catch (const std::exception& e) {
    LOG_ERROR("CHttpTransport::Read: Read exception: {}", e.what());
    return ERRNO_INTERNAL_ERROR;
  }
}

int CHttpTransport::Write(const std::string& strIn) {
  try {
    if (!m_running || !m_server) {
      LOG_ERROR("CHttpTransport::Write: Server not running or not initialized");
      return ERRNO_INTERNAL_ERROR;
    }

    // Check if there is a currently processing connection
    if (!m_currentConnection) {
      LOG_ERROR("CHttpTransport::Write: No active connection");
      return ERRNO_INTERNAL_ERROR;
    }

    // Set response and notify corresponding connection's Post handler
    {
      std::lock_guard<std::mutex> connLock(m_currentConnection->mutex);
      m_currentConnection->response_body = strIn;
      m_currentConnection->has_response = true;
      m_currentConnection->response_cond
        .notify_one();  // Notify corresponding connection's Post handler
    }

    // Clear current connection info
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_currentConnectionId = 0;
      m_currentConnection.reset();
    }

    return ERRNO_OK;
  } catch (const std::exception& e) {
    LOG_ERROR("CHttpTransport::Write: Write exception: {}", e.what());
    return ERRNO_INTERNAL_ERROR;
  }
}

int CHttpTransport::Error(const std::string& strIn) {
  LOG_ERROR("CHttpTransport::Error: {}", strIn);
  return ERRNO_OK;
}

}  // namespace MCP
