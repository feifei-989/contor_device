

// LaserManager.h
#pragma once
#include "laser_distance.h"
#include <memory>
#include <atomic>

class LaserManager {
private:
    std::shared_ptr<LaserRangeFinder> laser_;
    bool stopFlag_;

public:
    LaserManager() : stopFlag_(false) {}

    ~LaserManager() {
        stop();
    }

    // 启动测量
    void start(const std::string& portName) {
        laser_ = std::make_shared<LaserRangeFinder>(portName);
        
        if (!laser_->detectBaudRate()) {
            std::cerr << "波特率检测失败" << std::endl;
            return;
        }

        // 启动连续测量
        laser_->startContinuousFastMeasurement(
            [this](LaserRangeFinder::MeasurementResult result) {
                if (result.valid) {
                    std::cout << "距离: " << result.distance << " mm，信号质量: " 
                              << result.signalQuality << std::endl;
                } else {
                    std::cout << "测量结果无效" << std::endl;
                }
            },
            stopFlag_ // 传递控制标志
        );
    }

    // 停止测量
    void stop() {
        stopFlag_ = true;
        if (laser_) {
            laser_->stopContinuousMeasurement();
        }
    }
};


