#include <functional>
#include <thread>
#include "modbus_protocol.h"


ModbusProtocol::ModbusProtocol()
{

}

ModbusProtocol::~ModbusProtocol()
{
   stop();
}

int ModbusProtocol::start(const std::string &ip)
{
  int ret = -1;
   server_ip_ = ip;
   int server_port = 502;
   ctx_ = modbus_new_tcp(server_ip_.c_str(), server_port);
   if (ctx_ == nullptr) {
   	 std::cerr << "Failed to create MODBUS TCP context!" << std::endl;
	 return ret;
   }

   if (modbus_set_response_timeout(ctx_, 10, 500000) == -1) {
   	std::cerr << "Failed to set response timeout: " << modbus_strerror(errno) << std::endl;
	modbus_free(ctx_);
	return ret;
   }

   if (modbus_connect(ctx_) == -1){
     	std::cerr << "Connection to MODBUS TCP server failed: " << modbus_strerror(errno) << std::endl;
	modbus_free(ctx_);
	return ret;
   }
   //modbus_set_debug(ctx_,true);
   std::cout << "Connected to MODBUS TCP server sucess" << std::endl;
   init_ = true;
   ret = 1;
   return ret;
}

uint16_t ModbusProtocol::packData(uint8_t type, uint8_t addr, uint8_t func, uint8_t value0, uint8_t value1, bool flag)
{
   if(!init_)return 0;
   std::lock_guard<std::mutex> lk(mtx_);
   std::vector<uint8_t> vec;
   vec.push_back(0x01);
   vec.push_back(type);
   vec.push_back(addr);
   vec.push_back(func);
   vec.push_back(value0);
   vec.push_back(value1);
#if 1
   std::cout << "send packRegisterData : ";
   for (int i = 0; i < vec.size(); i++){
   	printf("%02X ", vec[i]);
   }
   std::cout << std::endl;
   auto crc = calculateCRC(vec);
   printf("crc low = %02X high = %02X\n",crc & 0xFF, (crc >> 8) & 0xFF);
#endif
   //std::lock_guard<std::mutex> lk(mtx_);
   if (modbus_send_raw_request(ctx_, vec.data(), vec.size()) == -1){
      std::cerr << "Failed to send request: " << modbus_strerror(errno) << std::endl;
      return 0;
   }
   uint8_t response[MODBUS_TCP_MAX_ADU_LENGTH];
   int response_length = modbus_receive_confirmation(ctx_, response);
   if (response_length == -1) {
   	std::cerr << "Failed to receive response: " << modbus_strerror(errno) << std::endl;
	return 0;
   }
#if 1
   std::cout << "Response received (" << response_length << " bytes): ";
   for (int i = 0; i < response_length; i++){
   	printf("%02X ", response[i]);
   }
   std::cout << std::endl;
   parseAndValidate(response,response_length);
#endif
   uint16_t v = 0;
   if(flag) {
   	v = response[9] << 8 | response[10];
   }
   std::this_thread::sleep_for(std::chrono::milliseconds(200));
   return v;
}



void ModbusProtocol::packData2(uint8_t type, uint8_t addr, uint8_t func, uint8_t value0, uint8_t value1, uint8_t addr1, uint8_t func1, uint8_t v2, uint8_t v3)
{
   if(!init_)return;
   std::vector<uint8_t> vec;
   vec.push_back(0x01);
   vec.push_back(type);
   vec.push_back(addr);
   vec.push_back(func);
   vec.push_back(value0);
   vec.push_back(value1);
   vec.push_back(addr1);
   vec.push_back(func1);
   vec.push_back(v2);
   vec.push_back(v3);
   std::cout << "send packRegisterData : ";
   for (int i = 0; i < vec.size(); i++){
   	printf("%02X ", vec[i]);
   }
   std::cout << std::endl;
   auto crc = calculateCRC(vec);
   printf("crc low = %02X high = %02X\n",crc & 0xFF, (crc >> 8) & 0xFF);
   std::lock_guard<std::mutex> lk(mtx_);
   if (modbus_send_raw_request(ctx_, vec.data(), vec.size()) == -1){
      std::cerr << "Failed to send request: " << modbus_strerror(errno) << std::endl;
      return;
   }
   uint8_t response[MODBUS_TCP_MAX_ADU_LENGTH];
   int response_length = modbus_receive_confirmation(ctx_, response);
   if (response_length == -1) {
   	std::cerr << "Failed to receive response: " << modbus_strerror(errno) << std::endl;
	return;
   }
   std::cout << "Response received (" << response_length << " bytes): ";
   for (int i = 0; i < response_length; i++){
   	printf("%02X ", response[i]);
   }
   std::cout << std::endl;
   parseAndValidate(response,response_length);
}

