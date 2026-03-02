#include "Transport.h"

#include <chrono>
#include <future>
#include <thread>

#include "../Public/Logger.h"
#include "../Public/PublicDef.h"

#include "Channel.h"

namespace MCP {

CStdioTransport::CStdioTransport() : m_channelCreated(false) {}

int CStdioTransport::Start() {
  LOG_INFO("CStdioTransport::Start: Stdio transport started");
  return ERRNO_OK;
}

int CStdioTransport::Stop() {
  LOG_INFO("CStdioTransport::Stop: Stdio transport stopped");
  return ERRNO_OK;
}

std::shared_ptr<IChannel> CStdioTransport::AcceptChannel() {
  if (!m_channelCreated) {
    m_channelCreated = true;
    LOG_INFO("CStdioTransport::AcceptChannel: Creating stdio channel");
    return std::make_shared<CStdioChannel>();
  }
  return nullptr;
}

CHttpTransport::CHttpTransport(const std::string& host, int port)
  : m_strHost(host), m_nPort(port), m_running(false) {}

CHttpTransport::~CHttpTransport() {
  Stop();
}

int CHttpTransport::Start() {
  LOG_INFO(
    "CHttpTransport::Start: Starting HTTP server {}:{}", m_strHost, m_nPort);

  try {
    m_server = std::make_unique<httplib::Server>();

    m_server->Post("/", [this](
                          const httplib::Request& req, httplib::Response& res) {
      LOG_INFO(
        "CHttpTransport::Start: POST request received, body {}", req.body);

      auto context = std::make_shared<ConnectionContext>();
      context->request_body = req.body;
      context->has_request = true;

      for (const auto& header : req.headers) {
        context->request_header[header.first] = header.second;
      }

      auto channel = std::make_shared<CHttpChannel>(context);
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingChannels.push(channel);
        m_channelCond.notify_one();
      }

      {
        std::unique_lock<std::mutex> lock(context->mutex);
        context->response_cond.wait(lock,
          [context, this]() { return context->has_response || !m_running; });

        if (!m_running) {
          res.set_content("{\"error\":\"Server stopped\"}", "application/json");
          res.status = 503;
        } else {
          LOG_INFO(
            "CHttpTransport::Start: Response body: {}", context->response_body);

          for (const auto& header : context->response_header) {
            res.set_header(header.first, header.second);
            LOG_TRACE("CHttpTransport::Start: Setting header: {} = {}",
              header.first, header.second);
          }

          if (context->response_header.find("Content-Type") ==
              context->response_header.end()) {
            res.set_content(context->response_body, "application/json");
          } else {
            res.set_content(context->response_body, "");
          }
          res.status = 200;
        }
      }
    });

    m_running = true;
    m_serverThread = std::make_unique<std::thread>(
      [this]() { m_server->listen(m_strHost, m_nPort); });

    for (int i = 0; i < 50; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      if (m_server && m_server->is_running()) {
        LOG_INFO("CHttpTransport::Start: HTTP server started successfully");
        return ERRNO_OK;
      }
    }

    LOG_ERROR("CHttpTransport::Start: HTTP server startup timeout");
    m_running = false;
    return ERRNO_INTERNAL_ERROR;
  } catch (const std::exception& e) {
    LOG_ERROR("CHttpTransport::Start: Exception: {}", e.what());
    m_running = false;
    return ERRNO_INTERNAL_ERROR;
  }
}

int CHttpTransport::Stop() {
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_running) {
      return ERRNO_OK;
    }
    m_running = false;
    m_channelCond.notify_all();
  }

  LOG_INFO("CHttpTransport::Stop: Stopping HTTP server");

  if (m_server) {
    try {
      m_server->stop();
      LOG_INFO("CHttpTransport::Stop: HTTP server stop signal sent");
    } catch (const std::exception& e) {
      LOG_ERROR(
        "CHttpTransport::Stop: Exception during server stop: {}", e.what());
    }
  }

  if (m_serverThread && m_serverThread->joinable()) {
    try {
      auto future = std::async(std::launch::async, [this]() {
        if (m_serverThread && m_serverThread->joinable()) {
          m_serverThread->join();
        }
      });

      if (future.wait_for(std::chrono::seconds(5)) ==
          std::future_status::timeout) {
        LOG_ERROR("CHttpTransport::Stop: Server thread join timeout");
      } else {
        LOG_INFO("CHttpTransport::Stop: Server thread joined successfully");
      }
    } catch (const std::exception& e) {
      LOG_ERROR(
        "CHttpTransport::Stop: Exception during thread join: {}", e.what());
    }
  }

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    while (!m_pendingChannels.empty()) {
      auto channel = m_pendingChannels.front();
      m_pendingChannels.pop();
      try {
        if (channel) {
          channel->Close();
        }
      } catch (...) {
      }
    }
  }

  try {
    m_server.reset();
    m_serverThread.reset();
  } catch (const std::exception& e) {
    LOG_ERROR(
      "CHttpTransport::Stop: Exception during resource cleanup: {}", e.what());
  }

  LOG_INFO("CHttpTransport::Stop: HTTP server stopped successfully");
  return ERRNO_OK;
}

std::shared_ptr<IChannel> CHttpTransport::AcceptChannel() {
  std::unique_lock<std::mutex> lock(m_mutex);

  while (m_running) {
    if (m_channelCond.wait_for(lock, std::chrono::milliseconds(100),
          [this]() { return !m_pendingChannels.empty() || !m_running; })) {
      if (!m_running) {
        return nullptr;
      }

      if (!m_pendingChannels.empty()) {
        auto channel = m_pendingChannels.front();
        m_pendingChannels.pop();
        LOG_INFO("CHttpTransport::AcceptChannel: Channel accepted");
        return channel;
      }
    }
  }

  return nullptr;
}

}  // namespace MCP

