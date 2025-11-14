

#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <chrono>
#include <queue>
#include <vector>
#include <memory>
#include <condition_variable>
#include <cstring> // For strerror

// Linux/POSIX headers
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> // For close
#include <cerrno>   // For errno
#include <netdb.h>

#include <nlohmann/json.hpp>
#include "common.h"


using json = nlohmann::json;

class TCPClient {
public:
    // 回调函数类型定义
    using DataCallback = std::function<void(const std::vector<uint8_t>& data)>;
    using ConnectionCallback = std::function<void(bool connected)>;

    TCPClient(const std::string& serverIP = "192.168.110.196", uint16_t serverPort = 55555,
              const std::string& client_id = "123456789",
              int reconnectInterval = 5000, int maxReconnectAttempts = -1)
        : serverIP_(serverIP),
        serverPort_(serverPort),
        client_id_(client_id),
        reconnectInterval_(reconnectInterval),
        maxReconnectAttempts_(maxReconnectAttempts),
        socket_(-1), // INVALID_SOCKET on Linux is -1
        running_(false),
        connected_(false),
        reconnectAttempts_(0),
        dataCallback_(nullptr),
        connectionCallback_(nullptr)
    {
    }

    ~TCPClient() {
        disconnect();
    }

    void setDataCallback(DataCallback callback) {
        dataCallback_ = callback;
    }

    void setConnectionCallback(ConnectionCallback callback) {
        connectionCallback_ = callback;
    }

    /**
     * @brief 启动客户端，开始连接和I/O线程
     */
    bool connect() {
        if (running_.exchange(true)) { // Atomically set to true
            return connected_;
        }
		std::string ip_ = "";
		int port_ = -1;
		Common::getUdpIpAndPort(ip_, port_);
		if(ip_.length() > 0) serverIP_ = ip_;		
        reconnectAttempts_ = 0;
		printf("tcp service ip is = %s\n",serverIP_.c_str());
        // 启动所有三个后台线程
        connectThread_ = std::thread(&TCPClient::connectLoop, this);
        receiveThread_ = std::thread(&TCPClient::receiveLoop, this);
        sendThread_ = std::thread(&TCPClient::sendLoop, this);

        return true;
    }

    /**
     * @brief 停止客户端，断开连接并清理线程
     */
    void disconnect() {
        if (!running_.exchange(false)) { // Atomically set to false
            return;
        }

        std::cout << "Disconnecting..." << std::endl;

        // 唤醒所有可能在等待的线程
        connectCv_.notify_one();
        sendCv_.notify_one();

        // 关闭套接字以中断阻塞的 recv/select
        if (socket_ != -1) {
            // SHUT_RDWR 会使 recv 和 send 立即失败
            ::shutdown(socket_, SHUT_RDWR);
            ::close(socket_);
            socket_ = -1;
        }

        // 等待所有线程安全退出
        if (connectThread_.joinable()) {
            connectThread_.join();
        }
        if (receiveThread_.joinable()) {
            receiveThread_.join();
        }
        if (sendThread_.joinable()) {
            sendThread_.join();
        }

        // 清理发送队列
        clearSendQueue();
        connected_ = false; // 确保状态为
        std::cout << "Disconnected." << std::endl;
    }

    /**
     * @brief 异步发送数据 (原始字节)
     * 将数据放入发送队列，由发送线程处理。
     * @return true 如果成功放入队列, false 如果客户端未运行或数据无效。
     */
    bool send(const uint8_t* data, int length) {
        if (!running_ || !connected_ || !data || length <= 0) {
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            sendQueue_.push(std::vector<uint8_t>(data, data + length));
        }
        sendCv_.notify_one(); // 唤醒发送线程
        return true;
    }

    /**
     * @brief 异步发送数据 (std::vector)
     */
    bool send(const std::vector<uint8_t>& data) {
        return send(data.data(), data.size());
    }

    /**
     * @brief 异步发送数据 (std::string)
     */
    bool send(const std::string& data) {
        return send(reinterpret_cast<const uint8_t*>(data.c_str()), data.length());
    }

    bool isConnected() const {
        return connected_;
    }

    void setReconnectInterval(int milliseconds) {
        reconnectInterval_ = milliseconds;
    }

    void setMaxReconnectAttempts(int attempts) {
        maxReconnectAttempts_ = attempts;
    }

    // --- 应用层协议辅助函数 ---

