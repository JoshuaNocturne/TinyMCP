#pragma once

#include "../Message/Message.h"
#include <memory>

namespace MCP {
class CMCPTask {
public:
  virtual ~CMCPTask() {}
  virtual std::shared_ptr<CMCPTask> Clone() const = 0;
  virtual bool IsValid() const = 0;
  virtual bool IsFinished() const = 0;
  virtual bool IsCancelled() const = 0;
  virtual int Execute() = 0;
  virtual int Cancel() = 0;
};
}  // namespace MCP
