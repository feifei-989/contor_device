#pragma once

#include <iostream>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <memory>
#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include "serial_port.h"
#include "protocol_process.h"
#include "common.h"
class Controller {
private:
    std::shared_ptr<SerialPort> serial_port_ptr_ = nullptr;
    bool running_ = false;
    std::thread worker_thread_;
    std::thread inputThread_;
    bool is_run_ = false;
    int cnt = 0;
    bool have_run_ = false;
    float old_move_point_ = 0.0;
    ProtocolProcess protocol_process_;
public:
    Controller(const std::string& port) {
    	serial_port_ptr_ = std::make_shared<SerialPort>(port);
	auto r = Common::getContorDevUrl();
	protocol_process_.setUrl(r);
    }
    ~Controller(){
    	stop();
    }
    int start(){
        running_ = false;
        worker_thread_ = std::thread(&Controller::run, this);
	worker_thread_.detach();
	//inputThread_ = std::thread(&Controller::keyboardInputThread, this);
   	//inputThread_.detach();
	return 0;
    }

    void stop() {
        running_ = true;
    }


    // 定速运行（不指定终点）
    void moveConstantSpeed(bool forward, uint16_t speed) {
        std::vector<uint8_t> cmd = {
            0x55, 0xAA, 0x06, (forward ? 0x09 : 0x0A),
            static_cast<uint8_t>(speed & 0xFF), static_cast<uint8_t>(speed >> 8),
            0x00, 0x00, 0x00, 0xC3
        };
        serial_port_ptr_->sendData2(cmd);
    }

    // 定速运行（指定终点）
    void moveToPosition(uint16_t speed, int32_t steps, bool absolute = true) {
       if(is_run_) {
       	  printf("device is run ....\n");
	  return;
       }
        std::vector<uint8_t> cmd = {
            0x55, 0xAA, (absolute ? 0x07 : 0x08),
            static_cast<uint8_t>(speed & 0xFF), static_cast<uint8_t>(speed >> 8),
            static_cast<uint8_t>(steps & 0xFF),
            static_cast<uint8_t>((steps >> 8) & 0xFF),
            static_cast<uint8_t>((steps >> 16) & 0xFF),
            static_cast<uint8_t>((steps >> 24) & 0xFF),
            0xC3
        };
        serial_port_ptr_->sendData2(cmd);
    }

    void moveToPosition2(float speed, float angle) {
    	if(speed < 0){
	  speed = 10000;
	}else if(speed > 40000){
	  speed = 30000;
	}else if(speed < 400){
	  speed = 500;
	}
	if(std::fabs(angle) < 1e-6f){
   	  if(old_move_point_ > 0) returnToMechanicalZero(25000,false);
	  else returnToMechanicalZero(25000,true);
	}else{
	  //int32_t steps = angle * 1240;
	  int32_t steps = angle * 800;
	  old_move_point_ = angle;
	  printf(">> move speed is = %f to position is = %d\n",speed,steps);
	  moveToPosition(speed,steps);
	}
	have_run_ = true;
    }
    // 暂停运行
    void pause() {
        std::vector<uint8_t> cmd = {0x55, 0xAA, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC3};
        serial_port_ptr_->sendData2(cmd);
    }

    // 回程序零
    void returnToProgramZero(uint16_t speed) {
        std::vector<uint8_t> cmd = {0x55, 0xAA, 0x07, speed & 0xFF, speed >> 8, 0x00, 0x00, 0x00, 0x00, 0xC3};
        serial_port_ptr_->sendData2(cmd);
    }

    // 回机械零
    void returnToMechanicalZero(uint16_t speed, bool forward = true) {
	if(is_run_) return;
        std::vector<uint8_t> cmd = {
            0x55, 0xAA, 0x0B, (forward ? 0x09 : 0x0A),
            static_cast<uint8_t>(speed & 0xFF), static_cast<uint8_t>(speed >> 8),
            0x00, 0x00, 0x00, 0xC3
        };
        serial_port_ptr_->sendData2(cmd);
    }

    // 读取状态
    void getStatus() {
	static uint32_t old_position = 0;
        std::vector<uint8_t> cmd = {0x55, 0xAA, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC3};
	std::vector<uint8_t> response = serial_port_ptr_->sendData2(cmd);
        if (response.size() < 8) {
            std::cerr << "❌ 读取失败" << std::endl;
            return;
        }
        int32_t position = (response[0] << 24) | (response[1] << 16) | (response[2] << 8) | response[3];
        uint16_t timer_value = (response[4] << 8) | response[5];
        uint8_t input_status = response[6];
        uint8_t work_status = response[7];
	if(old_position != position){
	   old_position = position;
	   cnt = 0;
	}else{
	   if(cnt++ > 1){
	     cnt = 5;
	     is_run_ = false;
	     if(have_run_){
		have_run_ = false;
	     	protocol_process_.sendResults("ok");
	     }
	   }
	   return;
	}
	is_run_ = true;
        double speed = 14745600.0 / (65535 + 46 - timer_value);	
        std::cout << "📌 设备状态：" << std::endl;
        std::cout << " - 当前位置: " << position << " 步" << std::endl;
        std::cout << " - 计算速度: " << speed << std::endl;
        std::cout << " - 限位状态: " << std::bitset<8>(input_status) << std::endl;
        std::cout << " - 工作模式: " << std::hex << static_cast<int>(work_status) << std::dec << std::endl;
    }

    // 控制输出端口
    void controlOutput(uint8_t port, bool status) {
        std::vector<uint8_t> cmd = {0x55, 0xAA, 0x0D, port, (status ? 1 : 0), 0x00, 0x00, 0x00, 0x00, 0xC3};
        serial_port_ptr_->sendData(cmd);
    }
private:
    void run() {
        while (!running_) {
            {
		getStatus();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        }
        std::cout << "Worker thread stopped." << std::endl;
    }

    void keyboardInputThread() {
       std::string input;
       while (!running_) {
         std::cout << "请输入一个数字 (输入 exit 退出): ";
         if (!std::getline(std::cin, input)) {
            break;
         }
         if (input == "exit") {
            break;
         }
         try {
            float value = std::stof(input);
	    if(value > 360){
	    	returnToMechanicalZero(25000,false);
	    }else if(value > 0 && value <= 360){
	    	//uint32_t d = value * 1240;
	    	uint32_t d = value * 800;
		printf("move to position is = %d\n",d);
		moveToPosition(25000,d);
	    }
            std::cout << "转换后的 float 值为: " << value << std::endl;
         } catch (const std::invalid_argument& e) {
            std::cout << "输入无效，不是一个数字，请重新输入。" << std::endl;
         } catch (const std::out_of_range& e) {
            std::cout << "输入的数字超出范围，请重新输入。" << std::endl;
         }
       }
    }
};


