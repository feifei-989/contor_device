

#include "JrtB9x.h"
#include <iostream>
#include <numeric>
#include <chrono>
#include <thread>

// The manual specifies a baud rate of 19200 bps.
JrtB9xSensor::JrtB9xSensor(const std::string& port_name) : continuous_mode_(false) {
    serial_ = std::make_unique<SerialPort>(port_name);
}

JrtB9xSensor::~JrtB9xSensor() {
    if (continuous_mode_) {
        stopContinuousMeasurement();
    }
}

// ... (init, createCommandFrame, parseResponse, and other measurement methods remain the same) ...
// The implementation for the existing methods is omitted for brevity. You can use the one from the previous answer.

bool JrtB9xSensor::init() {
    return true;
    std::lock_guard<std::mutex> lock(comm_mutex_);
    serial_->sendData({0x55});
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    auto response = serial_->receiveData(1);

    if (!response.empty()) {
        slave_address_ = response[0];
        std::cout << "✅ Sensor initialized. Address: 0x" << std::hex << (int)slave_address_ << std::dec << std::endl;
	slave_address_ = 0x00;
        return true;
    } else {
        std::cerr << "❌ Sensor initialization failed. No response to auto-baud command." << std::endl;
        return false;
    }
}

std::vector<uint8_t> JrtB9xSensor::createCommandFrame(uint8_t rw, uint16_t reg, const std::vector<uint16_t>& payload) {
    std::vector<uint8_t> frame;
    frame.push_back(0xAA);
    uint8_t rw_addr = (rw << 7) | (slave_address_ & 0x7F);
    frame.push_back(rw_addr);
    frame.push_back((reg >> 8) & 0xFF);
    frame.push_back(reg & 0xFF);
    if (rw == 0) {
        uint16_t payload_count = payload.size();
        frame.push_back((payload_count >> 8) & 0xFF);
        frame.push_back(payload_count & 0xFF);
        for (uint16_t val : payload) {
            frame.push_back((val >> 8) & 0xFF);
            frame.push_back(val & 0xFF);
        }
    }
    uint8_t checksum = 0;
    for (size_t i = 1; i < frame.size(); ++i) {
        checksum += frame[i];
    }
    frame.push_back(checksum);
    return frame;
}

MeasurementResult JrtB9xSensor::parseResponse(const std::vector<uint8_t>& response) {
    MeasurementResult result = {false, 0, 0, 0};
    if (response.empty()) {
        result.error_code = 0xFFFF;
        return result;
    }
    if (response[0] == 0xEE) {
        if (response.size() >= 8) {
            result.success = false;
            result.error_code = (response[6] << 8) | response[7];
        }
        return result;
    }
    if (response[0] != 0xAA) {
        return result;
    }
    if (response.size() < 13) {
        return result;
    }
    if (response[2] == 0x00 && response[3] == 0x22) {
        result.distance_mm = (uint32_t)(response[6] << 24 | response[7] << 16 | response[8] << 8 | response[9]);
        result.signal_quality = (uint16_t)(response[10] << 8 | response[11]);
        result.success = true;
        result.error_code = 0x0000;
    }
    return result;
}

MeasurementResult JrtB9xSensor::takeSingleMeasurement(MeasurementMode mode) {
    std::lock_guard<std::mutex> lock(comm_mutex_);
    if (continuous_mode_) {
        std::cerr << "⚠️ Cannot take single measurement while in continuous mode." << std::endl;
        return {false, 0, 0, 0};
    }
    std::vector<uint16_t> payload = {(uint16_t)mode};
    auto command = createCommandFrame(0, 0x0020, payload);
    serial_->sendData(command);
    auto response = serial_->receiveData(13);
    return parseResponse(response);
}

bool JrtB9xSensor::startContinuousMeasurement(MeasurementMode mode) {
    std::lock_guard<std::mutex> lock(comm_mutex_);
    if (continuous_mode_) {
        std::cerr << "⚠️ Continuous mode already active." << std::endl;
        return false;
    }
    std::vector<uint16_t> payload = {(uint16_t)mode};
    auto command = createCommandFrame(0, 0x0020, payload);
    int bytes_sent = serial_->sendData(command);
    if (bytes_sent > 0) {
        continuous_mode_ = true;
        return true;
    }
    return false;
}

MeasurementResult JrtB9xSensor::readContinuousMeasurement() {
    std::lock_guard<std::mutex> lock(comm_mutex_);
    if (!continuous_mode_) {
        std::cerr << "⚠️ Not in continuous mode." << std::endl;
        return {false, 0, 0, 0};
    }
    auto response = serial_->receiveData(13);
    return parseResponse(response);
}

bool JrtB9xSensor::stopContinuousMeasurement() {
    std::lock_guard<std::mutex> lock(comm_mutex_);
    if (!continuous_mode_) {
        return true;
    }
    int bytes_sent = serial_->sendData({0x58});
    if (bytes_sent > 0) {
        continuous_mode_ = false;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        serial_->receiveData(100);
        return true;
    }
    return false;
}


// --- NEW: Laser Control Implementations ---

bool JrtB9xSensor::turnLaserOn() {
    std::lock_guard<std::mutex> lock(comm_mutex_);
    // Payload for turning laser on is 0x0001 as per section 6.4.9
    std::vector<uint16_t> payload = {0x0001};
    // Register for laser diode control is 0x01BE
    auto command = createCommandFrame(0, 0x01BE, payload);

    serial_->sendData(command,1000);
    // The response frame has 9 bytes according to table 6-19
    auto response = serial_->receiveData(9,1000);

    // Basic check for acknowledgement
    return !response.empty() && response[0] == 0xAA;
}

bool JrtB9xSensor::turnLaserOff() {
    std::lock_guard<std::mutex> lock(comm_mutex_);
    // Payload for turning laser off is 0x0000 as per section 6.4.9
    std::vector<uint16_t> payload = {0x0000};
    // Register for laser diode control is 0x01BE
    auto command = createCommandFrame(0, 0x01BE, payload);

    serial_->sendData(command);
    // The response frame has 9 bytes according to table 6-19
    auto response = serial_->receiveData(9);

    // Basic check for acknowledgement
    return !response.empty() && response[0] == 0xAA;
}


// --- Convenience functions ---

bool JrtB9xSensor::autoContinuous() {
    return startContinuousMeasurement(CONTINUOUS_AUTO);
}

MeasurementResult JrtB9xSensor::slowSingle() {
    return takeSingleMeasurement(SINGLE_SLOW);
}

MeasurementResult JrtB9xSensor::fastSingle() {
    return takeSingleMeasurement(SINGLE_FAST);
}



