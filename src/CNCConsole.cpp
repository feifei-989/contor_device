

#include "CNCConsole.h"
#include <iostream>
#include <sstream>
#include <regex>
#include <cctype>

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
    //std::thread(&CNCConsole::inputLoop, this, cnc_ptr_).detach();
    cnc_ptr_->startPositionPolling(2000,[this](const YX20KController::Position &d){
		     printf("x = %f, y = %f, z = %f, c = %f\n",d.x,d.y,d.z,d.c);
		    });
    return true;
}

void CNCConsole::stop()
{
    running_ = false;     
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

void CNCConsole::stopPositionPolling()
{
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


