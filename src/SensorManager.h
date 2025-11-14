

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "JrtB9x.h"
#include <memory>
#include <thread>
#include <atomic>
#include <string>
#include <mutex>

class SensorManager {
public:
    /**
     * @brief Constructor that initializes the sensor.
     * @param port The serial port name (e.g., "/dev/ttyUSB0").
     */
    SensorManager(const std::string& port);

    /**
     * @brief Destructor that ensures the measurement thread is stopped and joined correctly.
     */
    ~SensorManager();

    /**
     * @brief Starts the background thread for continuous measurement.
     * @return True if the sensor is initialized and the thread starts successfully, false otherwise.
     */
    bool start();

    /**
     * @brief Stops the background measurement thread.
     */
    void stop();

private:
    /**
     * @brief The main function for the worker thread.
     * This loop continuously reads and prints sensor data.
     */
    void measurement_loop();

    // A smart pointer to manage the JrtB9xSensor object.
    // std::unique_ptr ensures the sensor is automatically deleted when SensorManager is destroyed.
    std::unique_ptr<JrtB9xSensor> sensor_;

    // The thread object for background processing.
    std::thread worker_thread_;

    // An atomic flag to safely control the thread's execution loop.
    std::atomic<bool> is_running_;

    // A mutex to prevent garbled output from multiple threads writing to std::cout.
    std::mutex cout_mutex_;
};

#endif // SENSOR_MANAGER_H


