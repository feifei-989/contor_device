

#pragma once
#include <iostream>
#include <thread>
#include "httplib.h"    
#include "nlohmann/json.hpp"   
#include "controller.h"
#include "modbus_business.h"
#include "CNCConsole.h"
#include "laser_range_business.h"
#include "SensorManager.h"
using json = nlohmann::json;

class HttpServer {
public:
    HttpServer(int port, std::shared_ptr<CNCConsole> cnc, std::shared_ptr<ModbusBusiness> modbus)
        : port_(port), cnc_ptr_(cnc), modbusBusiness_ptr_(modbus) 
    {
        server_.set_pre_routing_handler([](const httplib::Request &req, httplib::Response &res) -> httplib::Server::HandlerResponse {
          res.set_header("Access-Control-Allow-Origin", "*");
          res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
          res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
          if (req.method == "OPTIONS") {
            res.status = 200;
            return httplib::Server::HandlerResponse::Handled;
          }
          return httplib::Server::HandlerResponse::Unhandled;
        });

        server_.Post("/angle", [this](const httplib::Request &req, httplib::Response &res) {
            this->handlePost(req, res);
        });

        server_.Post("/model", [this](const httplib::Request &req, httplib::Response &res) {
            this->handleModel(req, res);
        });

       server_.Post("/info_msg", [this](const httplib::Request &req, httplib::Response &res) {
        this->handleInfoMsg(req, res);
        });
    }

    void start() {}

    void run() {
        std::cout << "HTTP server is running on port " << port_ << "..." << std::endl;
        server_.listen("0.0.0.0", port_);
    }

private:
    void handlePost(const httplib::Request &req, httplib::Response &res) {
        try {
            json j = json::parse(req.body);
            if (j.contains("mode")) {
                auto mode = j["mode"].get<int>();
                if (mode == 2 || mode == 5) { // Turntable control
                    if(modbusBusiness_ptr_){
                        auto axis = j.value("axis", "");
                        auto angle = j.value("angle", 0.0);
                        auto speed = j.value("speed", 0.0);

                        if(speed > 0){
                            modbusBusiness_ptr_->axisRunSpeed(axis, std::to_string(static_cast<int>(speed)));
                            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Give speed command time to process
                        }

                        ModbusBusiness::CompositeMovementTask task;
                        if (axis == "X") {
                            task.x_value = std::to_string(static_cast<int>(angle));
                        } else if (axis == "Z") {
                            task.z_value = std::to_string(static_cast<int>(angle));
                        }

                        // HTTP is fire-and-forget, so callback is null
                        modbusBusiness_ptr_->pushCompositeTask(task, nullptr, 7777);
                        res.set_content("{\"status\":\"processing\"}", "application/json");
                    }
                } else if (mode == 1 || mode == 4) { // CNC Control
                    if(cnc_ptr_){
                        auto angle = j.value("angle", -80000.0);
                        auto angle1 = j.value("angle1", -80000.0);
                        auto angle2 = j.value("angle2", -80000.0);
                        auto angle3 = j.value("angle3", -80000.0);
                        auto speed = j.value("speed", 25000);
                        if(mode == 1) cnc_ptr_->getYX20KController()->moveAbsolute(angle, angle1, angle2, angle3, speed);
                        else if(mode == 4) cnc_ptr_->getYX20KController()->moveAbsolute2(angle, angle1, angle2, angle3, speed, nullptr);
                    }
                    res.set_content("{\"status\":\"success\"}", "application/json");
                } else {
                    res.set_content("{\"status\":\"success\"}", "application/json");
                }
            } else {
                res.status = 400;
                res.set_content("{\"status\":\"error\",\"message\":\"Missing mode field\"}", "application/json");
            }
        } catch (const std::exception &e) {
            res.status = 400;
            res.set_content("{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}", "application/json");
        }
    }

    void handleModel(const httplib::Request &req, httplib::Response &res) {
        try {
            json j = json::parse(req.body);
            if (j.contains("model")) {
                auto model = j["model"].get<std::string>();
                auto value = j.value("value", "");
                std::cout << "Received model: " << model << " value = "<< value << std::endl;
                if(modbusBusiness_ptr_){
                    modbusBusiness_ptr_->execute_command(model, value);
                }
                res.set_content("{\"status\":\"success\"}", "application/json");
            } else {
                res.status = 400;
                res.set_content("{\"status\":\"error\",\"message\":\"Missing model field\"}", "application/json");
            }
        }
        catch (const std::exception &e) {
            res.status = 400;
            res.set_content("{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}", "application/json");
        }
    }

    void handleInfoMsg(const httplib::Request &req, httplib::Response &res) { /* ... */ }
    
    httplib::Server server_;
    int port_;
    std::shared_ptr<ModbusBusiness> modbusBusiness_ptr_;
    std::shared_ptr<CNCConsole> cnc_ptr_;
    std::shared_ptr<Controller> controller_ptr_ = nullptr;
    std::shared_ptr<LaserManager> laser_manager_ptr_ = nullptr;
    std::shared_ptr<SensorManager> sensor_manager_ptr_ = nullptr;
};
