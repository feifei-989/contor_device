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

    // This start method is now for initializing OTHER internal, non-shared services.
    void start() {
        // Create other non-shared services here, as they were in the original design.
        // controller_ptr_ = std::make_shared<Controller>("/dev/ttyUSB0");
        // controller_ptr_->start();
        //laser_manager_ptr_ = std::make_shared<LaserManager>();
        //laser_manager_ptr_->start("/dev/ttyUSB1");
        //sensor_manager_ptr_ = std::make_shared<SensorManager>("/dev/ttyUSB0");
        //sensor_manager_ptr_->start();
    }

    void start2() {
        // Create other non-shared services here, as they were in the original design.
        controller_ptr_ = std::make_shared<Controller>("/dev/ttyUSB0");
        controller_ptr_->start();
        //laser_manager_ptr_ = std::make_shared<LaserManager>();
        //laser_manager_ptr_->start("/dev/ttyUSB1");
        //sensor_manager_ptr_ = std::make_shared<SensorManager>("/dev/ttyUSB0");
        //sensor_manager_ptr_->start();
    }
    void run() {
        std::cout << "HTTP server is running on port " << port_ << "..." << std::endl;
        server_.listen("0.0.0.0", port_);
    }

private:
    void handlePost(const httplib::Request &req, httplib::Response &res) {
        try {
            json j = json::parse(req.body);
            if (j.contains("mode")) {
		auto mode = j["mode"];
		if(mode == 0){
                  auto angle = j["angle"];
                  auto axis = j["axis"];
		  auto speed = j["speed"];
		  printf("---------contor duo zuo \n");
                  std::cout << "Received angle: " << angle << " axis:" << axis << " speed: "<< speed << std::endl;
                  json response;
                  response["status"] = "success";
                  res.set_content(response.dump(), "application/json");
		  if(controller_ptr_ != nullptr){
		    //controller_ptr_->moveToPosition2(speed,angle);
		  }
		  if(cnc_ptr_ != nullptr){
		    printf("0000000\n");
		    cnc_ptr_->getYX20KController()->moveAxis2(axis, angle, speed);
		  }
		}else if(mode == 1 || 4 == mode){ 
                  auto angle = j["angle"];
                  auto angle1 = j["angle1"];
                  auto angle2 = j["angle2"];
                  auto angle3 = j["angle3"];
		  auto speed = j["speed"];
                  std::cout << "Received angle: " << angle << " angle1:" << angle1 << " angle2:" << angle2
			<< " angle3:" << angle3  << " speed: "<< speed << std::endl;
                  json response;
                  response["status"] = "success";
                  res.set_content(response.dump(), "application/json");
		  if(controller_ptr_ != nullptr){
		    //controller_ptr_->moveToPosition2(speed,angle);
		  }
		  if(cnc_ptr_ != nullptr){
		    if(mode == 1) cnc_ptr_->getYX20KController()->moveAbsolute(angle, angle1, angle2, angle3, speed);
		    else if(mode == 4) cnc_ptr_->getYX20KController()->moveAbsolute2(angle, angle1, angle2, angle3, speed);
		  }
		}else if(mode == 2 || mode == 5){ // zhuan tai
                  auto angle = j["angle"];
                  auto axis = j["axis"];
		  auto speed = j["speed"];
                  std::cout << "Received angle: " << angle << " axis:" << axis << " speed: "<< speed << std::endl;
                  json response;
                  response["status"] = "success";
                  res.set_content(response.dump(), "application/json");
		  if(modbusBusiness_ptr_ != nullptr){
		   bool ok = false;
		   if(true && speed > 0){
			if(axis == "X"){
			    if(speed > 600) speed = 600;
			}else if(speed == "Z"){
			    if(speed > 300) speed = 300;
			}
		        std::string s = std::to_string(static_cast<int>(speed));
		   	modbusBusiness_ptr_->axisRunSpeed(axis,s);
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			ok = true;
		   }
		   if(angle >= 0){
			#if 0
		    	std::string str = std::to_string(static_cast<int>(angle));
		    	modbusBusiness_ptr_->axisRunLength(axis,str);
			std::this_thread::sleep_for(std::chrono::milliseconds(2000));
			#else
			/*
			ModbusBusiness::Para para;
			para.axis = axis;
			para.value = std::to_string(static_cast<int>(angle));
			modbusBusiness_ptr_->pushData(para,nullptr,7777);
			*/
			ModbusBusiness::CompositeMovementTask task;
                        if (axis == "X") {
                            task.x_value = std::to_string(static_cast<int>(angle));
                        } else if (axis == "Z") {
                            task.z_value = std::to_string(static_cast<int>(angle));
                        }
			printf("----------\n");
			modbusBusiness_ptr_->pushCompositeTask(task, nullptr, 7777);
			#endif
			ok = true;
		    }
		    if(!ok){
		    	printf("No control of turntable.....\n");
			ProtocolProcess protocol_process_;
			protocol_process_.sendResults("ok",2);
		    }
		    //ProtocolProcess protocol_process_;
		    //protocol_process_.sendResults("ok",2);
		  }
		}else if(mode == 3){
                  auto angle = j["angle"];
                  auto axis = j["axis"];
		  auto speed = j["speed"];
                  std::cout << "mode = " << mode << "Received angle: " << angle << " axis:" << axis << " speed: "<< speed << std::endl;
                  json response;
                  response["status"] = "success";
                  res.set_content(response.dump(), "application/json");
		  if(controller_ptr_ != nullptr){
		    controller_ptr_->moveToPosition2(speed,angle);
		  }
		}
            } else {
                json response;
                response["status"] = "error";
                response["message"] = "Missing angle field";
                res.status = 400;
                res.set_content(response.dump(), "application/json");
            }
        } catch (const std::exception &e) {
            json response;
            response["status"] = "error";
            response["message"] = e.what();
            res.status = 400;
            res.set_content(response.dump(), "application/json");
        }
    }
	
	
    void handleModel(const httplib::Request &req, httplib::Response &res) {
    try {
        json j = json::parse(req.body);
        if (j.contains("model")) {
            auto model = j["model"];
	        auto value = j["value"];
            std::cout << "Received model: " << model << " vlaue = "<< value << std::endl;
            json response;
            response["status"] = "success";
            //response["received"] = model;
            res.set_content(response.dump(), "application/json");
	    if(nullptr != modbusBusiness_ptr_){
	    	modbusBusiness_ptr_->execute_command(model);
            modbusBusiness_ptr_->execute_command(model,value);
	    }
        } else {
            json response;
            response["status"] = "error";
            response["message"] = "Missing model field";
            res.status = 400;
            res.set_content(response.dump(), "application/json");
        }
    } catch (const std::exception &e) {
            json response;
            response["status"] = "error";
            response["message"] = e.what();
            res.status = 400;
            res.set_content(response.dump(), "application/json");
        }
    }

    void handleInfoMsg(const httplib::Request &req, httplib::Response &res) {
        try {
            json j = json::parse(req.body);
            if (j.contains("subject")) {
                auto subject = j["subject"];
                auto message = j["message"];
                std::cout << "Received subject: " << subject << " message = "<< message << std::endl;
                json response;
                response["status"] = "success";
                //response["received"] = model;
                res.set_content(response.dump(), "application/json");
            } else {
                json response;
                response["status"] = "error";
                response["message"] = "Missing model field";
                res.status = 400;
                res.set_content(response.dump(), "application/json");
            }
        } catch (const std::exception &e) {
            json response;
            response["status"] = "error";
            response["message"] = e.what();
            res.status = 400;
            res.set_content(response.dump(), "application/json");
         }
    }
    


    httplib::Server server_;
    int port_;
    // Shared controllers from outside
    std::shared_ptr<ModbusBusiness> modbusBusiness_ptr_;
    std::shared_ptr<CNCConsole> cnc_ptr_;
    // Internally managed controllers
    std::shared_ptr<Controller> controller_ptr_ = nullptr;
    std::shared_ptr<LaserManager> laser_manager_ptr_ = nullptr;
    std::shared_ptr<SensorManager> sensor_manager_ptr_ = nullptr;
};

