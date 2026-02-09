#include <signal.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <thread>

#include "EchoServer.h"
#include "Session/Session.h"
#include "Transport/Transport.h"

int LaunchEchoServer(Implementation::TransportType transportType,
  const std::string& host, int port) {
  // 1. Configure the Server with specified transport type.
  auto& server = Implementation::CEchoServer::GetInstance();
  auto& echoServer = static_cast<Implementation::CEchoServer&>(server);
  echoServer.SetTransportType(transportType);
  if (transportType == Implementation::TransportType::kHttp) {
    echoServer.SetHttpTransportParams(host, port);
  }
  int iErrCode = server.Initialize();
  if (MCP::ERRNO_OK == iErrCode) {
    // 2. Start the Server.
    iErrCode = server.Start();
    if (MCP::ERRNO_OK == iErrCode) {
      // 3. Stop the Server.
      server.Stop();
    }
  }

  return iErrCode;
}

void signal_handler(int signal) {
  auto& server = Implementation::CEchoServer::GetInstance();
  server.RequestStop();
}

void print_usage(const char* program_name) {
  std::cout
    << "Usage: " << program_name << " [options]\n"
    << "Options:\n"
    << "  --stdio              Use standard input/output transport (default)\n"
    << "  --http               Use HTTP transport (default: 0.0.0.0:8080)\n"
    << "  --host <address>     HTTP server host address (default: 0.0.0.0)\n"
    << "  --port <number>      HTTP server port (default: 8080)\n"
    << "  --help               Show this help message\n"
    << "\nExamples:\n"
    << "  " << program_name << " --stdio\n"
    << "  " << program_name << " --http\n"
    << "  " << program_name << " --http --host 127.0.0.1 --port 3000\n"
    << std::endl;
}

int main(int argc, char* argv[]) {
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  // Default to stdio transport
  Implementation::TransportType transportType =
    Implementation::TransportType::kStdio;
  std::string host = "0.0.0.0";
  int port = 8080;

  // Parse command line arguments
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      print_usage(argv[0]);
      return 0;
    } else if (strcmp(argv[i], "--stdio") == 0) {
      transportType = Implementation::TransportType::kStdio;
    } else if (strcmp(argv[i], "--http") == 0) {
      transportType = Implementation::TransportType::kHttp;
    } else if (strcmp(argv[i], "--host") == 0) {
      if (i + 1 < argc) {
        host = argv[++i];
      } else {
        std::cerr << "Error: --host requires an argument" << std::endl;
        print_usage(argv[0]);
        return 1;
      }
    } else if (strcmp(argv[i], "--port") == 0) {
      if (i + 1 < argc) {
        try {
          port = std::stoi(argv[++i]);
          if (port <= 0 || port > 65535) {
            std::cerr << "Error: Port must be between 1 and 65535" << std::endl;
            return 1;
          }
        } catch (const std::exception& e) {
          std::cerr << "Error: Invalid port number" << std::endl;
          print_usage(argv[0]);
          return 1;
        }
      } else {
        std::cerr << "Error: --port requires an argument" << std::endl;
        print_usage(argv[0]);
        return 1;
      }
    } else {
      std::cerr << "Error: Unknown argument '" << argv[i] << "'" << std::endl;
      print_usage(argv[0]);
      return 1;
    }
  }

  // Display transport information
  if (transportType == Implementation::TransportType::kStdio) {
    std::cout << "Using Stdio Transport" << std::endl;
  } else if (transportType == Implementation::TransportType::kHttp) {
    std::cout << "Using HTTP Transport (listening on " << host << ":" << port
              << ")" << std::endl;
  }

  return LaunchEchoServer(transportType, host, port);
}
