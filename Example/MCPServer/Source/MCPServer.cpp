#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <signal.h>
#include <memory>
#include "EchoServer.h"
#include "Session/Session.h"
#include "Transport/Transport.h"


int LaunchEchoServer()
{
    // 1. Configure the Server.
    auto& server = Implementation::CEchoServer::GetInstance();
    int iErrCode = server.Initialize();
    if (MCP::ERRNO_OK == iErrCode)
    {
        // 2. Start the Server.
        iErrCode = server.Start();
        if (MCP::ERRNO_OK == iErrCode)
        {
            // 3. Stop the Server.
            server.Stop();
        }
    }

    return iErrCode;
}


std::atomic_bool g_bStop{ false };

void signal_handler(int signal) {
  g_bStop = true;
  
  // 直接通过基类指针调用虚函数，无需类型转换
  auto spTransport = MCP::CMCPSession::GetInstance().GetTransport();
  if (spTransport) {
    spTransport->Stop();
  }
}

int main(int argc, char* argv[])
{
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    return LaunchEchoServer();
}
