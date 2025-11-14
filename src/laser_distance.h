
#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <thread>
#include <chrono>
#include <iostream>
#include <vector>
#include <iomanip>
#include <sstream>

// 串口通信库（此处需要引入适合您平台的串口库，如Windows下的SerialPort或Linux下的termios）
#include "serial_port.h" // 假设的串口库头文件

// 寄存器地址定义
constexpr uint16_t REG_ERR_CODE = 0x0000;   // 系统状态代码
constexpr uint16_t REG_BAT_VLTG = 0x0006;   // 输入电压
constexpr uint16_t REG_ADDRESS = 0x0010;    // 模块地址
constexpr uint16_t REG_OFFSET = 0x0012;     // 模块测量结果偏移量
constexpr uint16_t REG_MEA_START = 0x0020;  // 初始测量
constexpr uint16_t REG_MEA_RESULT = 0x0022; // 测量结果
constexpr uint16_t REG_CTRL_LD = 0x01BE;    // 激光二极管控制

// 命令头定义
constexpr uint8_t CMD_HEAD = 0xAA;        // 常规命令头
constexpr uint8_t ERROR_HEAD = 0xEE;      // 错误命令头
constexpr uint8_t BAUD_RATE_DETECT = 0x55; // 波特率检测命令
constexpr uint8_t STOP_CONTINUOUS = 0x58;  // 停止连续测量命令 'X'

// 从机地址
constexpr uint8_t DEFAULT_ADDRESS = 0x00;  // 默认地址
constexpr uint8_t BROADCAST_ADDRESS = 0x7F; // 广播地址

class LaserRangeFinder {
private:
    std::shared_ptr<SerialPort> serial_port_ptr_ = nullptr;
    uint8_t moduleAddress;

public:
    LaserRangeFinder(const std::string& portName, uint8_t address = DEFAULT_ADDRESS)
        : moduleAddress(address) {
        // 初始化串口
	serial_port_ptr_ = std::make_shared<SerialPort>(portName, 19200);
    }

    ~LaserRangeFinder() {
    }

    // 自动检测波特率
    bool detectBaudRate() {
        // 发送0x55进行波特率自动检测
        uint8_t cmd = BAUD_RATE_DETECT;
        serial_port_ptr_->write2(&cmd, 1);

        // 等待响应
        uint8_t response;
        if (serial_port_ptr_->read2(&response, 1, 1000)) {
            moduleAddress = response;
            std::cout << "模块地址: 0x" << std::hex << (int)moduleAddress << std::dec << std::endl;
            return true;
        }
        return false;
    }
private:


    void printByteVectorHex(const std::vector<uint8_t>& data, const std::string& label = "") {
    if (!label.empty()) {
        std::cout << label << ": ";
    }

    for (uint8_t byte : data) {
        std::cout << "0x"
                  << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(byte) << " ";
    }

    std::cout << std::dec << std::endl;
    }

    // 计算校验和
    uint8_t calculateChecksum(const std::vector<uint8_t>& data, size_t start, size_t length) {
        uint8_t sum = 0;
        for (size_t i = start; i < start + length; i++) {
            sum += data[i];
        }
        return sum;
    }

    // 构建命令包
    std::vector<uint8_t> buildCommand(bool isRead, uint16_t regAddress,
                                      const std::vector<uint8_t>& payload = std::vector<uint8_t>()) {
        std::vector<uint8_t> cmd;
        cmd.push_back(CMD_HEAD); // 命令头 0xAA

        // R/W位与地址
        uint8_t rwAddress = moduleAddress & 0x7F; // 确保地址只有7位
        if (isRead) {
            rwAddress |= 0x80; // 读取命令，第7位设为1
        }
        cmd.push_back(rwAddress);

        // 寄存器地址，高8位在前，低8位在后
        cmd.push_back((regAddress >> 8) & 0xFF);
        cmd.push_back(regAddress & 0xFF);

        // 如果是写入命令，添加有效载荷
        if (!isRead && !payload.empty()) {
            // 有效载荷计数，高8位在前，低8位在后
            cmd.push_back(0x00);
            cmd.push_back(payload.size() & 0xFF);

            // 添加有效载荷数据
            for (const auto& byte : payload) {
                cmd.push_back(byte);
            }
        }

        // 计算并添加校验和
        uint8_t checksum = calculateChecksum(cmd, 1, cmd.size() - 1);
        cmd.push_back(checksum);

        return cmd;
    }

