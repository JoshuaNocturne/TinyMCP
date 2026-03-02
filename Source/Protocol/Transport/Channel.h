#pragma once

#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace MCP {

class IChannel {
public:
  virtual ~IChannel() = default;

  virtual int Read(std::string& data) = 0;
  virtual int Write(const std::string& data) = 0;
  virtual int Close() = 0;
  virtual bool IsActive() = 0;
  virtual int SetAttribute(
    const std::string& key, const std::string& value) = 0;
  virtual std::string GetAttribute(const std::string& key) = 0;
};

class CStdioChannel : public IChannel {
public:
  CStdioChannel();
  ~CStdioChannel() override = default;

  int Read(std::string& data) override;
  int Write(const std::string& data) override;
  int Close() override;
  bool IsActive() override;
  int SetAttribute(const std::string& key, const std::string& value) override;
  std::string GetAttribute(const std::string& key) override;

private:
  bool m_active;
};

struct ConnectionContext {
  std::string request_body;
  std::string response_body;
  std::map<std::string, std::string> request_header;
  std::map<std::string, std::string> response_header;
  bool has_request = false;
  bool has_response = false;
  std::mutex mutex;
  std::condition_variable response_cond;
};

class CHttpChannel : public IChannel {
public:
  explicit CHttpChannel(std::shared_ptr<ConnectionContext> context);
  ~CHttpChannel() override = default;

  int Read(std::string& data) override;
  int Write(const std::string& data) override;
  int Close() override;
  bool IsActive() override;
  int SetAttribute(const std::string& key, const std::string& value) override;
  std::string GetAttribute(const std::string& key) override;

private:
  std::shared_ptr<ConnectionContext> m_context;
};

}  // namespace MCP

