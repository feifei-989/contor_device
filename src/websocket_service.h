
#pragma once

#include <iostream>
#include <thread>
#include <string>
#include <functional>
#include <map>
#include <mutex>
#include <atomic>
#include <chrono>

#include "uWebSockets/App.h"
#include "nlohmann/json.hpp"
#include "CNCConsole.h"
#include "modbus_business.h"

using json = nlohmann::json;

class WebSocketService {
private:
    struct PerSocketData {
        int task_id = -1;
    };

    struct TaskState {
        uWS::WebSocket<false, true, PerSocketData>* ws;
        int total_sub_tasks;
        int completed_sub_tasks;
        std::chrono::steady_clock::time_point start_time;
    };

    std::map<int, TaskState> active_tasks_;
    std::atomic<int> next_task_id_ = 0;
    std::mutex tasks_mutex_;
    std::thread cleanup_thread_;
    std::atomic<bool> stop_cleanup_ = false;
    const std::chrono::seconds TASK_TIMEOUT = std::chrono::seconds(40);

    int port_;
    std::shared_ptr<ModbusBusiness> modbusBusiness_ptr_;
    std::shared_ptr<CNCConsole> cnc_ptr_;

public:
    WebSocketService(int port, std::shared_ptr<CNCConsole> cnc, std::shared_ptr<ModbusBusiness> modbus)
        : port_(port), cnc_ptr_(cnc), modbusBusiness_ptr_(modbus) 
    {
        cleanup_thread_ = std::thread(&WebSocketService::cleanup_loop, this);
    }

    ~WebSocketService() {
        stop_cleanup_ = true;
        if (cleanup_thread_.joinable()) {
            cleanup_thread_.join();
        }
    }

    void run() {
        uWS::App().ws<PerSocketData>("/control", {
            .open = [this](auto *ws) { handle_open(ws); },
            .message = [this](auto *ws, std::string_view message, uWS::OpCode opCode) { handle_message(ws, message, opCode); },
            .close = [this](auto *ws, int code, std::string_view message) { handle_close(ws, code, message); }
        }).listen(port_, [this](auto *listen_socket) {
            if (listen_socket) {
                std::cout << "SUCCESS: WebSocket server listening on port " << port_ << std::endl;
            } else {
                std::cerr << "ERROR: Failed to listen on port " << port_ << std::endl;
            }
        }).run();
    }

private:
    void handle_open(uWS::WebSocket<false, true, PerSocketData>* ws) {
        std::cout << "INFO: New client connected." << std::endl;
        ws->getUserData()->task_id = -1;
    }

    void handle_close(uWS::WebSocket<false, true, PerSocketData>* ws, int code, std::string_view message) {
        std::cout << "INFO: Client disconnected." << std::endl;
        int task_id_to_remove = ws->getUserData()->task_id;
        if (task_id_to_remove != -1) {
            std::lock_guard<std::mutex> lock(tasks_mutex_);
            active_tasks_.erase(task_id_to_remove);
            if (modbusBusiness_ptr_) modbusBusiness_ptr_->resetTask(task_id_to_remove);
            std::cout << "INFO: Cleaned up active task " << task_id_to_remove << " due to client disconnection." << std::endl;
        }
    }

