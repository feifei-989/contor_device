#include "modbus_business.h"
#include "protocol_process.h"
#include <chrono>

ModbusBusiness::ModbusBusiness() {}

ModbusBusiness::~ModbusBusiness() {
    stop();
}

int ModbusBusiness::start() {
    modbusProtocol_ptr_ = std::make_shared<ModbusProtocol>();
    if (modbusProtocol_ptr_->start() != 0) { return -1; }
    run_ = true;
    movement_thread_ = std::thread(&ModbusBusiness::processMovement, this);
    polling_thread_ = std::thread(&ModbusBusiness::pollPositions, this);
    printf("ModbusBusiness started successfully.\n");
    return 0;
}

void ModbusBusiness::stop() {
    if (run_) {
        run_ = false;
        cv_.notify_one();
        if (movement_thread_.joinable()) movement_thread_.join();
        if (polling_thread_.joinable()) polling_thread_.join();
    }
}

void ModbusBusiness::pollPositions() {
    const int poll_interval_ms = 500;
    const int idle_poll_cycles = 4; // 4 * 500ms = 2 seconds
    int idle_counter = 0;

    while(run_) {
        if (verification_pending_) {
            if (verification_countdown_-- <= 0) {
                printf("VERIFY: 5-second verification period over. Checking final positions.\n");
                uint16_t final_x = modbusProtocol_ptr_->OneAxisCurrentCoordinates();
                uint16_t final_z = modbusProtocol_ptr_->TwoAxisCurrentCoordinates();
                
                std::lock_guard<std::mutex> lock(last_command_mutex_);
                if (std::to_string(final_x) != last_x_commanded_) {
                    printf("VERIFY: X-axis drifted. Updating last known position from %s to %u.\n", last_x_commanded_.c_str(), final_x);
                    last_x_commanded_ = std::to_string(final_x);
                }
                if (std::to_string(final_z) != last_z_commanded_) {
                    printf("VERIFY: Z-axis drifted. Updating last known position from %s to %u.\n", last_z_commanded_.c_str(), final_z);
                    last_z_commanded_ = std::to_string(final_z);
                }
                verification_pending_ = false;
            }
        }

        char currently_active = active_axis_.load();
        if (currently_active == 'X') {
            current_x_pos_ = modbusProtocol_ptr_->OneAxisCurrentCoordinates();
            idle_counter = 0;
        } else if (currently_active == 'Z') {
            current_z_pos_ = modbusProtocol_ptr_->TwoAxisCurrentCoordinates();
            idle_counter = 0;
        } else { // Idle
            if (++idle_counter >= idle_poll_cycles) {
                current_x_pos_ = modbusProtocol_ptr_->OneAxisCurrentCoordinates();
                current_z_pos_ = modbusProtocol_ptr_->TwoAxisCurrentCoordinates();
                printf("IDLE POLL: X=%u, Z=%u\n", current_x_pos_.load(), current_z_pos_.load());
                idle_counter = 0;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
    }
}

void ModbusBusiness::pushCompositeTask(const CompositeMovementTask& task, MovementCallback cb, int task_id) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (current_task_id_ != -1) {
        if (cb) cb(false);
        return;
    }
    data_queue_.push({task_id, task, cb});
    cv_.notify_one();
}

void ModbusBusiness::resetTask(int task_id) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (current_task_id_ == task_id) {
        current_task_id_ = -1;
        active_axis_ = '\0';
        verification_pending_ = false;
        std::queue<QueuedTask> empty_queue;
        data_queue_.swap(empty_queue);
    }
}

bool ModbusBusiness::run_axis_to_completion(int task_id, const std::string& axis, const std::string& target_value_str) {
    std::string* last_commanded_val_ptr = (axis == "X") ? &last_x_commanded_ : &last_z_commanded_;
    
    {
        std::lock_guard<std::mutex> lock(last_command_mutex_);
        if (target_value_str == "-80000" || target_value_str == *last_commanded_val_ptr) {
            return true;
        }
    }

    uint16_t target_pos = std::stoi(target_value_str);
    active_axis_ = axis[0]; // Set the active axis for the polling thread

    if (axis == "X") {
        modbusProtocol_ptr_->OneAxisLengthCommand2(target_value_str);
    } else {
        modbusProtocol_ptr_->TwoAxisLengthCommand2(target_value_str);
    }
    
    {
        std::lock_guard<std::mutex> lock(last_command_mutex_);
        *last_commanded_val_ptr = target_value_str;
    }

    // Wait for 2 seconds (4 cycles of 500ms)
    for(int i = 0; i < 4; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if(!run_ || current_task_id_ != task_id) {
            active_axis_ = '\0';
            return false;
        }
    }

    while (run_) {
        if (current_task_id_ != task_id) { 
            active_axis_ = '\0';
            return false; 
        }
        
        uint16_t current_pos = (axis == "X") ? current_x_pos_.load() : current_z_pos_.load();
        if (current_pos == target_pos) {
            printf("INFO: Axis %s reached target %u.\n", axis.c_str(), target_pos);
            active_axis_ = '\0'; // Clear the active axis
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    active_axis_ = '\0';
    return false;
}

void ModbusBusiness::processMovement() {
    while (run_) {
        QueuedTask current_task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            cv_.wait(lock, [this] { return !data_queue_.empty() || !run_; });
            if (!run_) break;
            current_task = data_queue_.front();
            data_queue_.pop();
            current_task_id_ = current_task.task_id;
            verification_pending_ = false;
        }

        bool x_success = run_axis_to_completion(current_task.task_id, "X", current_task.task.x_value);
        bool z_success = false;
        if (x_success) {
            z_success = run_axis_to_completion(current_task.task_id, "Z", current_task.task.z_value);
        }

        bool overall_success = x_success && z_success;

        if (current_task.cb) {
            current_task.cb(overall_success);
        }

        if (overall_success) {
            ProtocolProcess protocol_process_;
            protocol_process_.sendResults("ok", 2);
            verification_countdown_ = 10;
            verification_pending_ = true;
        }

        current_task_id_ = -1;
    }
}

// --- Legacy function implementations ---
void ModbusBusiness::axisRunSpeed(const std::string &axis, const std::string &speed) { /* ... */ }
void ModbusBusiness::execute_command(const std::string& input) { /* ... */ }
void ModbusBusiness::execute_command(const std::string& input, const std::string value) { /* ... */ }