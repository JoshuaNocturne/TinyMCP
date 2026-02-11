#pragma once

#include <memory>
#include <string>

#include <spdlog/common.h>
#include <spdlog/spdlog.h>

namespace MCP {

enum class LogLevel { Trace, Debug, Info, Warning, Error, Critical };

class Logger {
public:
  static Logger& Instance();

  void Initialize(const std::string& logFileName = "",
    LogLevel level = LogLevel::Info, size_t maxFileSize = 1024 * 1024 * 10,
    size_t maxFiles = 3);

  void SetLevel(LogLevel level);

  template <typename... Args>
  void Trace(const spdlog::source_loc& loc, const char* format, Args&&... args);

  template <typename... Args>
  void Debug(const spdlog::source_loc& loc, const char* format, Args&&... args);

  template <typename... Args>
  void Info(const spdlog::source_loc& loc, const char* format, Args&&... args);

  template <typename... Args>
  void Warning(
    const spdlog::source_loc& loc, const char* format, Args&&... args);

  template <typename... Args>
  void Error(const spdlog::source_loc& loc, const char* format, Args&&... args);

  template <typename... Args>
  void Critical(
    const spdlog::source_loc& loc, const char* format, Args&&... args);

  void Flush();

private:
  Logger();
  ~Logger();
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  std::shared_ptr<spdlog::logger> m_logger;
  bool m_initialized;
};

template <typename... Args>
void Logger::Trace(
  const spdlog::source_loc& loc, const char* format, Args&&... args) {
  if (m_logger) {
    m_logger->log(
      loc, spdlog::level::trace, format, std::forward<Args>(args)...);
  }
}

template <typename... Args>
void Logger::Debug(
  const spdlog::source_loc& loc, const char* format, Args&&... args) {
  if (m_logger) {
    m_logger->log(
      loc, spdlog::level::debug, format, std::forward<Args>(args)...);
  }
}

template <typename... Args>
void Logger::Info(
  const spdlog::source_loc& loc, const char* format, Args&&... args) {
  if (m_logger) {
    m_logger->log(
      loc, spdlog::level::info, format, std::forward<Args>(args)...);
  }
}

template <typename... Args>
void Logger::Warning(
  const spdlog::source_loc& loc, const char* format, Args&&... args) {
  if (m_logger) {
    m_logger->log(
      loc, spdlog::level::warn, format, std::forward<Args>(args)...);
  }
}

template <typename... Args>
void Logger::Error(
  const spdlog::source_loc& loc, const char* format, Args&&... args) {
  if (m_logger) {
    m_logger->log(loc, spdlog::level::err, format, std::forward<Args>(args)...);
  }
}

template <typename... Args>
void Logger::Critical(
  const spdlog::source_loc& loc, const char* format, Args&&... args) {
  if (m_logger) {
    m_logger->log(
      loc, spdlog::level::critical, format, std::forward<Args>(args)...);
  }
}

}  // namespace MCP

#define LOG_TRACE(...)           \
  MCP::Logger::Instance().Trace( \
    spdlog::source_loc{ __FILE__, __LINE__, SPDLOG_FUNCTION }, __VA_ARGS__)
#define LOG_DEBUG(...)           \
  MCP::Logger::Instance().Debug( \
    spdlog::source_loc{ __FILE__, __LINE__, SPDLOG_FUNCTION }, __VA_ARGS__)
#define LOG_INFO(...)           \
  MCP::Logger::Instance().Info( \
    spdlog::source_loc{ __FILE__, __LINE__, SPDLOG_FUNCTION }, __VA_ARGS__)
#define LOG_WARNING(...)           \
  MCP::Logger::Instance().Warning( \
    spdlog::source_loc{ __FILE__, __LINE__, SPDLOG_FUNCTION }, __VA_ARGS__)
#define LOG_ERROR(...)           \
  MCP::Logger::Instance().Error( \
    spdlog::source_loc{ __FILE__, __LINE__, SPDLOG_FUNCTION }, __VA_ARGS__)
#define LOG_CRITICAL(...)           \
  MCP::Logger::Instance().Critical( \
    spdlog::source_loc{ __FILE__, __LINE__, SPDLOG_FUNCTION }, __VA_ARGS__)