    // 发送命令并接收响应
    std::vector<uint8_t> sendCommand(const std::vector<uint8_t>& cmd, int expectedResponseSize = 0) {
        // 发送命令
        serial_port_ptr_->write2(cmd.data(), cmd.size());

        // 如果不需要接收响应，直接返回
        if (expectedResponseSize <= 0) {
            return std::vector<uint8_t>();
        }

        // 接收响应
        std::vector<uint8_t> response(expectedResponseSize);
        int bytesRead = serial_port_ptr_->read2(response.data(), expectedResponseSize, 1000);

        if (bytesRead != expectedResponseSize) {
            std::cerr << "接收响应数据不完整" << std::endl;
            return std::vector<uint8_t>();
        }

        return response;
    }

public:
    // 读取模块最新状态
    uint16_t readStatus() {
        auto cmd = buildCommand(true, REG_ERR_CODE);
        auto response = sendCommand(cmd, 9);

        if (response.size() < 9 || response[0] != CMD_HEAD) {
            std::cerr << "读取状态失败" << std::endl;
            return 0xFFFF;
        }

        // 状态码在响应的第7和第8个字节
        return (response[7] << 8) | response[8];
    }

    uint16_t readHardwareVersion() {
        auto cmd = buildCommand(true, 0x000A);
        auto response = sendCommand(cmd, 9);

        if (response.size() < 9 || response[0] != CMD_HEAD) {
            std::cerr << "读取硬件版本失败" << std::endl;
            return 0;
        }

        // 硬件版本在响应的第6和第7个字节
        return (response[6] << 8) | response[7];
    }

    // 读取软件版本
    uint16_t readSoftwareVersion() {
        auto cmd = buildCommand(true, 0x000C);
        auto response = sendCommand(cmd, 9);

        if (response.size() < 9 || response[0] != CMD_HEAD) {
            std::cerr << "读取软件版本失败" << std::endl;
            return 0;
        }

        // 软件版本在响应的第6和第7个字节
        return (response[6] << 8) | response[7];
    }

    // 读取模块序列号
    uint16_t readSerialNumber() {
        auto cmd = buildCommand(true, 0x000E);
        auto response = sendCommand(cmd, 9);

        if (response.size() < 9 || response[0] != CMD_HEAD) {
            std::cerr << "读取序列号失败" << std::endl;
            return 0;
        }

        // 序列号在响应的第6和第7个字节
        return (response[6] << 8) | response[7];
    }

    // 读取输入电压
    uint16_t readInputVoltage() {
        auto cmd = buildCommand(true, REG_BAT_VLTG);
        auto response = sendCommand(cmd, 9);

        if (response.size() < 9 || response[0] != CMD_HEAD) {
            std::cerr << "读取输入电压失败" << std::endl;
            return 0;
        }

        // 电压值在响应的第6和第7个字节 (BCD编码，如0x3219表示3219mV)
        return (response[6] << 8) | response[7];
    }

    // 设置模块地址
    bool setModuleAddress(uint8_t newAddress) {
        if (newAddress == BROADCAST_ADDRESS) {
            std::cerr << "不能将模块地址设置为广播地址0x7F" << std::endl;
            return false;
        }

        std::vector<uint8_t> payload = {0x00, newAddress & 0x7F};
        auto cmd = buildCommand(false, REG_ADDRESS, payload);
        auto response = sendCommand(cmd, 9);

        if (response.size() < 9 || response[0] != CMD_HEAD) {
            std::cerr << "设置模块地址失败" << std::endl;
            return false;
        }

        moduleAddress = newAddress & 0x7F;
        return true;
    }

              // 设置测量偏移
    bool setMeasurementOffset(int16_t offset) {
        std::vector<uint8_t> payload = {
            static_cast<uint8_t>((offset >> 8) & 0xFF),
            static_cast<uint8_t>(offset & 0xFF)
        };

        auto cmd = buildCommand(false, REG_OFFSET, payload);
        auto response = sendCommand(cmd, 9);

        if (response.size() < 9 || response[0] != CMD_HEAD) {
            std::cerr << "设置测量偏移失败" << std::endl;
            return false;
        }

        return true;
    }

