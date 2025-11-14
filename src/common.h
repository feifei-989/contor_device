
#pragma once
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <fstream>
#include <string>
#include <chrono>
#include <iomanip>


	class Common
	{
	public:
		static std::string floatToString(float value,int setprecision = 4);
		static std::string formatOmegaDeltas(const std::string prefix, const std::vector<float>& list_dist, const std::string seg = "\n");
		static void writeDataToFile(const std::string& filePath, const std::string& data, std::string& time);
		static std::string creatFileDir(const std::string path = "");
		static int extractValuesFromText(const std::string& in_data, std::vector<float>& out_value);
		static std::string formatOmegaDeltas(const std::vector<float>& list_dist, int type = 0);
		static int getCalibrationPara(const std::string& in_data, std::vector<float>& out_value);
		static int64_t getTimestamp();
		static int getCfgPara(std::vector<float> &out_data,int type);
		static std::string durationToMillisecondsString(const std::chrono::milliseconds& duration);
		static std::string getUrl();
		static std::string getCurrentTimeWithMilliseconds();
		static int getFace();
		static int getSaveCount();
		static int getLightSpotDetection();
		static int getModel();
		static int getIndex(int &out_v0, int &out_v1);
		static int getUdpIpAndPort(std::string &ip, int &port);
		static std::string getUrl2();
		static std::string getContorDevUrl();
		static std::string cfg_dir_;// = "D:\\work\\cfg\\ys_calibration.ini";
	};



