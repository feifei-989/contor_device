#pragma once

#include <modbus/modbus.h>
#include <iostream>
#include <vector>
#include <cstring>
#include <string>
#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>
#include <mutex>


class ModbusProtocol{
  public:
      ModbusProtocol();
      ~ModbusProtocol();
      int start(const std::string &ip = "192.168.1.10");
      //int start(const std::string &ip = "192.168.110.122");
      void stop();
      //细分指令
      void OneAxisSubdivisionCommand();
      void TwoAxisSubdivisionCommand();
      void OneAxisSubdivisionCommand2(const std::string &value = "");
      void TwoAxisSubdivisionCommand2(const std::string &value = "");
      //螺距指令
      void OneAxisPitchCommand();
      void TwoAxisPitchCommand();
      void OneAxisPitchCommand2(const std::string &value = "");
      void TwoAxisPitchCommand2(const std::string &value = "");
      //减速比
      void OneAxisReductionRatioCommand();
      void TwoAxisReductionRatioCommand();
      void OneAxisReductionRatioCommand2(const std::string &value = "");
      void TwoAxisReductionRatioCommand2(const std::string &value = "");
      //加速时间
      void OneAxisSpeedTimeCommand();
      void TwoAxisSpeedTimeCommand();
      void OneAxisSpeedTimeCommand2(const std::string &value = "");
      void TwoAxisSpeedTimeCommand2(const std::string &value = "");
      //减速时间
      void OneAxisDecelerateTimeCommand();
      void TwoAxisDecelerateTimeCommand();
      void OneAxisDecelerateTimeCommand2(const std::string &value = "");
      void TwoAxisDecelerateTimeCommand2(const std::string &value = "");
      //设定长度
      void OneAxisLengthCommand();
      void TwoAxisLengthCommand();
      void OneAxisLengthCommand2(const std::string &value = "");
      void TwoAxisLengthCommand2(const std::string &value = "");
      //运行速度
      void OneAxisRunnSpeedCommand();
      void TwoAxisRunnSpeedCommand();
      void OneAxisRunnSpeedCommand2(const std::string &value = "");
      void TwoAxisRunnSpeedCommand2(const std::string &value = "");
      //回原点速度
      void OneAxisReturnOriginSpeedCommand();
      void TwoAxisReturnOriginSpeedCommand();
      //回原点
      void OneAxisBackOrigin();
      void TwoAxisBackOrigin();
      //坐标清零
      void OneAxisCoordinatesClear();
      void TwoAxisCoordinatesClear();
      //当前坐标
      uint16_t OneAxisCurrentCoordinates();
      uint16_t TwoAxisCurrentCoordinates();
      //系统急停
      void SystemEmergencyStop();
      //
      void readData(int type = 0);


  private:
      uint16_t packData(uint8_t type, uint8_t addr, uint8_t func, uint8_t value0, uint8_t value1, bool flag = false);
      void packRegisterData(uint8_t addr, uint8_t func, uint8_t value0, uint8_t value1);
      void packSingleCoilData(uint8_t addr, uint8_t func, uint8_t value0, uint8_t value1);
      void packReadRegisterData(uint8_t addr, uint8_t func, uint8_t value0, uint8_t value1);
      uint16_t calculateCRC(const std::vector<uint8_t> &data);
      bool parseAndValidate(const uint8_t *data,int size);
      std::vector<unsigned char> stringToBytes(const std::string& str);
      void packData2(uint8_t type, uint8_t addr, uint8_t func, uint8_t value0, uint8_t value1, uint8_t addr1, uint8_t func1, uint8_t v2, uint8_t v3);
  private:
	std::string server_ip_ = "192.168.110.122"; 
	modbus_t *ctx_ = nullptr;
	bool init_ = false;
	std::mutex mtx_;
};