void ModbusProtocol::packRegisterData(uint8_t addr, uint8_t func, uint8_t value0, uint8_t value1)
{
   if(!init_)return;
   std::vector<uint8_t> vec;
   vec.push_back(0x01);
   vec.push_back(0x06);
   vec.push_back(addr);
   vec.push_back(func);
   vec.push_back(value0);
   vec.push_back(value1);
   std::cout << "send packRegisterData : ";
   for (int i = 0; i < vec.size(); i++){
   	printf("%02X ", vec[i]);
   }
   std::cout << std::endl;
   auto crc = calculateCRC(vec);
   printf("crc low = %02X high = %02X\n",crc & 0xFF, (crc >> 8) & 0xFF);
   std::lock_guard<std::mutex> lk(mtx_);
   if (modbus_send_raw_request(ctx_, vec.data(), vec.size()) == -1){
      std::cerr << "Failed to send request: " << modbus_strerror(errno) << std::endl;
      return;
   }
   uint8_t response[MODBUS_TCP_MAX_ADU_LENGTH];
   int response_length = modbus_receive_confirmation(ctx_, response);
   if (response_length == -1) {
   	std::cerr << "Failed to receive response: " << modbus_strerror(errno) << std::endl;
	return;
   }
   std::cout << "Response received (" << response_length << " bytes): ";
   for (int i = 0; i < response_length; i++){
   	printf("%02X ", response[i]);
   }
   std::cout << std::endl;
   parseAndValidate(response,response_length);
}

void ModbusProtocol::packSingleCoilData(uint8_t addr, uint8_t func, uint8_t value0, uint8_t value1)
{
   if(!init_)return;
   std::vector<uint8_t> vec;
   vec.push_back(0x01);
   vec.push_back(0x05);
   vec.push_back(addr);
   vec.push_back(func);
   vec.push_back(value0);
   vec.push_back(value1);
   std::cout << "send packSingleCoilData : ";
   for (int i = 0; i < vec.size(); i++){
   	printf("%02X ", vec[i]);
   }
   std::cout << std::endl;
   std::lock_guard<std::mutex> lk(mtx_);
   if (modbus_send_raw_request(ctx_, vec.data(), vec.size()) == -1){
      std::cerr << "Failed to send request: " << modbus_strerror(errno) << std::endl;
      return;
   }
   uint8_t response[MODBUS_TCP_MAX_ADU_LENGTH];
   int response_length = modbus_receive_confirmation(ctx_, response);
   if (response_length == -1) {
   	std::cerr << "Failed to receive response: " << modbus_strerror(errno) << std::endl;
	return;
   }
   std::cout << "Response received (" << response_length << " bytes): ";
   for (int i = 0; i < response_length; i++){
   	printf("%02X ", response[i]);
   }
   std::cout << std::endl;
}

void ModbusProtocol::packReadRegisterData(uint8_t addr, uint8_t func, uint8_t value0, uint8_t value1)
{
   if(!init_)return;
   std::vector<uint8_t> vec;
   vec.push_back(0x01);
   vec.push_back(0x03);
   vec.push_back(addr);
   vec.push_back(func);
   vec.push_back(value0);
   vec.push_back(value1);
   std::cout << "send packReadRegisterData : ";
   for (int i = 0; i < vec.size(); i++){
   	printf("%02X ", vec[i]);
   }
   std::cout << std::endl;
   auto crc = calculateCRC(vec);
   printf("crc is = %d\n",crc);
   std::lock_guard<std::mutex> lk(mtx_);
   if (modbus_send_raw_request(ctx_, vec.data(), vec.size()) == -1){
      std::cerr << "Failed to send request: " << modbus_strerror(errno) << std::endl;
      return;
   }
   uint8_t response[MODBUS_TCP_MAX_ADU_LENGTH];
   int response_length = modbus_receive_confirmation(ctx_, response);
   if (response_length == -1) {
   	std::cerr << "Failed to receive response: " << modbus_strerror(errno) << std::endl;
	return;
   }
   std::cout << "Response received (" << response_length << " bytes): ";
   for (int i = 0; i < response_length; i++){
   	printf("%02X ", response[i]);
   }
   std::cout << std::endl;
}

