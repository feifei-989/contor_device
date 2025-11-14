
#if 0
#include <modbus/modbus.h>
#include <iostream>
#include <vector>
#include <cstring>

int main() {
    const char *server_ip = "192.168.110.122"; // 服务器IP地址
    int server_port = 502;                  // MODBUS TCP端口

    // 创建TCP上下文
    modbus_t *ctx = modbus_new_tcp(server_ip, server_port);
    if (ctx == nullptr) {
        std::cerr << "Failed to create MODBUS TCP context!" << std::endl;
        return 1;
    }


    if (modbus_set_response_timeout(ctx, 10, 500000) == -1) {
        std::cerr << "Failed to set response timeout: " << modbus_strerror(errno) << std::endl;
        modbus_free(ctx);
        return 1;
    }

    // 连接服务器
    if (modbus_connect(ctx) == -1) {
        std::cerr << "Connection to MODBUS TCP server failed: " << modbus_strerror(errno) << std::endl;
        modbus_free(ctx);
        return 1;
    }
    std::cout << "Connected to MODBUS TCP server." << std::endl;

    // 构造请求 (MBAP Header 自动处理)
    uint8_t request[] = {0x01, 0x06, 0x01, 0x4A, 0x03, 0xE8}; // 单元标识符, 功能码, 地址, 写入值
   #if 0
    uint8_t request[] = {
        0x00, 0x01, // Transaction ID
        0x00, 0x00, // Protocol ID
        0x00, 0x06, // Length
        0x01,        // Unit ID (Slave address)
        0x06,        // Function code (Write Multiple Registers)
        0x01, 0xAE, 0x01, 0x18 // Data (原 RTU 数据部分)
    };
   #endif
    // 发送请求
    if (modbus_send_raw_request(ctx, request, sizeof(request)) == -1) {
        std::cerr << "Failed to send request: " << modbus_strerror(errno) << std::endl;
        modbus_close(ctx);
        modbus_free(ctx);
        return 1;
    }

    // 接收响应
    uint8_t response[MODBUS_TCP_MAX_ADU_LENGTH];
    int response_length = modbus_receive_confirmation(ctx, response);
    if (response_length == -1) {
        std::cerr << "Failed to receive response: " << modbus_strerror(errno) << std::endl;
    } else {
        std::cout << "Response received (" << response_length << " bytes): ";
        for (int i = 0; i < response_length; i++) {
            printf("%02X ", response[i]);
        }
        std::cout << std::endl;
    }

    // 释放资源
    modbus_close(ctx);
    modbus_free(ctx);
    return 0;
}
#else
#include <iostream>
#include <thread>
#include <memory>
#include "websocket_service.h"
#include "http_service.h"
#include "CNCConsole.h"
#include "modbus_business.h"

int main(int argc, char *argv[])
{
  // --- Create a SINGLE, SHARED set of controller instances ---
  auto cnc_controller = std::make_shared<CNCConsole>("/dev/ttyUSB0");
  cnc_controller->start();

  auto modbus_controller = std::make_shared<ModbusBusiness>();
  modbus_controller->start();

  // --- Create and run services in separate threads, both using the shared controllers ---
  std::thread http_thread([&]() {
      HttpServer http_server(44444, cnc_controller, modbus_controller);
      http_server.start(); // For http-server-specific initializations
      http_server.run();
  });

  std::thread ws_thread([&]() {
      WebSocketService ws_server(44446, cnc_controller, modbus_controller);
      ws_server.run();
  });

  std::cout << "HTTP and WebSocket services are running in parallel." << std::endl;
  std::cout << "Press Ctrl+C to exit." << std::endl;

  // Keep the main thread alive
  http_thread.join();
  ws_thread.join();

  return 0;
}



#endif

