
#pragma once
#include <string>
#include <thread>
#include <functional>
#include <atomic>
#include <mutex>
#include <cstdint>
#include "protocol_process.h"
class YX20KController {
public:
	struct JogCommand {
    	std::string cmd_tpye;
    	std::string axis;
    	int dir = 0;
    	float gradient = 0.0f;
    	int speed = 0;
	};
    using MovementCallback = std::function<void(bool success, const std::string &msg)>;
    struct Position { double x{0}, y{0}, z{0}, c{0}; };
    struct IOStatus { uint32_t inputs{0}; };
    enum class Dir { Positive, Negative };
    explicit YX20KController(std::string device = "/dev/ttyUSB0",
                             int         baud   = 115200);
    ~YX20KController();                                  
    bool open();
    void close();
    bool isOpen() const;
    bool moveAbsolute(double x, double y, double z, double c, int feed);
    bool moveAbsolute2(double x, double y, double z, double c, int feed, MovementCallback cb = nullptr);
    bool setPosition (double x, double y, double z, double c);      
    bool homeX(Dir d = Dir::Positive);                               
    bool homeY(Dir d = Dir::Positive);                                
    bool homeZ(Dir d = Dir::Positive);                                
    bool homeC(Dir d = Dir::Positive);                               
    bool queryPosition(Position& pos, int timeout_ms = 300); 
    bool queryInputs  (IOStatus& io , int timeout_ms = 300); 
    void startPositionPolling(int interval_ms,
                              std::function<void(const Position&)> cb);
    void stopPositionPolling();
    bool moveAxis(const std::string &axis, double position, int feed);
    bool moveAxis2(const std::string &axis, double position, int feed);
    bool moveAbsolute2(const std::string &axis, double position, int feed);
	int  cameraMove(std::string &axis, int dir, int feed = 5000);
	void updatePara(const JogCommand &para);
	bool areFloatsEqual(float a, float b, float epsilon = 1e-3f);
private:
    bool        writeCmd(const std::string& cmd, int timeout_ms = 300);
    std::string readLine(int timeout_ms);
    bool        readBytes(void* buf, std::size_t len, int timeout_ms);
    int 	writeAndRead(const std::string &cmd, uint8_t *buf, int len, int timeout_ms);
    void        pollingLoop(int interval_ms,
                            std::function<void(const Position&)> cb);
    bool isAlmostZero(float value, float epsilon = 1e-2f);
    bool isDefaultValue(double val);
	bool cameraMoveAbsolute(const std::string &axis, int dir, int feed);
    std::string dev_;
    int         baud_;
    int         fd_{-1};
    std::mutex  io_mtx_;
    std::thread       poll_th_;
    std::atomic<bool> polling_{false};
    MovementCallback current_callback_ = nullptr;
    double last_x_ = -80000.0, last_y_ = -80000.0, last_z_ = -80000.0, last_c_ = -80000.0; // Cache for last sent positions, using an unlikely value
    float move_x_ = -10000.0;
    float move_z_ = -10000.0;
    float move_c_ = -10000.0;
    bool move_flag_ = false;
    bool fisrt_start = false;
    std::string last_comm_;
    ProtocolProcess protocol_process_;
	bool camera_move_flag_ = true;
	float gradient_ = 0.1;
	JogCommand para_;
	float camera_move_y_ = -1.0;
	float camera_move_x_ = -1.0;
};


