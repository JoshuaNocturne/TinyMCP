#pragma once

#include "BasicMessage.h"
#include <json/json.h>

namespace MCP {
struct Notification : public MCP::Message {
public:
  Notification(MessageType eMsgType, bool bNeedIdentity)
    : Message(eMsgType, MessageCategory_Notification, bNeedIdentity) {}

  std::string strMethod;

  bool IsValid() const override;
  int DoSerialize(Json::Value& jMsg) const override;
  int DoDeserialize(const Json::Value& jMsg) override;
};

struct InitializedNotification : public MCP::Notification {
public:
  InitializedNotification(bool bNeedIdentity)
    : Notification(MessageType_InitializedNotification, bNeedIdentity) {}

  bool IsValid() const override;
};

struct CancelledNotification : public MCP::Notification {
public:
  CancelledNotification(bool bNeedIdentity)
    : Notification(MessageType_CancelledNotification, bNeedIdentity) {}

  MCP::RequestId requestId;

  bool IsValid() const override;
  int DoSerialize(Json::Value& jMsg) const override;
  int DoDeserialize(const Json::Value& jMsg) override;
};

struct ProgressNotification : public MCP::Notification {
public:
  ProgressNotification(bool bNeedIdentity)
    : Notification(MessageType_ProgressNotification, bNeedIdentity) {}

  MCP::ProgressToken progressToken;
  int iProgress{ -1 };
  int iTotal{ -1 };

  bool IsValid() const override;
  int DoSerialize(Json::Value& jMsg) const override;
  int DoDeserialize(const Json::Value& jMsg) override;
};
}  // namespace MCP
