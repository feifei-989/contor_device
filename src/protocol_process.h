#pragma once
#include "nlohmann/json.hpp"
#include "http_client.h"
#include "common.h"
class ProtocolProcess{
   public:
      ProtocolProcess() { auto r = Common::getContorDevUrl(); setUrl(r); }
      ~ProtocolProcess() {}
      void setUrl(const std::string &url = ""){
	if(url == "") return;
      	url_ = url;
	printf(".... url = %s\n",url_.c_str());
      }
      int sendResults(const std::string &data, int type = 0){
        nlohmann::json json_data;
	json_data["type"] = type;
	json_data["status"] = data;
	std::string d = json_data.dump();
	if (ys_scene::HttpClient::sendData(url_, "/status", d) == "")return -1;
	return 0;
      }
   private:
      std::string url_ = "http://192.168.110.196:44444";
};