std::vector<unsigned char> ModbusProtocol::stringToBytes(const std::string& str) {
    int num = -1;
    try {
        num = std::stoi(str);
    } catch (...) {
        return {};
    }
    
    if (num < 0 || num > 65535) {
  	printf("number = %d  str is = %s\n",num, str.c_str());
        throw std::out_of_range("数字超出范围（0～65535）");
    }
    
    unsigned char highByte = static_cast<unsigned char>((num >> 8) & 0xFF);
    unsigned char lowByte = static_cast<unsigned char>(num & 0xFF);
    return std::vector<unsigned char>{ highByte, lowByte };
}


void ModbusProtocol::OneAxisSubdivisionCommand()
{
   //packRegisterData(0x01,0x4a,0x03,0xe8);
   packData(0x06,0x01,0x4a,0x03,0xe8);
}

void ModbusProtocol::TwoAxisSubdivisionCommand()
{
   //packRegisterData(0x01,0x54,0x03,0xe8);
   packData(0x06,0x01,0x54,0x03,0xe8);
}


void ModbusProtocol::OneAxisSubdivisionCommand2(const std::string &value)
{
   auto r = stringToBytes(value);
   if(r.size() == 2){
   	packRegisterData(0x01,0x4a, r[0],r[1]);
   }else{
   	printf("value is = %s error\n",value.c_str());
   }
}



void ModbusProtocol::TwoAxisSubdivisionCommand2(const std::string &value)
{
   auto r = stringToBytes(value);
   if(r.size() == 2){
   	packRegisterData(0x01,0x54, r[0],r[1]);
   }else{
   	printf("value is = %s error\n",value.c_str());
   }
}

void ModbusProtocol::OneAxisPitchCommand2(const std::string &value)
{
   auto r = stringToBytes(value);
   if(r.size() == 2){
   	packRegisterData(0x01,0x4b, r[0],r[1]);
   }else{
   	printf("value is = %s error\n",value.c_str());
   }
}

void ModbusProtocol::TwoAxisPitchCommand2(const std::string &value)
{
   auto r = stringToBytes(value);
   if(r.size() == 2){
   	packRegisterData(0x01,0x55, r[0],r[1]);
   }else{
   	printf("value is = %s error\n",value.c_str());
   }
}

void ModbusProtocol::OneAxisReductionRatioCommand2(const std::string &value)
{
   auto r = stringToBytes(value);
   if(r.size() == 2){
   	packRegisterData(0x01,0x4c, r[0],r[1]);
   }else{
   	printf("value is = %s error\n",value.c_str());
   }
}

void ModbusProtocol::TwoAxisReductionRatioCommand2(const std::string &value)
{
   auto r = stringToBytes(value);
   if(r.size() == 2){
   	packRegisterData(0x01,0x56, r[0],r[1]);
   }else{
   	printf("value is = %s error\n",value.c_str());
   }
}

void ModbusProtocol::OneAxisSpeedTimeCommand2(const std::string &value)
{
   auto r = stringToBytes(value);
   if(r.size() == 2){
   	packRegisterData(0x01,0x98, r[0],r[1]);
   }else{
   	printf("value is = %s error\n",value.c_str());
   }
}

void ModbusProtocol::TwoAxisSpeedTimeCommand2(const std::string &value)
{
   auto r = stringToBytes(value);
   if(r.size() == 2){
   	packRegisterData(0x01,0xa2, r[0],r[1]);
   }else{
   	printf("value is = %s error\n",value.c_str());
   }
}

void ModbusProtocol::OneAxisDecelerateTimeCommand2(const std::string &value)
{
   auto r = stringToBytes(value);
   if(r.size() == 2){
   	packRegisterData(0x01,0x99, r[0],r[1]);
   }else{
   	printf("value is = %s error\n",value.c_str());
   }
}

