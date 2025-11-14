
#if 0
#else
#include <iostream>
#include <thread>
#include <memory>
#include "websocket_service.h"
#include "http_service.h"
#include "CNCConsole.h"
#include "modbus_business.h"
#include "tcp_client.h"
#include "control_layer.h"


//#define Manual_Control 1

int main(int argc, char *argv[])
{
  // --- Create a SINGLE, SHARED set of controller instances ---
#ifdef Manual_Control

#else
  auto cnc_controller = std::make_shared<CNCConsole>("/dev/ttyUSB0");
  cnc_controller->start();
#endif
  auto modbus_controller = std::make_shared<ModbusBusiness>();
  modbus_controller->start();

  // --- Create and run services in separate threads, both using the shared controllers ---
  std::thread http_thread([&]() {
#ifdef Manual_Control
      printf("Manual_Control ....\n");
      HttpServer http_server(44444, nullptr, modbus_controller);
      http_server.start2(); // For http-server-specific initializations
#else
      HttpServer http_server(44444, cnc_controller, modbus_controller);
      http_server.start(); // For http-server-specific initializations
#endif
      http_server.run();
  });
#ifdef Manual_Control
  std::thread ws_thread([&]() {
      WebSocketService ws_server(44446, nullptr, modbus_controller);
      ws_server.run();
  });
#else
  std::thread ws_thread([&]() {
      WebSocketService ws_server(44446, cnc_controller, modbus_controller);
      ws_server.run();
  });
#endif
  auto control_layer_ = std::make_shared<ControlLayer>();
  control_layer_->start();
  control_layer_->setCncController(cnc_controller);	
  std::cout << "HTTP and WebSocket services are running in parallel." << std::endl;
  std::cout << "Press Ctrl+C to exit." << std::endl;
  
  // Keep the main thread alive
  http_thread.join();
  ws_thread.join();

  return 0;
}



#endif