    static std::string createResponse(const std::string& cmd, const json& data) {
        json response = {
            {"cmd", cmd},
            {"data", data}
        };
        return response.dump();
    }

    void Login() {
        json j{
            {"dev_id", client_id_}
        };
        auto r = createResponse("login", j);
        printf("Sending login message: %s\n", r.c_str());
        send(r);
    }

    void sendMsg(const std::string& comm_type, const std::string& cmd_type, const std::string& content, const std::string& des_id) {
        json j{
            {"src_id", client_id_},
            {"des_id", des_id},
            {"comm_type", comm_type},
            {"cmd_type", cmd_type},
            {"content", content}
        };
        auto r = createResponse("msg", j);
        printf("Sending message: %s\n", r.c_str());
        send(r);
    }

private:
    /**
     * @brief (线程) 负责连接与重连
     */
    void connectLoop() {
        while (running_) {
            {
                // 等待，直到被通知 (running_ = false) 或 (connected_ = false)
                std::unique_lock<std::mutex> lock(connectMutex_);
                connectCv_.wait(lock, [this] {
                    return !running_ || !connected_;
                });
            }
            
            if (!running_) break;

            // 此时我们 !connected_ 并且 running_，所以尝试连接
            if (attemptConnection()) {
                connected_ = true;
                reconnectAttempts_ = 0;
                if (connectionCallback_) {
                    connectionCallback_(true);
                }
                // (注意) Login 应该由用户在 connectionCallback(true) 中调用
                // 或者在这里自动调用
                // Login(); 
            }
            else {
                reconnectAttempts_++;
                if (maxReconnectAttempts_ > 0 && reconnectAttempts_ >= maxReconnectAttempts_) {
                    std::cerr << "达到最大重连次数，停止重连" << std::endl;
                    running_ = false; // 停止整个客户端
                    break;
                }

                std::cout << "连接失败，将在 " << reconnectInterval_ << " 毫秒后重试 (尝试 "
                    << reconnectAttempts_ << ")" << std::endl;
                
                // 非阻塞的休眠，以便 disconnect() 可以快速中断
                std::unique_lock<std::mutex> lock(connectMutex_);
                connectCv_.wait_for(lock, std::chrono::milliseconds(reconnectInterval_), [this]{ return !running_; });
            }
        }
        std::cout << "Connect thread exiting." << std::endl;
    }

