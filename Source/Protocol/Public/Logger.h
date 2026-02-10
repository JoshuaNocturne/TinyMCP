#pragma once

#include <memory>
#include <string>

#include <spdlog/common.h>
#include <spdlog/spdlog.h>

namespace MCP {

/**
 * @brief 日志级别枚举
 */
enum class LogLevel {
  Trace,  // 跟踪级别
  Debug,  // 调试级别
  Info,  // 信息级别
  Warning,  // 警告级别
  Error,  // 错误级别
  Critical  // 严重错误级别
};

/**
 * @brief 日志管理类，封装spdlog的调用
 *
 * 这是一个单例类，提供统一的日志记录接口
 * 使用方法：
 *   Logger::Instance().Info("消息内容");
 *   Logger::Instance().Error("错误: {}", errorCode);
 */
class Logger {
public:
  /**
   * @brief 获取Logger单例实例
   * @return Logger的单例引用
   */
  static Logger& Instance();

  /**
   * @brief 初始化日志系统
   * @param logFileName 日志文件名（可选，不指定则只输出到控制台）
   * @param level 日志级别（默认为Info）
   * @param maxFileSize 单个日志文件最大大小（字节，默认10MB）
   * @param maxFiles 保留的日志文件数量（默认3个）
   */
  void Initialize(const std::string& logFileName = "",
    LogLevel level = LogLevel::Info, size_t maxFileSize = 1024 * 1024 * 10,
    size_t maxFiles = 3);

  /**
   * @brief 设置日志级别
   * @param level 日志级别
   */
  void SetLevel(LogLevel level);

  /**
   * @brief 跟踪级别日志
   * @param loc 源代码位置信息
   * @param format 格式化字符串
   * @param args 参数列表
   */
  template <typename... Args>
  void Trace(const spdlog::source_loc& loc, const char* format, Args&&... args);

  /**
   * @brief 调试级别日志
   * @param loc 源代码位置信息
   * @param format 格式化字符串
   * @param args 参数列表
   */
  template <typename... Args>
  void Debug(const spdlog::source_loc& loc, const char* format, Args&&... args);

  /**
   * @brief 信息级别日志
   * @param loc 源代码位置信息
   * @param format 格式化字符串
   * @param args 参数列表
   */
  template <typename... Args>
  void Info(const spdlog::source_loc& loc, const char* format, Args&&... args);

  /**
   * @brief 警告级别日志
   * @param loc 源代码位置信息
   * @param format 格式化字符串
   * @param args 参数列表
   */
  template <typename... Args>
  void Warning(
    const spdlog::source_loc& loc, const char* format, Args&&... args);

  /**
   * @brief 错误级别日志
   * @param loc 源代码位置信息
   * @param format 格式化字符串
   * @param args 参数列表
   */
  template <typename... Args>
  void Error(const spdlog::source_loc& loc, const char* format, Args&&... args);

  /**
   * @brief 严重错误级别日志
   * @param loc 源代码位置信息
   * @param format 格式化字符串
   * @param args 参数列表
   */
  template <typename... Args>
  void Critical(
    const spdlog::source_loc& loc, const char* format, Args&&... args);

  /**
   * @brief 刷新日志缓冲区
   */
  void Flush();

private:
  Logger();
  ~Logger();
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  std::shared_ptr<spdlog::logger> m_logger;
  bool m_initialized;
};

// 模板函数实现
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

