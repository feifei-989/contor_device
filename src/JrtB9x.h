

#ifndef JRTB9X_H
#define JRTB9X_H

#include "serial_port.h"
#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <mutex>

// A structure to hold the result of a measurement
struct MeasurementResult {
    bool success;          // True if the measurement was successful
    uint32_t distance_mm;  // Measured distance in millimeters
    uint16_t signal_quality; // Signal quality, a smaller value is better
    uint16_t error_code;   // Error code if success is false, 0 otherwise
};

class JrtB9xSensor {
public:
    enum MeasurementMode {
        SINGLE_AUTO = 0x00,
        SINGLE_SLOW = 0x01,
        SINGLE_FAST = 0x02,
        CONTINUOUS_AUTO = 0x04,
        CONTINUOUS_SLOW = 0x05,
        CONTINUOUS_FAST = 0x06
    };

    JrtB9xSensor(const std::string& port_name);
    ~JrtB9xSensor();

    bool init();
    MeasurementResult takeSingleMeasurement(MeasurementMode mode);
    bool startContinuousMeasurement(MeasurementMode mode);
    MeasurementResult readContinuousMeasurement();
    bool stopContinuousMeasurement();

    // --- NEW: Laser Control Methods ---
    /**
     * @brief Turns the sensor's laser diode ON.
     * @return True if the command was acknowledged successfully.
     */
    bool turnLaserOn();

    /**
     * @brief Turns the sensor's laser diode OFF.
     * @return True if the command was acknowledged successfully.
     */
    bool turnLaserOff();


    // --- Convenience functions ---
    bool autoContinuous();
    MeasurementResult slowSingle();
    MeasurementResult fastSingle();


private:
    std::unique_ptr<SerialPort> serial_;
    uint8_t slave_address_ = 0x00;
    bool continuous_mode_ = false;
    std::mutex comm_mutex_;

    std::vector<uint8_t> createCommandFrame(uint8_t rw, uint16_t reg, const std::vector<uint16_t>& payload);
    MeasurementResult parseResponse(const std::vector<uint8_t>& response);
};

#endif // JRTB9X_H

