#include "YX20KController.h"
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <sstream>
#include <iomanip>
#include <regex>
#include <chrono>
#include <thread>
#include <cmath>
namespace {
speed_t toBaud(int b) {
    switch (b) {
        case 115200: return B115200;
        case 57600:  return B57600;
        case 38400:  return B38400;
        case 19200:  return B19200;
        default:     return B9600;
    }
}
bool setupPort(int fd, int baud) {
    termios t{};
    if (tcgetattr(fd, &t)) return false;
    cfsetispeed(&t, baud);
    cfsetospeed(&t, baud);
    t.c_cflag &= ~(PARENB | CSTOPB | CSIZE | CRTSCTS);
    t.c_cflag |=  CS8 | CREAD | CLOCAL;
    t.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    t.c_iflag &= ~(IXON | IXOFF | IXANY);
    t.c_oflag &= ~OPOST;
    t.c_cc[VMIN]=0;  t.c_cc[VTIME]=1;  
    return tcsetattr(fd, TCSANOW, &t) == 0;
}
} 

YX20KController::YX20KController(std::string dev,int baud)
    : dev_(std::move(dev)), baud_(baud) {}
YX20KController::~YX20KController() {
    stopPositionPolling();
    close();
}

bool YX20KController::open() {
    if (fd_ != -1) return true;
    fd_ = ::open(dev_.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (fd_ == -1) return false;
    if (!setupPort(fd_, toBaud(baud_))) { close(); return false; }
    tcflush(fd_, TCIOFLUSH);
    return true;
}
void YX20KController::close() {
    if (fd_ != -1) { ::close(fd_); fd_=-1; }
}
bool YX20KController::isOpen() const { return fd_!=-1; }

bool YX20KController::writeCmd(const std::string& cmd, int timeout_ms) {
    int ret = -1;
    std::lock_guard<std::mutex> lk(io_mtx_);
    //printf("send cmd = %s\n",cmd.c_str());
    if (fd_==-1) return false;
    ret = ::write(fd_, cmd.data(), cmd.size());
    //printf("send msg = %s r = %d\n",cmd.c_str(),ret);
    if(ret > 0){
	uint8_t buf[512] = {0};
	int len = 512;
    	usleep(timeout_ms);
	ret = ::read(fd_,buf,len);
	//if(ret > 0) printf("recv data is = %s\n",buf);
    }
    if(ret > 0) return true;
    return false;
}

int YX20KController::writeAndRead(const std::string &cmd, uint8_t *buf, int len, int timeout_ms)
{
    int ret = -1;
    std::lock_guard<std::mutex> lk(io_mtx_);
    if (fd_==-1) return false;
    auto r = ::write(fd_, cmd.data(), cmd.size());
    //printf("send msg 00 = %s r = %d\n",cmd.c_str(),r);
    if(r > 0){
    	usleep(timeout_ms);
    	ret = ::read(fd_,buf,len);
	//if(ret > 0) printf("recv data is = %s\n",buf);
    }
    return ret;
}

void YX20KController::updatePara(const JogCommand &para)
{
	if(move_flag_) return;
	para_ = para;
	if(!areFloatsEqual(gradient_, para_.gradient)) gradient_ = para_.gradient;	
	cameraMoveAbsolute(para_.axis,para_.dir,para_.speed);
}

bool YX20KController::areFloatsEqual(float a, float b, float epsilon)
{
	return std::abs(a - b) <= epsilon;
}

std::string YX20KController::readLine(int timeout_ms) {
    std::lock_guard<std::mutex> lk(io_mtx_);
    if (fd_==-1) return {};
    usleep(timeout_ms);
    std::string s; char ch;
    while (read(fd_,&ch,1)==1){ if(ch=='\n'||ch=='\r') break; s+=ch; }
    return s;
}
bool YX20KController::readBytes(void* buf,std::size_t len,int timeout_ms){
    std::lock_guard<std::mutex> lk(io_mtx_);
    if (fd_==-1) return false;
    usleep(timeout_ms);
    return ::read(fd_,buf,len)==static_cast<ssize_t>(len);
}

bool YX20KController::moveAbsolute(double x,double y,double z,double c,int f){
    std::ostringstream oss;
    oss<<std::fixed<<std::setprecision(3)
       <<"CJXCgX"<<x<<"Y"<<y<<"Z"<<z<<"C"<<c
       <<"F"<<f<<"$";
    return writeCmd(oss.str());
}

bool YX20KController::cameraMoveAbsolute(const std::string &axis, int dir, int feed)
{	
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    oss << "CJXCg";
	oss << axis;
	if(axis == "X"){
		if(0 == dir) camera_move_x_ = last_x_ - gradient_;
		else if(1 == dir) camera_move_x_ = last_x_ + gradient_;
		oss << camera_move_x_;
	}else if(axis == "Y"){
		if(0 == dir) camera_move_y_ = last_y_ - gradient_;
		else if(1 == dir) camera_move_y_ = last_y_ + gradient_;
		oss << camera_move_y_;
	}
	oss << "F" << feed << "$";
	std::string str = oss.str();
	move_flag_ = true;
    return writeCmd(str);
}

bool YX20KController::moveAbsolute2(double x, double y, double z, double c, int f, MovementCallback cb) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    oss << "CJXCg";

    bool needs_move = false;
    // For each axis, check if it needs to be moved (value is not the default and different from last time)
    if (!isDefaultValue(x) && !isAlmostZero(x - last_x_)) { oss << "X" << x; move_x_ = x; /*last_x_ = x;*/ needs_move = true; }
    if (!isDefaultValue(z) && !isAlmostZero(z - last_z_)) { oss << "Z" << z; move_z_ = z; /*last_z_ = z;*/ needs_move = true; }
    if (!isDefaultValue(c) && !isAlmostZero(c - last_c_)) { oss << "C" << c; move_c_ = c; /*last_c_ = c;*/ needs_move = true; }

    if (!needs_move) {
        printf("INFO: CNC axes already at target position. No movement needed.\n");
        if (cb) {
            cb(true, "CNC axes already at target position"); // Report completion immediately via callback
        } else {
            protocol_process_.sendResults("ok", 1); // Report completion via HTTP
        }
        return true;
    }

    oss << "F" << f << "$";
    std::string str = oss.str();

    move_flag_ = true;
    current_callback_ = cb; // Store the callback (will be nullptr if called from HTTP)
    printf("INFO: Sending selective CNC command: %s\n", str.c_str());
    return writeCmd(str);
}