    /**
     * @brief (线程) 负责接收数据
     */
    void receiveLoop() {
        const int BUFFER_SIZE = 4096;
        std::vector<uint8_t> buffer(BUFFER_SIZE);

        while (running_) {
            if (!connected_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(socket_, &readSet);

            timeval timeout;
            timeout.tv_sec = 1;  // 1秒超时
            timeout.tv_usec = 0;

            // 在Linux上，select的第一个参数是 max_fd + 1
            int result = select(socket_ + 1, &readSet, nullptr, nullptr, &timeout);

            if (!running_) break; // 检查是否在 select 期间被停止

            if (result == -1) { // SOCKET_ERROR
                if (errno == EBADF) { // Socket被关闭 (正常断开)
                    std::cout << "receiveLoop: Socket closed, exiting." << std::endl;
                } else {
                    std::cerr << "select 错误: " << strerror(errno) << std::endl;
                }
                handleDisconnect();
                continue;
            }
            else if (result == 0) { // 超时
                continue;
            }

            // 数据可读
            int bytesRead = recv(socket_, reinterpret_cast<char*>(buffer.data()), BUFFER_SIZE, 0);

            if (bytesRead > 0) {
                // 成功读取
                std::vector<uint8_t> receivedData(buffer.begin(), buffer.begin() + bytesRead);
                if (dataCallback_) {
                    dataCallback_(receivedData);
                }
            }
            else if (bytesRead == 0) {
                // 服务器关闭了连接
                std::cout << "服务器关闭了连接" << std::endl;
                handleDisconnect();
            }
            else {
                // 发生错误
                std::cerr << "接收数据错误: " << strerror(errno) << std::endl;
                handleDisconnect();
            }
        }
        std::cout << "Receive thread exiting." << std::endl;
    }

    /**
     * @brief (线程) 负责从队列中取出并发送数据
     */
    void sendLoop() {
        while (running_) {
            std::vector<uint8_t> data;
            {
                std::unique_lock<std::mutex> lock(queueMutex_);
                sendCv_.wait(lock, [this] {
                    return !running_ || !sendQueue_.empty();
                });

                if (!running_) break;

                data = std::move(sendQueue_.front());
                sendQueue_.pop();
            }
            
            if (!connected_) {
                std::cerr << "Not connected, dropping packet." << std::endl;
                continue; // 丢弃数据包，因为连接已断开
            }
            if (!internalSend(data.data(), data.size())) {
                // 发送失败，internalSend 内部会调用 handleDisconnect
                std::cerr << "Failed to send data, connection lost." << std::endl;
            }
        }
        std::cout << "Send thread exiting." << std::endl;
    }

    /**
     * @brief 尝试建立一个TCP连接
     */
    bool attemptConnection() {
        if (socket_ != -1) {
            ::close(socket_);
        }
        
        socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socket_ == -1) {
            std::cerr << "创建套接字失败: " << strerror(errno) << std::endl;
            return false;
        }

        // 设置发送和接收超时
        struct timeval timeout;
        timeout.tv_sec = 5; // 5秒
        timeout.tv_usec = 0;
        
        if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
            std::cerr << "设置接收超时失败: " << strerror(errno) << std::endl;
        }
        if (setsockopt(socket_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == -1) {
            std::cerr << "设置发送超时失败: " << strerror(errno) << std::endl;
        }

        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(serverPort_);
        inet_pton(AF_INET, serverIP_.c_str(), &serverAddr.sin_addr);

        if (::connect(socket_, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == -1) {
            std::cerr << "连接服务器失败: " << strerror(errno) << std::endl;
            ::close(socket_);
            socket_ = -1;
            return false;
        }

        std::cout << "成功连接到服务器: " << serverIP_ << ":" << serverPort_ << std::endl;
        return true;
    }

    /**
     * @brief 内部发送函数，处理部分发送
     * @return true on success, false on failure (and triggers disconnect)
     */
    bool internalSend(const uint8_t* data, int length) {
        int totalSent = 0;
        while (totalSent < length) {
            int bytesSent = ::send(socket_, reinterpret_cast<const char*>(data + totalSent), length - totalSent, 0);

            if (bytesSent == -1) { // SOCKET_ERROR
                int error = errno;
                // EPIPE 和 ECONNRESET 是明确的断开连接
                if (error == EPIPE || error == ECONNRESET) {
                    std::cerr << "Send error (connection lost): " << strerror(error) << std::endl;
                    handleDisconnect();
                    return false;
                }
                // EAGAIN/EWOULDBLOCK 表示缓冲区已满 (我们设置了超时，所以不应该卡住)
                else if (error == EAGAIN || error == EWOULDBLOCK) {
                     std::cerr << "Send buffer full, retrying..." << std::endl;
                     std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 稍作等待
                     continue;
                }
                else {
                    std::cerr << "Send error: " << strerror(error) << std::endl;
                    handleDisconnect();
                    return false;
                }
            }
            totalSent += bytesSent;
        }
        return true;
    }

    /**
     * @brief 统一处理断开连接，确保线程安全且只执行一次
     */
    void handleDisconnect() {
        bool expected_true = true;
        if (connected_.compare_exchange_strong(expected_true, false)) {
            std::cout << "Handling disconnect..." << std::endl;
            ::close(socket_);
            socket_ = -1;

            if (connectionCallback_) {
                connectionCallback_(false);
            }
            clearSendQueue();
            connectCv_.notify_one();
        }
    }

    void clearSendQueue() {
        std::lock_guard<std::mutex> lock(queueMutex_);
        std::queue<std::vector<uint8_t>> empty;
        std::swap(sendQueue_, empty);
    }

private:
    std::string serverIP_;
    uint16_t serverPort_;
    std::string client_id_;
    int reconnectInterval_;
    int maxReconnectAttempts_;
    int reconnectAttempts_;
    int socket_; // Linux socket 描述符
    std::atomic<bool> running_;
    std::atomic<bool> connected_;
    std::thread connectThread_;
    std::thread receiveThread_;
    std::thread sendThread_;
    DataCallback dataCallback_;
    ConnectionCallback connectionCallback_;
    std::queue<std::vector<uint8_t>> sendQueue_;
    std::mutex queueMutex_;
    std::condition_variable sendCv_;
    std::mutex connectMutex_;
    std::condition_variable connectCv_;
};

#endif // TCP_CLIENT_H

