

#include "config.h"
#include <iostream>
Config::~Config()
{
	if (ini) {
		close();
		delete ini;
		ini = nullptr;
	}
}

int Config::open(std::string& fileName)
{
	if (!ini) {
		ini = new mINI::INIFile(fileName);
		this->fileNmae = fileName;
	}
	auto ret = ini->read(iniStruct);
	if (!ret) {
		std::cout << "open file :" << this->fileNmae << " failed" << std::endl;
	}
	else {
		openFlag = true;
	}
	return ret;
}

int Config::write(const std::string& section, const std::string& key, const std::vector<float>& list_dist)
{
	if (!ini)return -1;
	if (PointsCount == list_dist.size()) {
		for (int i = 0; i < list_dist.size(); i++) {
			//std::string str = key + std::to_string(i) + " = " + Common::floatToString(list_dist[i]);
			//iniStruct[section][key + std::to_string(i)] = Common::floatToString(list_dist[i]);
		}
		std::cout << "file Name is :" << fileNmae  << " PointsCount is :" << list_dist.size() << " success" << std::endl;
	}
	else {
		std::cout << "file Nmae is :" << fileNmae <<  " PointsCount is :" << list_dist.size() << " not enough" << std::endl;
	}
	return 0;
}

int Config::read(const std::string& section, const std::string& key, std::string& out_data)
{
	bool hasKey = iniStruct[section].has(key);
	if (hasKey) {
		std::string& value = iniStruct[section][key];
		out_data = value;
		return 0;
	}
	return -1;
}

int Config::close()
{
	if (!ini)return -1;
	if (openFlag) {
		openFlag = false;
		ini->write(iniStruct);
	}
	return 0;
}

