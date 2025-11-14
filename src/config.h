
#pragma once
#include "ini.h"
class Config
{
public:
	Config()=default;
	~Config();
	const int PointsCount = 96;
	int open(std::string &fileName);
	int write(const std::string &section, const std::string &key, const std::vector<float>& list_dist);
	int read(const std::string& section, const std::string& key, std::string& out_data);
	int close();
private:
	mINI::INIFile* ini = nullptr;
	mINI::INIStructure iniStruct;
	std::string fileNmae;
	bool openFlag = false;
};



