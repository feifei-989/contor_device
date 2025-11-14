#include "control_layer.h"


ControlLayer::ControlLayer()
{

}

ControlLayer::~ControlLayer()
{

}


int ControlLayer::start()
{
	int ret = -1;
	if(tcp_client_ptr_ == nullptr) {
		tcp_client_ptr_ = std::make_shared<TCPClient>();
		tcp_client_ptr_->setDataCallback([this](const std::vector<uint8_t>& data){
			std::string str(data.begin(), data.end());
			if(cnc_controller_ptr_) cnc_controller_ptr_->pushCommand(str);
		});
		tcp_client_ptr_->connect();
		ret = 1;
	}
	return ret;
}

void ControlLayer::stop()
{

}


void ControlLayer::setCncController(std::shared_ptr<CNCConsole> data)
{
	if(cnc_controller_ptr_ == nullptr) cnc_controller_ptr_ = data;
}


