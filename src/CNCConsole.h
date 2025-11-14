
#pragma once
#include "YX20KController.h"
#include <memory>
#include <thread>
#include <atomic>
#include <functional>

class CNCConsole {
public:
    CNCConsole(std::string dev = "/dev/ttyUSB0", int baud = 115200);
    bool start();
    void stop();
    void startPositionPolling(int interval_ms,
                              std::function<void(const YX20KController::Position&)> cb);
    void stopPositionPolling();
    std::shared_ptr<YX20KController> getYX20KController();
private:
    void inputLoop(std::shared_ptr<YX20KController> cnc);
    static bool isNumber(const std::string& s);
    std::string dev_;
    int         baud_;
    std::atomic<bool> running_{false};
    std::shared_ptr<YX20KController> cnc_ptr_;
};