int  YX20KController::cameraMove(std::string &axis, int dir, int feed)
{
	int ret = -1;
	if(last_x_ < -70000 || last_z_ < -70000) return ret;
	if(move_flag_) return ret;
	if(!move_flag_) {
		move_flag_ = true;
		printf("camer move axis = %s dir = %d, feed = %d\n",axis.c_str(),dir,feed);
	}
	if(axis == "x" || axis == "X"){
		if(0 == dir) last_x_ -= gradient_;
		else if(1 == dir) last_x_ += gradient_;
		moveAxis("X",last_x_,feed);
	}else if(axis == "y" || axis == "Y"){
		if(0 == dir) last_z_ -= gradient_;
		else if(1 == dir) last_z_ += gradient_;
		moveAxis("Y",last_z_,feed);
	}
	ret = 1;
	return ret;
}


bool YX20KController::setPosition(double x,double y,double z,double c){
    std::ostringstream oss;
    oss<<std::fixed<<std::setprecision(3)
       <<"CJXC!X"<<x<<"Y"<<y<<"Z"<<z<<"C"<<c<<"$";
    return writeCmd(oss.str());
}
bool YX20KController::moveAbsolute2(const std::string &axis, double position, int feed)
{
   std::ostringstream oss;
    oss<<std::fixed<<std::setprecision(3)
       <<"CJXCg"<<axis<<position<<"F"<<feed<<"$";
    return writeCmd(oss.str());
}
bool YX20KController::homeX(Dir d){return writeCmd(d==Dir::Positive?"CJXZX":"CJXZx");}
bool YX20KController::homeY(Dir d){return writeCmd(d==Dir::Positive?"CJXZY":"CJXZy");}
bool YX20KController::homeZ(Dir d){return writeCmd(d==Dir::Positive?"CJXZZ":"CJXZz");}
bool YX20KController::homeC(Dir d){return writeCmd(d==Dir::Positive?"CJXZC":"CJXZc");}
bool YX20KController::moveAxis(const std::string &axis, double position, int feed)
{
    bool ok = false;
    std::map<std::string, float> data_map;
    data_map["X"] = 0;
    data_map["Y"] = 0;
    data_map["Z"] = 0;
    data_map["C"] = 0;
    auto find = data_map.find(axis);
    if(find == data_map.end()){
    	printf("axis = %s is error\n",axis.c_str());
	return ok;
    }
    find->second = position;
    ok = moveAbsolute(data_map["X"],data_map["Y"],data_map["Z"],data_map["C"],feed);
    return ok;
}
bool YX20KController::queryPosition(Position& p,int to){
    char buf[512] = {0};
    int len = 512;
    auto r = writeAndRead("CJXSA", (uint8_t *)buf, 512, to);
    if(r <= 0) return r;
    std::string line = buf;
    if(line.empty()) return false;
    auto pos = line.find("X:");
    if(pos == std::string::npos) return false;
    std::string coordPart = line.substr(pos);
    std::regex re(R"(X:\s*([-\d\.]+)\s*Y:\s*([-\d\.]+)\s*Z:\s*([-\d\.]+)\s*C:\s*([-\d\.]+))");
    std::smatch m;
    if(!std::regex_search(coordPart,m,re)) return false;
    p.x=std::stod(m[1]); p.y=std::stod(m[2]);
    p.z=std::stod(m[3]); p.c=std::stod(m[4]);
    return true;
}
bool YX20KController::queryInputs(IOStatus& io,int to){
    uint8_t buf[4] = {0};
    auto r = writeAndRead("CJXBI", buf, 4, to);
    if(r <= 0) return r;
    io.inputs=(uint32_t(buf[0])<<24)|(uint32_t(buf[1])<<16)|
              (uint32_t(buf[2])<<8)|uint32_t(buf[3]);
    return true;
}

