

#pragma once

#include <iostream>
#include <fstream>
#include <string>




namespace ys_scene {

	class HttpClient{
		
			static std::string readFile(const std::string& filePath);
		public:
			static void sendFile(const std::string &url, const std::string &fileName);
			static bool downloadFile(const std::string& url, const std::string& saveDir);
			static void sendFile2(const std::string& url, const std::string& fileName);
			static std::string sendData(const std::string& url, const std::string& path, const std::string& data);
	};

}