void ModbusProtocol::TwoAxisDecelerateTimeCommand2(const std::string &value)
{
   auto r = stringToBytes(value);
   if(r.size() == 2){
   	packRegisterData(0x01,0xa3, r[0],r[1]);
   }else{
   	printf("value is = %s error\n",value.c_str());
   }
}

void ModbusProtocol::OneAxisLengthCommand2(const std::string &value)
{
   auto r = stringToBytes(value);
   if(r.size() == 2){
	packData(0x06,0x01,0xb0,r[0],r[1]);
   }else{
   	printf("value is = %s error\n",value.c_str());
   }
}

void ModbusProtocol::TwoAxisLengthCommand2(const std::string &value)
{
   auto r = stringToBytes(value);
   if(r.size() == 2){
	packData(0x06,0x01,0xae,r[0],r[1]);
   }else{
   	printf("value is = %s error\n",value.c_str());
   }
}

void ModbusProtocol::OneAxisRunnSpeedCommand2(const std::string &value)
{
   auto r = stringToBytes(value);
   if(r.size() == 2){
   	packRegisterData(0x01,0xd8, r[0],r[1]);
   }else{
   	printf("value is = %s error\n",value.c_str());
   }
}

void ModbusProtocol::TwoAxisRunnSpeedCommand2(const std::string &value)
{
   auto r = stringToBytes(value);
   if(r.size() == 2){
   	packRegisterData(0x01,0xd6, r[0],r[1]);
   }else{
   	printf("value is = %s error\n",value.c_str());
   }
}

void ModbusProtocol::OneAxisPitchCommand()
{
   //packRegisterData(0x01,0x4b,0x00,0x7d);
   packData(0x06,0x01,0x4b,0x00,0x7d);
}

void ModbusProtocol::TwoAxisPitchCommand()
{
   //packRegisterData(0x01,0x55,0x00,0x7d);
   packData(0x06,0x01,0x55,0x00,0x7d);
}

void ModbusProtocol::OneAxisReductionRatioCommand()
{
   packRegisterData(0x01,0x4c,0x00,0x0a);
}

void ModbusProtocol::TwoAxisReductionRatioCommand()
{
   packRegisterData(0x01,0x56,0x00,0x0a);
}

void ModbusProtocol::OneAxisSpeedTimeCommand()
{
   packRegisterData(0x01,0x98,0x00,0x64);
}

void ModbusProtocol::TwoAxisSpeedTimeCommand()
{
   packRegisterData(0x01,0xa2,0x00,0x64);
}

void ModbusProtocol::OneAxisDecelerateTimeCommand()
{
   packRegisterData(0x01,0x99,0x00,0x64);
}

void ModbusProtocol::TwoAxisDecelerateTimeCommand()
{
   packRegisterData(0x01,0xa3,0x00,0x64);
}

void ModbusProtocol::OneAxisLengthCommand()
{
   //packData(0x06,0x01,0xb0,0x00,0x00);
   //packData(0x06,0x01,0xb0,0x13,0x88);
   //packData(0x06,0x01,0xb0,0x9c,0x40);
   //packData(0x06,0x01,0xb0,0x13,0x88);
   packData(0x06,0x01,0xb0,0x00,0x00);
   //packData(0x06,0x01,0xb1,0x00,0x00);
   //packData2(0x06,0x01,0xb0,0x86,0xa0,0x01,0xb1,0x00,0x01);
   //packData2(0x06,0x01,0xb1,0x00,0x01,0x01,0xb0,0x86,0xa0);
   //okpackData2(0x06,0x01,0xb0,0xa6,0xa0,0x01,0xb1,0x00,0x01);
   //packData2(0x06,0x01,0xb0,0x71,0x00,0x01,0xb1,0x00,0x02);
   //packData2(0x06,0x01,0xb1,0x00,0x02,0x01,0xb0,0xa6,0xa0);
   //packData2(0x06,0x01,0xb1,0x00,0x02,0x01,0xb0,0x71,0x00);
}

void ModbusProtocol::TwoAxisLengthCommand()
{
   //packRegisterData(0x01,0xb0,0x03,0xe8);
   //packData(0x06,0x01,0xae,0x1c,0x20);
   //packData(0x06,0x01,0xae,0x02,0xd0);
   //packData(0x06,0x01,0xae,0x0e,0x10);
   packData(0x06,0x01,0xae,0x07,0x08);
}  

