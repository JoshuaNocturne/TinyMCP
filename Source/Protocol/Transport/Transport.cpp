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
      return ERRNO_INTERNAL_INPUT_ERROR;
    }

    // 超时设置为50ms，平衡响应速度和CPU占用
    DWORD waitResult = WaitForSingleObject(hStdin, 50);

    if (waitResult == WAIT_FAILED) {
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
          return ERRNO_INTERNAL_INPUT_ERROR;
        }
      }
    }
#else
    struct pollfd fds[1];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    // 超时设置为50ms，平衡响应速度和CPU占用
    int ret = poll(fds, 1, 50);

    if (ret < 0) {
      // poll出错
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
  try {
    m_server = std::make_unique<httplib::Server>();

    // 设置POST处理函数
    m_server->Post("/", [this](
                          const httplib::Request& req, httplib::Response& res) {
      // 创建新的连接上下文
      auto connContext = std::make_shared<ConnectionContext>();
      ConnectionId connId;

      // 在锁保护下分配ID并添加到连接映射
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        connId = m_nextConnectionId++;
        m_connections[connId] = connContext;

        // 在连接锁保护下设置请求数据
        std::lock_guard<std::mutex> connLock(connContext->mutex);
        connContext->request_body = req.body;
        connContext->has_request = true;
        m_requestCond.notify_one();  // 通知Read函数有新请求
      }

      {  // 等待响应
        std::unique_lock<std::mutex> lock(connContext->mutex);
        // 等待响应准备好或服务器停止
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

      // 在外部清理连接（不持有连接的mutex）
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_connections.erase(connId);
      }
    });

    // 启动服务器线程
    m_running = true;
    std::thread server_thread([this]() {
      std::cout << "HTTP server listening on " << m_strHost << ":" << m_nPort
                << std::endl;
      m_server->listen(m_strHost, m_nPort);
    });
    server_thread.detach();

    // 等待服务器启动并验证
    for (int i = 0; i < 50; ++i) {  // 最多等待500ms
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      if (m_server && m_server->is_running()) {
        std::cout << "HTTP server started successfully" << std::endl;
        return ERRNO_OK;
      }
    }

    // 启动超时或失败
    std::cerr << "HTTP server failed to start within timeout" << std::endl;
    m_running = false;
    return ERRNO_INTERNAL_ERROR;
  } catch (const std::exception& e) {
    std::cerr << "HTTP server start error: " << e.what() << std::endl;
    m_running = false;
    return ERRNO_INTERNAL_ERROR;
  }
}

int CHttpTransport::Disconnect() {
  try {
    if (m_running && m_server) {
      m_running = false;

      // 通知所有活跃连接服务器停止
      {  // 作用域：通知所有连接
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
    std::cerr << "HTTP server stop error: " << e.what() << std::endl;
    return ERRNO_INTERNAL_ERROR;
  }
}

int CHttpTransport::Read(std::string& strOut) {
  try {
    if (!m_running || !m_server) {
      return ERRNO_INTERNAL_ERROR;
    }

    // 使用带超时的等待，以便能够响应停止请求
    std::unique_lock<std::mutex> lock(m_mutex);
    while (m_running) {
      // 等待最多100ms，然后重新检查条件
      bool hasData =
        m_requestCond.wait_for(lock, std::chrono::milliseconds(100), [this]() {
          if (!m_running)
            return true;
          // 检查是否有任何连接有请求（需要锁保护）
          for (const auto& pair : m_connections) {
            std::lock_guard<std::mutex> connLock(pair.second->mutex);
            if (pair.second->has_request) {
              return true;
            }
          }
          return false;
        });

      // 如果收到停止信号，立即返回
      if (!m_running) {
        return ERRNO_INTERNAL_INPUT_TERMINATE;
      }

      // 如果有数据到达，跳出循环继续处理
      if (hasData) {
        break;
      }
    }

    // 再次检查运行状态
    if (!m_running) {
      return ERRNO_INTERNAL_INPUT_TERMINATE;
    }

    // 找到第一个有请求的连接（使用shared_ptr保证安全）
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
      return ERRNO_INTERNAL_ERROR;
    }

    // 保存当前处理的连接
    m_currentConnectionId = selectedConnId;
    m_currentConnection = selectedContext;

    // 获取请求体（需要在连接的锁保护下）
    {
      std::lock_guard<std::mutex> connLock(selectedContext->mutex);
      strOut = selectedContext->request_body;
      selectedContext->request_body.clear();
      selectedContext->has_request = false;
    }

    return ERRNO_OK;
  } catch (const std::exception& e) {
    std::cerr << "HTTP read error: " << e.what() << std::endl;
    return ERRNO_INTERNAL_ERROR;
  }
}

int CHttpTransport::Write(const std::string& strIn) {
  try {
    if (!m_running || !m_server) {
      return ERRNO_INTERNAL_ERROR;
    }

    // 检查是否有当前处理的连接
    if (!m_currentConnection) {
      return ERRNO_INTERNAL_ERROR;
    }

    // 设置响应并通知对应连接的Post处理函数
    {
      std::lock_guard<std::mutex> connLock(m_currentConnection->mutex);
      m_currentConnection->response_body = strIn;
      m_currentConnection->has_response = true;
      m_currentConnection->response_cond
        .notify_one();  // 通知对应连接的Post处理函数
    }

    // 清除当前连接信息
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_currentConnectionId = 0;
      m_currentConnection.reset();
    }

    return ERRNO_OK;
  } catch (const std::exception& e) {
    std::cerr << "HTTP write error: " << e.what() << std::endl;
    return ERRNO_INTERNAL_ERROR;
  }
}

int CHttpTransport::Error(const std::string& strIn) {
  // 处理错误，输出到标准错误
  std::cerr << "HTTP transport error: " << strIn << std::endl;
  return ERRNO_OK;
}

}  // namespace MCP
