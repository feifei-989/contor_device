#pragma once

#include <iostream>
#include <thread>
#include <string>
#include <regex>
#include <mutex>
#include <memory>
#include <iostream>
#include <string>
#include <condition_variable>
#include <atomic>
#include<vector>
#include <queue>
#include <unordered_map>
#include <functional>
#include "modbus_protocol.h"

class ModbusBusiness{
  public:
       using MovementCallback = std::function<void(bool success, const std::string &msg)>;
       ModbusBusiness();
       ~ModbusBusiness();
      
	typedef struct Para_{
	   std::string axis;
	   uint8_t type;
	   std::string value;
	}Para;

 	struct CompositeMovementTask {
        	std::string x_value = "-80000";
        	std::string z_value = "-80000";
    	};

 public:
       int start();
       void stop();
       void resetTask(int task_id);
	void execute_command(const std::string& input);
	void execute_command(const std::string& input, const std::string value);
	void axisRunLength(const std::string &axis, const std::string &length);
	void axisRunSpeed(const std::string &axis, const std::string &speed);
	void pushData(const Para &data, MovementCallback cb, int task_id);
	void pushCompositeTask(const CompositeMovementTask& task, MovementCallback cb, int task_id);
  private:

    struct QueuedTask {
        Para para;
        MovementCallback cb;
    };

    struct QueuedTaskEx {
        int task_id;
        CompositeMovementTask task;
        MovementCallback cb;
    };

	void keyboard_listener();
	void get_length();
	bool bjlenght();
	void processQueue();

private:
    void processMovement();
    void pollPositions();
    bool run_axis_to_completion(int task_id, const std::string& axis, const std::string& target_value_str);

  private:
    MovementCallback current_callback_ = nullptr;
	std::mutex mtx;
 	bool run_ = false;
	std::shared_ptr<ModbusProtocol> modbusProtocol_ptr_ = nullptr;
	std::unordered_map<std::string, void (ModbusProtocol::*)()> command_map_;
        std::string x_speed_;
	std::string z_speed_;
	std::string x_length_;
	std::string z_length_;
	uint16_t current_x_length_;
	uint16_t current_z_length_;
	bool is_req = false;
	std::thread parse_thread_;
	std::mutex mutex_data_;
	std::condition_variable cv_;
	std::atomic<bool> stop_flag_;
	std::queue<QueuedTask> data_queue_;
	std::queue<QueuedTaskEx> data_queue_ex_;
        int  current_task_id_ = -1;
	bool x_need_run_ = false;
	bool z_need_run_ = false;
	bool device_run_ = false;
	uint8_t cut_ = 0;
	std::mutex queue_mutex_;
	    std::thread movement_thread_;
    std::thread polling_thread_;
    std::atomic<uint16_t> current_x_pos_ = {0};
    std::atomic<uint16_t> current_z_pos_ = {0};
    std::atomic<char> active_axis_ = {'\0'};
    std::atomic<bool> verification_pending_ = {false};
    std::atomic<int> verification_countdown_ = {0};
    std::mutex last_command_mutex_;
    std::string last_x_commanded_ = "-1";
    std::string last_z_commanded_ = "-1";
    bool enable_contorl_ = false;//true;
};