void ModbusProtocol::OneAxisRunnSpeedCommand()
{
   //800
   //200
   packData(0x06,0x01,0xd8,0x06,0x40);
}

void ModbusProtocol::TwoAxisRunnSpeedCommand()
{
   //100
   packData(0x06,0x01,0xd6,0x01,0x2c);
}

void ModbusProtocol::OneAxisReturnOriginSpeedCommand()
{
   packRegisterData(0x01,0xc3,0x27,0x10);
}

void ModbusProtocol::TwoAxisReturnOriginSpeedCommand()
{
   packRegisterData(0x01,0xc5,0x27,0x10);
}

void ModbusProtocol::OneAxisBackOrigin()
{
   packSingleCoilData(0x01,0x5e,0xff,0x00);
}

void ModbusProtocol::TwoAxisBackOrigin()
{
   packSingleCoilData(0x01,0x5f,0xff,0x00);
}

void ModbusProtocol::OneAxisCoordinatesClear()
{
   packData(0x05,0x01,0x95,0xff,0x00);
}

void ModbusProtocol::TwoAxisCoordinatesClear()
{
   //packSingleCoilData(0x01,0x95,0xff,0x00);
   packData(0x05,0x01,0x94,0xff,0x00);
}

uint16_t ModbusProtocol::OneAxisCurrentCoordinates()
{
   return packData(0x03,0x01,0x1a,0x00,0x02,true);
   //packData(0x03,0x01,0x1b,0x00,0x02);
}

uint16_t ModbusProtocol::TwoAxisCurrentCoordinates()
{
   //packReadRegisterData(0x01,0x1a,0x00,0x02);
   return packData(0x03,0x01,0x18,0x00,0x02,true);
}

void ModbusProtocol::SystemEmergencyStop()
{
   //packSingleCoilData(0x01,0x64,0xff,0x00);
   //packRegisterData(0x01,0x64,0xff,0x00);
   //packData(0x05,0x01,0x64,0xff,0x00);
   packData(0x05,0x01,0x64,0x00,0x00);
}

void ModbusProtocol::readData(int type)
{
   if(0 == type){
   	packData(0x03,0x01,0xb0,0x00,0x02);
   	packData(0x03,0x01,0xb1,0x00,0x02);
   }
}

uint16_t ModbusProtocol::calculateCRC(const std::vector<uint8_t> &data)
{
    uint16_t crc = 0xFFFF; // 初始化 CRC 寄存器
    for (size_t i = 0; i < data.size(); i++) {
        crc ^= data[i]; // 将数据字节与 CRC 寄存器的低字节异或
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001; // 异或多项式
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

bool ModbusProtocol::parseAndValidate(const uint8_t *data,int size)
{
       // 检查数据长度
    if (size < 9) {
        std::cerr << "Data too short for validation!" << std::endl;
        return false;
    }
    // 判断第4和第5位置是否等于6
    uint16_t length = (data[4] << 8) | data[5];
    if (length != 6) {
        std::cerr << "Length mismatch: Expected 6, got " << length << std::endl;
        return false;
    }
    // 提取需要校验的部分（第6位到倒数第3位）
    std::vector<uint8_t> crcData(data + 6, data + 12);
    // 计算 CRC
   printf("crc value is : ");
   for (int i = 0; i < 6; i++){
   	printf("%02X ", crcData[i]);
   }
   printf("\n");
    uint16_t calculatedCRC = calculateCRC(crcData);
    // 提取数据中携带的 CRC
    uint16_t providedCRC = (data[size - 2]) | (data[size - 1] << 8);
    // 校验 CRC
    if (calculatedCRC != providedCRC) {
        std::cerr << "CRC mismatch: Calculated 0x" << std::hex << calculatedCRC
                  << ", Provided 0x" << providedCRC << std::endl;
        return false;
    }
    // 数据解析成功
    std::cout << "Data parsed and validated successfully!" << std::endl;
    return true;
}

void ModbusProtocol::stop()
{
   if(init_){
    	init_ = false;
	modbus_close(ctx_);
	modbus_free(ctx_);
   }
}
