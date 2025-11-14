#pragma once
#include <iostream>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <mutex>
#include <thread>
#include <chrono>
class SerialPort {
private:
    int fd;  
    std::string port_name;
    int baud_rate;
    std::mutex mutex_;
    bool debug_ = true;

public:
    SerialPort(const std::string& port, int baud = 19200) : port_name(port), baud_rate(baud) {
        fd = open(port.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
        if (fd == -1) {
            std::cerr << "❌ 无法打开串口 " << port << std::endl;
            exit(EXIT_FAILURE);
        }
        configurePort();
    }

#if 0

    void configurePort() {
        struct termios options;
        // 读取现有的串口配置
        if (tcgetattr(fd, &options) != 0) {
            perror("❌ tcgetattr 错误");
            return;
        }

        // 设置波特率
        cfsetispeed(&options, baud_rate);
        cfsetospeed(&options, baud_rate);

        // 设置控制选项
        options.c_cflag &= ~PARENB;   // 禁用奇偶校验
        options.c_cflag &= ~CSTOPB;   // 设置1位停止位
        options.c_cflag &= ~CSIZE;    // 清除数据位掩码
        options.c_cflag |= CS8;       // 设置8位数据位
        options.c_cflag |= CREAD;     // 使能接收
        options.c_cflag |= CLOCAL;    // 忽略调制解调器控制线

        // 设置本地模式选项 (Raw Mode)
        options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // 禁用标准模式、回显和信号字符

        // 设置输入模式选项
        options.c_iflag &= ~(IXON | IXOFF | IXANY); // 禁用软件流控
        options.c_iflag &= ~(ICRNL | INLCR); // 防止回车和换行符的转换

        // 设置输出模式选项 (Raw Mode)
        options.c_oflag &= ~OPOST;

        // --- 优化点 2: 设置 VMIN 和 VTIME 实现读取超时 ---
        // VMIN = 0, VTIME > 0:  读取超时设置。
        // read() 将等待 VTIME * 0.1 秒。如果期间有任何数据到达，read()会立即返回收到的数据。
        // 如果超时，read() 将返回 0。这比非阻塞模式返回-1更稳定。
        options.c_cc[VMIN] = 0;
        options.c_cc[VTIME] = 5; // 设置超时时间为 5 * 0.1 = 0.5 秒

        // 清空串口缓冲区
        tcflush(fd, TCIFLUSH);

        // 应用新的配置
        if (tcsetattr(fd, TCSANOW, &options) != 0) {
            perror("❌ tcsetattr 错误");
        }
    }
#else
    void configurePort() {
        struct termios options;
        tcgetattr(fd, &options);
	printf("baud = %d\n",baud_rate);
        cfsetispeed(&options, baud_rate);
        cfsetospeed(&options, baud_rate);
        options.c_cflag &= ~PARENB;
        options.c_cflag &= ~CSTOPB;
        options.c_cflag &= ~CSIZE;
        options.c_cflag |= CS8;
        options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
        options.c_iflag &= ~(IXON | IXOFF | IXANY);
        options.c_oflag &= ~OPOST;
        tcsetattr(fd, TCSANOW, &options);
    }
#endif
    std::vector<uint8_t> sendData2(const std::vector<uint8_t>& data){
    	std::vector<uint8_t> ret;
	std::lock_guard<std::mutex> lock(mutex_);
	if(sendData(data)){
	  ret = receiveData(50);
	}
	return ret;
    }

    int write2(const uint8_t *buffer, int size) {
       auto ret = write(fd, buffer, size);
       printf("send data size = %d\n", ret);
       return ret;
    }
    int read2(uint8_t *buffer, int size, int timeout_ms) {
    	std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms / 10));
	auto bytes = read(fd, buffer, size);
	printf("recv data size = %d\n",bytes);
	return bytes;
    }

    int sendData(const std::vector<uint8_t>& data,int time = 500) {
        auto ret = write(fd, data.data(), data.size());
	if(debug_){
	  printf("send data size = %d\n",ret);
	  for(const auto item : data){
	    printf("%02X ", item);
	  }
	  printf("\n");
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(time));
	return ret;
    }

    std::vector<uint8_t> receiveData(size_t size, uint64_t time = 500) {
        std::vector<uint8_t> buffer(size);
	std::this_thread::sleep_for(std::chrono::milliseconds(time));
        int bytesRead = read(fd, buffer.data(), size);
        if (bytesRead > 0) {
            buffer.resize(bytesRead);
        } else {
            buffer.clear();
        }
	if(debug_){
	  printf("recv data size = %d\n",bytesRead);
	  for(const auto item : buffer){
	    printf("%02X ", item);
	  }
	  printf("\n");
	}
        return buffer;
    }

    // 关闭串口
    ~SerialPort() {
        close(fd);
    }
};