    void handle_message(uWS::WebSocket<false, true, PerSocketData>* ws, std::string_view message, uWS::OpCode opCode) {
        if (opCode != uWS::TEXT) return;

        try {
            json j = json::parse(message);

            if (ws->getUserData()->task_id != -1) {
                ws->send("{\"status\":\"processing\",\"message\":\"Server is busy with a previous task\"}", uWS::OpCode::TEXT);
                return;
            }

            int total_sub_tasks = 0;
            if (j.contains("first")) total_sub_tasks++;
            if (j.contains("second")) total_sub_tasks++;

            if (total_sub_tasks == 0) {
                ws->send("{\"status\":\"error\",\"message\":\"No valid movement data in command\"}", uWS::OpCode::TEXT);
                return;
            }

            int task_id = next_task_id_++;
            ws->getUserData()->task_id = task_id;

            {
                std::lock_guard<std::mutex> lock(tasks_mutex_);
                active_tasks_[task_id] = {ws, total_sub_tasks, 0, std::chrono::steady_clock::now()};
            }
            std::cout << "INFO: Created aggregated task " << task_id << " for client with " << total_sub_tasks << " sub-task(s)." << std::endl;

            auto sub_task_callback = [this, task_id](bool success) {
                std::lock_guard<std::mutex> lock(tasks_mutex_);
                auto it = active_tasks_.find(task_id);
                if (it == active_tasks_.end()) return;

                auto* current_ws = it->second.ws;

                if (!success) {
                    std::cerr << "ERROR: Sub-task for task " << task_id << " failed." << std::endl;
                    try { current_ws->send("{\"status\":\"error\",\"message\":\"A sub-task failed.\"}", uWS::OpCode::TEXT); } catch (...) {}
                    current_ws->getUserData()->task_id = -1;
                    active_tasks_.erase(it);
                    if (modbusBusiness_ptr_) modbusBusiness_ptr_->resetTask(task_id);
                    return;
                }

                it->second.completed_sub_tasks++;
                std::cout << "INFO: Sub-task for task " << task_id << " completed. (" << it->second.completed_sub_tasks << "/" << it->second.total_sub_tasks << ")" << std::endl;

                if (it->second.completed_sub_tasks >= it->second.total_sub_tasks) {
                    std::cout << "SUCCESS: Aggregated task " << task_id << " fully completed." << std::endl;
                    try { current_ws->send("{\"status\":\"completed\"}", uWS::OpCode::TEXT); } catch (...) {}
                    current_ws->getUserData()->task_id = -1;
                    active_tasks_.erase(it);
                    // No need to call resetTask on success, as the controller does it automatically.
                }
            };

            ws->send("{\"status\":\"processing\"}", uWS::OpCode::TEXT);

            if (j.contains("first")) {
                if (cnc_ptr_) {
                    const auto& g1 = j["first"];
                    uint64_t speed = g1.value("speed", 25000);
                    cnc_ptr_->getYX20KController()->moveAbsolute2(g1.value("x", -80000.0), 0.0, g1.value("z", -80000.0), g1.value("c", -80000.0), speed, sub_task_callback);
                } else { sub_task_callback(false); }
            }

            if (j.contains("second")) {
                if (modbusBusiness_ptr_) {
                    const auto& g2 = j["second"];
                    if (g2.contains("vf")) modbusBusiness_ptr_->axisRunSpeed("X", std::to_string(g2.value("vf", (uint64_t)200)));
                    if (g2.contains("vg")) modbusBusiness_ptr_->axisRunSpeed("Z", std::to_string(g2.value("vg", (uint64_t)300)));
                    if (g2.contains("vf") || g2.contains("vg")) std::this_thread::sleep_for(std::chrono::milliseconds(100));

                    ModbusBusiness::CompositeMovementTask task;
                    task.x_value = std::to_string(g2.value("f", -80000.0));
                    task.z_value = std::to_string(g2.value("g", -80000.0));
                    
                    modbusBusiness_ptr_->pushCompositeTask(task, sub_task_callback, task_id);
                } else {
                    sub_task_callback(false);
                }
            }

        } catch (const std::exception& e) {
            std::cerr << "ERROR: Exception in handle_message: " << e.what() << std::endl;
            ws->send("{\"status\":\"error\",\"message\":\"An internal error occurred\"}", uWS::OpCode::TEXT);
            ws->getUserData()->task_id = -1;
        }
    }

    void cleanup_loop() {
        while (!stop_cleanup_) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            std::lock_guard<std::mutex> lock(tasks_mutex_);
            
            auto now = std::chrono::steady_clock::now();
            for (auto it = active_tasks_.begin(); it != active_tasks_.end(); ) {
                if (std::chrono::duration_cast<std::chrono::seconds>(now - it->second.start_time) > TASK_TIMEOUT) {
                    int task_id_to_cancel = it->first;
                    std::cerr << "ERROR: Task " << task_id_to_cancel << " timed out." << std::endl;
                    try {
                        it->second.ws->send("{\"status\":\"error\",\"message\":\"Task timed out\"}", uWS::OpCode::TEXT);
                        it->second.ws->getUserData()->task_id = -1;
                    } catch (...) { /* Ignore send errors */ }
                    
                    if (modbusBusiness_ptr_) modbusBusiness_ptr_->resetTask(task_id_to_cancel);
                    it = active_tasks_.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }
};