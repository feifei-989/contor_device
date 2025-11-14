
#pragma once

#include "tcp_client.h"
#include "CNCConsole.h"


class ControlLayer{
public:
	ControlLayer();
	~ControlLayer();
	int start();
	void stop();
	void setCncController(std::shared_ptr<CNCConsole> data);


private:
	std::shared_ptr<TCPClient> tcp_client_ptr_ = nullptr;
	std::shared_ptr<CNCConsole> cnc_controller_ptr_ = nullptr;
};