void YX20KController::startPositionPolling(
        int interval_ms,std::function<void(const Position&)> cb){
    if(polling_) return;
    polling_=true;
    poll_th_=std::thread(&YX20KController::pollingLoop,
                         this,interval_ms,std::move(cb));
}
void YX20KController::stopPositionPolling(){
    if(!polling_) return;
    polling_=false;
    if(poll_th_.joinable()) poll_th_.join();
}

bool YX20KController::isAlmostZero(float value, float epsilon) 
{
    return std::fabs(value) < epsilon;
}
void YX20KController::pollingLoop(
        int interval_ms,std::function<void(const Position&)> cb){
    Position p;
	int cut = 0;
    while(polling_){
        if(queryPosition(p)) {if(cut++ % 10 == 0)cb(p);}
	if(!fisrt_start){
	    //if(move_flag_) fisrt_start = true;
	    last_x_ = p.x;
	    last_z_ = p.z;
	    last_c_ = p.c;
		last_y_ = p.y;
	}
	if(camera_move_flag_){
		if(move_flag_){
			if(para_.axis == "X" && isAlmostZero(p.x - camera_move_x_)) move_flag_ = false;
			else if(para_.axis == "Y" && isAlmostZero(p.x - camera_move_y_)) move_flag_ = false;
			else move_flag_ = false;
		}
	}
	else {
	//printf("move_x_ = %f move_z_ = %f move_c_ = %f move flag = %d\n",move_x_,move_z_,move_c_,move_flag_);
	if(move_flag_ && isAlmostZero(p.x - move_x_) && isAlmostZero(p.z - move_z_) && isAlmostZero(p.c - move_c_)){
	   if(!camera_move_flag_){
	   		printf(">>>>>>>>>>>>>>>  X,Z,C Run to the specified location <<<<<<<<<<<<<<<<<<<\n");
           	if (current_callback_) {
               current_callback_(true, "x,z,c move over ...."); // Execute the WebSocket callback
           	} else {
               protocol_process_.sendResults("ok", 1); // Fallback to HTTP feedback
           	}
           	current_callback_ = nullptr; // Reset after use
	    }
	   move_flag_ = false;
	}else if(move_flag_){
	   printf("-------------------start---------------------------------\n");
	   printf("move_x_ = %f move_z_ = %f move_c_ = %f move flag = %d\n",move_x_,move_z_,move_c_,move_flag_);
	   printf("x = %f z = %f c = %f\n",p.x,p.z,p.c);
	   printf("-------------------end---------------------------------\n");
	}
	}
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
}

bool YX20KController::isDefaultValue(double val)
{
    return std::fabs(val - (-80000.0)) < 0.0001;
}

bool YX20KController::moveAxis2(const std::string &axis, double position, int feed)
{
    bool ok = false;
    if(std::fabs(position) < 1e-2f){
	using Dir = YX20KController::Dir;
    	if(axis == "X"){
	   ok = homeX(Dir::Positive);
	}else if(axis == "Y"){
	   ok = homeY(Dir::Positive);	
	}else if(axis == "Z"){
	   ok = homeZ(Dir::Negative);
	}else if(axis == "C"){
	   ok = homeC(Dir::Positive);
	}
    }else{
    	ok = moveAbsolute2(axis, position, feed);
    }
    return ok;
}

