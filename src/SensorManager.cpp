

#include "SensorManager.h"
#include <iostream>
#include <chrono>

SensorManager::SensorManager(const std::string& port) {
    sensor_ = std::make_unique<JrtB9xSensor>(port);
    is_running_ = false;
}

SensorManager::~SensorManager() {
    stop();
}

bool SensorManager::start() {
    if (!sensor_->init()) {
        std::lock_guard<std::mutex> lock(cout_mutex_);
        std::cerr << "❌ Sensor hardware initialization failed. Cannot start measurement thread." << std::endl;
        return false;
    }

    is_running_ = true;
    worker_thread_ = std::thread(&SensorManager::measurement_loop, this);
    
    return true;
}

void SensorManager::stop() {
    is_running_ = false;
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void SensorManager::measurement_loop() {
    // --- OPTIMIZATION: Turn laser ON before starting ---
    {
        std::lock_guard<std::mutex> lock(cout_mutex_);
        std::cout << "[Thread] Turning laser ON..." << std::endl;
    }
    if (!sensor_->turnLaserOn()) {
        std::lock_guard<std::mutex> lock(cout_mutex_);
        std::cerr << "[Thread] ❌ Failed to turn laser ON." << std::endl;
        return;
    }

    if (!sensor_->autoContinuous()) {
        std::lock_guard<std::mutex> lock(cout_mutex_);
        std::cerr << "❌ Failed to start continuous auto-mode on sensor." << std::endl;
        // Turn the laser off on failure
        sensor_->turnLaserOff();
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(cout_mutex_);
        std::cout << "✅ Measurement thread started. Reading data..." << std::endl;
    }

    while (is_running_) {
        MeasurementResult result = sensor_->readContinuousMeasurement();

        if (result.success) {
            std::lock_guard<std::mutex> lock(cout_mutex_);
            std::cout << "[Thread] Distance: " << result.distance_mm 
                      << " mm, Signal Quality: " << result.signal_quality << std::endl;
        } else if (result.error_code != 0xFFFF) {
            std::lock_guard<std::mutex> lock(cout_mutex_);
            std::cerr << "[Thread] Sensor Error Code: 0x" << std::hex << result.error_code << std::dec << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // After the loop ends, stop the measurement mode first.
    sensor_->stopContinuousMeasurement();
    
    // --- OPTIMIZATION: Turn laser OFF after stopping ---
    {
        std::lock_guard<std::mutex> lock(cout_mutex_);
        std::cout << "[Thread] Turning laser OFF..." << std::endl;
    }
    sensor_->turnLaserOff();
    
    {
        std::lock_guard<std::mutex> lock(cout_mutex_);
        std::cout << "✅ Measurement thread finished." << std::endl;
    }
}

