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
  // 默认创建一个控制台日志记录器
  try {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::trace);

    m_logger = std::make_shared<spdlog::logger>("tinymcp", console_sink);
    m_logger->set_level(spdlog::level::trace);
    m_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] [%s:%#] %v");

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

    // 添加控制台输出
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::trace);
    sinks.push_back(console_sink);

    // 如果指定了日志文件名，添加文件输出
    if (!logFileName.empty()) {
      auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        logFileName, maxFileSize, maxFiles);
      file_sink->set_level(spdlog::level::trace);
      sinks.push_back(file_sink);
    }

    // 创建logger
    m_logger =
      std::make_shared<spdlog::logger>("tinymcp", sinks.begin(), sinks.end());

    // 设置日志级别
    SetLevel(level);

    // 设置日志格式：[时间] [级别] [线程] [文件:行号] 消息
    m_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] [%s:%#] %v");

    // 设置立即刷新
    m_logger->flush_on(spdlog::level::err);

    // 注册logger
    spdlog::register_logger(m_logger);

    m_initialized = true;

    Info(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, "Logger initialized successfully");
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