    // 控制激光开关
    bool setLaser(bool on) {
        std::vector<uint8_t> payload = {0x00, on ? 0x01 : 0x00};
        auto cmd = buildCommand(false, REG_CTRL_LD, payload);
        auto response = sendCommand(cmd, 9);

        if (response.size() < 9 || response[0] != CMD_HEAD) {
            std::cerr << "控制激光" << (on ? "开启" : "关闭") << "失败" << std::endl;
            return false;
        }

        return true;
    }

    // 读取测量结果
    struct MeasurementResult {
        uint32_t distance; // 单位：毫米
        uint16_t signalQuality; // 信号质量，越小越好
        bool valid;
    };

    MeasurementResult readMeasurementResult() {
        auto cmd = buildCommand(true, REG_MEA_RESULT);
        auto response = sendCommand(cmd, 12);

        MeasurementResult result = {0, 0, false};

        if (response.size() < 12 || response[0] != CMD_HEAD) {
            std::cerr << "读取测量结果失败" << std::endl;
            return result;
        }

        // 距离值在响应的第6-9个字节
        result.distance = (response[6] << 24) | (response[7] << 16) |
                          (response[8] << 8) | response[9];

        // 信号质量在响应的第10-11个字节
        result.signalQuality = (response[10] << 8) | response[11];

        result.valid = true;
        return result;
    }

    // 执行单次测量（自动模式）
    MeasurementResult performSingleMeasurement() {
        std::vector<uint8_t> payload = {0x00, 0x00};
        auto cmd = buildCommand(false, REG_MEA_START, payload);
        auto response = sendCommand(cmd, 12);

        MeasurementResult result = {0, 0, false};

        if (response.size() < 12 || response[0] != CMD_HEAD) {
            std::cerr << "执行单次测量失败" << std::endl;
            return result;
        }

        // 解析测量结果
        result.distance = (response[6] << 24) | (response[7] << 16) |
                          (response[8] << 8) | response[9];
        result.signalQuality = (response[10] << 8) | response[11];
        result.valid = true;

        return result;
    }

    // 执行单次慢速测量
    MeasurementResult performSingleSlowMeasurement() {
        std::vector<uint8_t> payload = {0x00, 0x01};
        auto cmd = buildCommand(false, REG_MEA_START, payload);
        auto response = sendCommand(cmd, 12);

        MeasurementResult result = {0, 0, false};

        if (response.size() < 12 || response[0] != CMD_HEAD) {
            std::cerr << "执行单次慢速测量失败" << std::endl;
            return result;
        }

        // 解析测量结果
        result.distance = (response[6] << 24) | (response[7] << 16) |
                          (response[8] << 8) | response[9];
        result.signalQuality = (response[10] << 8) | response[11];
        result.valid = true;

        return result;
    }

    // 执行单次快速测量
    MeasurementResult performSingleFastMeasurement() {
        std::vector<uint8_t> payload = {0x00, 0x02};
        auto cmd = buildCommand(false, REG_MEA_START, payload);
        auto response = sendCommand(cmd, 12);

        MeasurementResult result = {0, 0, false};

        if (response.size() < 12 || response[0] != CMD_HEAD) {
            std::cerr << "执行单次快速测量失败" << std::endl;
            return result;
        }

        // 解析测量结果
        result.distance = (response[6] << 24) | (response[7] << 16) |
                          (response[8] << 8) | response[9];
        result.signalQuality = (response[10] << 8) | response[11];
        result.valid = true;

        return result;
    }

    // 开始连续测量（自动模式）
    bool startContinuousMeasurement(std::function<void(MeasurementResult)> callback, bool& stopFlag) {
        std::vector<uint8_t> payload = {0x00, 0x04};
        auto cmd = buildCommand(false, REG_MEA_START, payload);
        sendCommand(cmd, 0);

        // 在单独的线程中接收连续测量数据
        std::thread([this, callback, &stopFlag]() {
            while (!stopFlag) {
                std::vector<uint8_t> response(12);
                int bytesRead = serial_port_ptr_->read2(response.data(), 12, 1000);

                if (bytesRead == 12 && response[0] == CMD_HEAD) {
                    MeasurementResult result;
                    result.distance = (response[6] << 24) | (response[7] << 16) |
                                      (response[8] << 8) | response[9];
                    result.signalQuality = (response[10] << 8) | response[11];
                    result.valid = true;
		    if(callback)
                    	callback(result);
                }

                // 小延迟避免CPU占用过高
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
            }
        }).detach();

        return true;
    }

