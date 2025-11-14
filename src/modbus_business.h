#pragma once

#include <iostream>
#include <thread>
#include <string>
#include <regex>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <queue>
#include <unordered_map>
#include <functional>
#include <optional>
#include "modbus_protocol.h"

class ModbusBusiness {
public:
    using MovementCallback = std::function<void(bool success)>;

    struct CompositeMovementTask {
        std::string x_value = "-80000";
        std::string z_value = "-80000";
    };

private:
    struct QueuedTask {
        int task_id;
        CompositeMovementTask task;
        MovementCallback cb;
    };

public:
    ModbusBusiness();
    ~ModbusBusiness();

    int start();
    void stop();

    void pushCompositeTask(const CompositeMovementTask& task, MovementCallback cb, int task_id);
    void resetTask(int task_id);

    void execute_command(const std::string& input);
    void execute_command(const std::string& input, const std::string value);
    void axisRunSpeed(const std::string &axis, const std::string &speed);

private:
    void processMovement();
    void pollPositions();
    bool run_axis_to_completion(int task_id, const std::string& axis, const std::string& target_value_str);

private:
    std::atomic<bool> run_ = {false};
    std::shared_ptr<ModbusProtocol> modbusProtocol_ptr_ = nullptr;

    std::queue<QueuedTask> data_queue_;
    std::mutex queue_mutex_;
    std::condition_variable cv_;
    std::thread movement_thread_;

    std::thread polling_thread_;
    std::atomic<uint16_t> current_x_pos_ = {0};
    std::atomic<uint16_t> current_z_pos_ = {0};
    std::atomic<char> active_axis_ = {'\0'};

    std::atomic<int> current_task_id_ = {-1};

    std::atomic<bool> verification_pending_ = {false};
    std::atomic<int> verification_countdown_ = {0};

    std::mutex last_command_mutex_;
    std::string last_x_commanded_ = "-1";
    std::string last_z_commanded_ = "-1";

    std::unordered_map<std::string, void (ModbusProtocol::*)()> command_map_;
    std::string x_speed_;
    std::string z_speed_;
};