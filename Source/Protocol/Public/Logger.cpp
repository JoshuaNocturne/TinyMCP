#include "Logger.h"

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace MCP {

Logger& Logger::Instance() {
  static Logger instance;
  return instance;
}

Logger::Logger() : m_initialized(false) {
  try {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::trace);

    m_logger = std::make_shared<spdlog::logger>("tinymcp", console_sink);
    m_logger->set_level(spdlog::level::trace);
    m_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] [%s:%#] %v");

    spdlog::register_logger(m_logger);
  } catch (const spdlog::spdlog_ex& ex) {
    // 如果创建失败，使用默认logger
    m_logger = spdlog::default_logger();
  }
}

Logger::~Logger() {
  if (m_logger) {
    m_logger->flush();
  }
}

void Logger::Initialize(const std::string& logFileName, LogLevel level,
  size_t maxFileSize, size_t maxFiles) {
  if (m_initialized) {
    return;
  }

  try {
    std::vector<spdlog::sink_ptr> sinks;

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::trace);
    sinks.push_back(console_sink);

    if (!logFileName.empty()) {
      auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        logFileName, maxFileSize, maxFiles);
      file_sink->set_level(spdlog::level::trace);
      sinks.push_back(file_sink);
    }

    m_logger =
      std::make_shared<spdlog::logger>("tinymcp", sinks.begin(), sinks.end());

    SetLevel(level);

    m_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] [%s:%#] %v");
    m_logger->flush_on(spdlog::level::err);
    spdlog::register_logger(m_logger);

    m_initialized = true;

    m_logger->info("Logger initialized successfully");
  } catch (const spdlog::spdlog_ex& ex) {
    // 初始化失败时使用默认logger
    m_logger = spdlog::default_logger();
    m_logger->error("Logger initialization failed: {}", ex.what());
  }
}

void Logger::SetLevel(LogLevel level) {
  if (!m_logger) {
    return;
  }

  spdlog::level::level_enum spdLevel;
  switch (level) {
  case LogLevel::Trace:
    spdLevel = spdlog::level::trace;
    break;
  case LogLevel::Debug:
    spdLevel = spdlog::level::debug;
    break;
  case LogLevel::Info:
    spdLevel = spdlog::level::info;
    break;
  case LogLevel::Warning:
    spdLevel = spdlog::level::warn;
    break;
  case LogLevel::Error:
    spdLevel = spdlog::level::err;
    break;
  case LogLevel::Critical:
    spdLevel = spdlog::level::critical;
    break;
  default:
    spdLevel = spdlog::level::info;
    break;
  }

  m_logger->set_level(spdLevel);
}

void Logger::Flush() {
  if (m_logger) {
    m_logger->flush();
  }
}

}  // namespace MCP