    // 开始连续慢速测量
    bool startContinuousSlowMeasurement(std::function<void(MeasurementResult)> callback, bool& stopFlag) {
        std::vector<uint8_t> payload = {0x00, 0x05};
        auto cmd = buildCommand(false, REG_MEA_START, payload);
        sendCommand(cmd, 0);

        // 在单独的线程中接收连续测量数据
        std::thread([this, callback, &stopFlag]() {
            while (!stopFlag) {
                std::vector<uint8_t> response(12);
                int bytesRead = serial_port_ptr_->read2(response.data(), 12, 1000);

                if (bytesRead == 12 && response[0] == CMD_HEAD) {
                    MeasurementResult result;
                    result.distance = (response[6] << 24) | (response[7] << 16) |
                                      (response[8] << 8) | response[9];
                    result.signalQuality = (response[10] << 8) | response[11];
                    result.valid = true;
		    if(callback)
                    	callback(result);
                }

                // 小延迟避免CPU占用过高
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
            }
        }).detach();

        return true;
    }

    // 开始连续快速测量
    bool startContinuousFastMeasurement(std::function<void(MeasurementResult)> callback, bool& stopFlag) {
        std::vector<uint8_t> payload = {0x00, 0x06};
        auto cmd = buildCommand(false, REG_MEA_START, payload);
        sendCommand(cmd, 0);
	printByteVectorHex(cmd,"send msg");
        // 在单独的线程中接收连续测量数据
        std::thread([this, callback, &stopFlag]() {
            while (!stopFlag) {
                std::vector<uint8_t> response(12);
                int bytesRead = serial_port_ptr_->read2(response.data(), 12, 1000);
		printByteVectorHex(response,"recv msg");
                if (bytesRead == 12 && response[0] == CMD_HEAD) {
                    MeasurementResult result;
                    result.distance = (response[6] << 24) | (response[7] << 16) |
                                      (response[8] << 8) | response[9];
                    result.signalQuality = (response[10] << 8) | response[11];
                    result.valid = true;
		    if(callback)
                    	callback(result);
                }

                // 小延迟避免CPU占用过高
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
            }
        }).detach();

        return true;
    }

    // 停止连续测量
    bool stopContinuousMeasurement() {
        uint8_t stopCmd = STOP_CONTINUOUS; // 0x58 'X'
        serial_port_ptr_->write2(&stopCmd, 1);
        return true;
    }

    // 启动多从机同时测量（广播命令）
    bool startMultiDeviceMeasurement() {
        uint8_t originalAddress = moduleAddress;
        moduleAddress = BROADCAST_ADDRESS; // 设置为广播地址

        std::vector<uint8_t> payload = {0x00, 0x00}; // 自动模式
        auto cmd = buildCommand(false, REG_MEA_START, payload);
        sendCommand(cmd, 0); // 发送命令但不期望响应

        moduleAddress = originalAddress; // 恢复原地址
        return true;
    }

     // 解析错误状态码
    std::string parseStatusCode(uint16_t statusCode) {
        switch (statusCode) {
            case 0x0000: return "无错误";
            case 0x0001: return "输入电压过低，应大于等于2.2V";
            case 0x0002: return "内部错误，可忽略";
            case 0x0003: return "模块温度过低(<-20℃)";
            case 0x0004: return "模块温度过高(>+40℃)";
            case 0x0005: return "测量目标超出量程";
            case 0x0006: return "无效测量结果";
            case 0x0007: return "背景光太强";
            case 0x0008: return "反射信号太弱";
            case 0x0009: return "反射信号太强";
            case 0x000A: return "硬件错误1";
            case 0x000B: return "硬件错误2";
            case 0x000C: return "硬件错误3";
            case 0x000D: return "硬件错误4";
            case 0x000E: return "硬件错误5";
            case 0x000F: return "反射信号不稳定";
            case 0x0010: return "硬件错误6";
            case 0x0011: return "硬件错误7";
            case 0x0081: return "无效结构";
            default: return "未知错误代码: 0x" + std::to_string(statusCode);
        }
    }
};
