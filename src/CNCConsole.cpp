

#include "CNCConsole.h"
#include <iostream>
#include <sstream>
#include <regex>
#include <cctype>

using json = nlohmann::json;

CNCConsole::CNCConsole(std::string dev, int baud)
    : dev_(std::move(dev)), baud_(baud) {}

bool CNCConsole::start()
{
    if (running_) return true;
    cnc_ptr_ = std::make_shared<YX20KController>(dev_, baud_);
    if (!cnc_ptr_->open()) {
        std::cerr << "打开失败\n";
        return false;
    }
    printf("dev = %s baud = %ld open sucess ...\n",dev_.c_str(),baud_);

    running_ = true; 
	int delay_time = 300;//2000;
    cnc_ptr_->startPositionPolling(delay_time,[this](const YX20KController::Position &d){
		     printf("x = %f, y = %f, z = %f, c = %f\n",d.x,d.y,d.z,d.c);
		    });
    
    commandThread_ = std::thread(&CNCConsole::commandLoop, this);

    // std::thread(&CNCConsole::inputLoop, this, cnc_ptr_).detach();
    return true;
}

void CNCConsole::stop()
{
    if (!running_.exchange(false)) {
        return;
    }
    queueCv_.notify_one();
    if (commandThread_.joinable()) {
        commandThread_.join();
    }
    stopPositionPolling();

    // 4. (可选, 推荐) 关闭控制器连接
    // if (cnc_ptr_) {
    //     cnc_ptr_->close(); 
    // }
}

void CNCConsole::startPositionPolling(
        int interval_ms,
        std::function<void(const YX20KController::Position&)> cb)
{
    if(nullptr == cnc_ptr_){
    	cnc_ptr_ = std::make_shared<YX20KController>(dev_, baud_);
    	if (!cnc_ptr_->open()) {
        	std::cerr << "打开失败\n"; return;
    	}
    }
    cnc_ptr_->startPositionPolling(interval_ms, std::move(cb));
}

bool CNCConsole::parseJogCommand(const std::string& jsonString, YX20KController::JogCommand &para)
{
	bool ok = false;
    try {
        json j = json::parse(jsonString);
        para.axis      = j.at("axis").get<std::string>();
        para.dir       = j.at("dir").get<int>();
        para.gradient  = j.at("gradient").get<float>();
        para.speed     = j.at("speed").get<int>();
		ok = true;

    } catch (const json::parse_error& e) {
        std::cerr << "JSON 解析错误: " << e.what()
                  << "\n原始字符串: " << jsonString << std::endl;
    } catch (const json::out_of_range& e) {
        std::cerr << "JSON 键名错误: " << e.what()
                  << "\n原始字符串: " << jsonString << std::endl;
    } catch (const json::type_error& e) {
        std::cerr << "JSON 类型错误: " << e.what()
                  << "\n原始字符串: " << jsonString << std::endl;
    }
    return ok;
}

void CNCConsole::stopPositionPolling()
{
    // if (cnc_ptr_) {
    //     cnc_ptr_->stopPositionPolling();
    // }
    // (保持与用户文件一致，暂时为空)
}

void CNCConsole::pushCommand(const std::string& cmd)
{
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        commandQueue_.push(cmd);
    } 
    queueCv_.notify_one();
}

void CNCConsole::commandLoop()
{
    while (running_) {
        std::string cmd;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCv_.wait(lock, [this] {
                return !running_ || !commandQueue_.empty();
            });

            if (!running_ && commandQueue_.empty()) {
                break;
            }
            if (!commandQueue_.empty()) {
                cmd = std::move(commandQueue_.front());
                commandQueue_.pop();
            }
        }
        if (!cmd.empty()) {
            processCommand(cmd);
        }
    }
    std::cout << "Command loop exiting." << std::endl;
}

void CNCConsole::processCommand(const std::string& cmd)
{
    try {
		printf("process msg is = %s\n",cmd.c_str());
		YX20KController::JogCommand para;
		if(parseJogCommand(cmd,para)) cnc_ptr_->updatePara(para);
    } catch (json::parse_error& e) {
        std::cerr << "Failed to parse command string to JSON: " << e.what() << std::endl;
        std::cerr << "Original string: " << cmd << std::endl;
    }
	}


bool CNCConsole::isNumber(const std::string& s)
{
    static const std::regex re(R"(^[-+]?\d*\.?\d+$)");
    return std::regex_match(s, re);
}

std::shared_ptr<YX20KController> CNCConsole::getYX20KController()
{
   return cnc_ptr_;
}

void CNCConsole::inputLoop(std::shared_ptr<YX20KController> cnc)
{
    std::string line;
    while (running_ && std::getline(std::cin, line)) {
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        if (line.empty()) continue;
        std::string lower; for(char ch:line) lower+=std::tolower(ch);
        if (lower=="quit"||lower=="exit"){ running_=false; break; }
        if (lower.size()==1 && std::string("xyzc").find(lower)!=std::string::npos) {
            using Dir = YX20KController::Dir;
            bool ok=false;
            switch(lower[0]){
		case 'x': ok=cnc->homeX(Dir::Positive); break;
                case 'y': ok=cnc->homeY(Dir::Positive); break;
                case 'z': ok=cnc->homeZ(Dir::Negative); break;
                case 'c': ok=cnc->homeC(Dir::Positive); break;
            }
            if(!ok) std::cerr<<"失败\n";
            continue;
        }
        std::istringstream iss(line); std::string tok;
        double v[4]; int f=0, cnt=0;
        while(iss>>tok){
            if(!isNumber(tok)){ cnt=-1; break; }
            if(cnt<4) v[cnt]=std::stod(tok);
            else      f=std::stoi(tok);
            ++cnt;
        }
        if(cnt==5){
            if(!cnc->moveAbsolute(v[0],v[1],v[2],v[3],f))
                std::cerr<<"发送指令失败\n";
        }else{
            std::cerr<<"错误\n";
        }
    }
}

