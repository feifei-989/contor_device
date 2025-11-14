

#include "common.h"
#include <iomanip>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <regex>
#include "config.h"
	std::string Common::cfg_dir_ = "D:\\work\\cfg\\ys_calibration.ini"; 

    std::string Common::floatToString(float value,int setprecision)
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(setprecision) << value;
        return oss.str();
    }

    std::string Common::formatOmegaDeltas(const std::string prefix, const std::vector<float>& list_dist, const std::string seg)
    {
        std::ostringstream oss;
        for (size_t i = 0; i < list_dist.size(); ++i) {
            oss << prefix << ">> d" << i << "_delta" << " = " << std::fixed << std::setprecision(4) << list_dist[i] << seg;
        }
        return oss.str();
    }

    void Common::writeDataToFile(const std::string& filePath, const std::string& data, std::string& time)
    {
        std::ofstream file(filePath, std::ios::app);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open or create file at " << filePath << std::endl;
            return;
        }
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d-%H:%M:%S");
        time = oss.str();
        file << time << " : " << data << std::endl;
        file.close();
    }

    std::string Common::creatFileDir(const std::string path)
    {
	return "";
    }

    int Common::extractValuesFromText(const std::string& in_data, std::vector<float>& out_value)
    {
        std::regex pattern(R"(omega_delta\d+\s*=\s*(-?\d+\.\d+))");
        std::smatch match;
        auto words_begin = std::sregex_iterator(in_data.begin(), in_data.end(), pattern);
        auto words_end = std::sregex_iterator();
        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            match = *i;
            float value = std::stof(match.str(1));
            out_value.push_back(value);
        }
        return out_value.size();
    }

    std::string Common::formatOmegaDeltas(const std::vector<float>& list_dist, int type)
    {
        std::ostringstream oss;
        if (0 == type) {
            for (size_t i = 0; i < list_dist.size(); ++i) {
                oss << "omega_delta" << i << " = " << std::fixed << std::setprecision(4) << list_dist[i] << "\n";
            }
        }
        else if (1 == type) {
            for (size_t i = 0; i < list_dist.size(); ++i) {
                oss << "d" << i << "_delta" << " = " << std::fixed << std::setprecision(4) << list_dist[i] << "\n";
            }
        }else if(2 == type){
			for (size_t i = 0; i < list_dist.size(); ++i) {
                oss << "side" << i << "_angular_offset" << " = " << std::fixed << std::setprecision(4) << list_dist[i] << "\n";
            }
		}else if(3 == type){
			for (size_t i = 0; i < list_dist.size(); ++i) {
                oss << "angular_distortion" << " = " << std::fixed << std::setprecision(4) << list_dist[i] << "\n";
            }
		}
        return oss.str();
    }

    int Common::getCalibrationPara(const std::string& in_data, std::vector<float>& out_value)
    {
        std::regex re(R"(= (-?\d+\.\d+))");
        auto begin = std::sregex_iterator(in_data.begin(), in_data.end(), re);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            std::smatch match = *it;
            float value = std::stof(match[1]);
            out_value.push_back(value);
        }
        return out_value.size();
    }

    int64_t Common::getTimestamp()
    {
        auto now = std::chrono::system_clock::now();
        int64_t seconds = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        return seconds;
    }

	int Common::getCfgPara(std::vector<float> &out_data,int type)
    {
		int ret = 0;
		Config cfg;
        int count = 96;
		if (cfg.open(cfg_dir_) > 0) {
            if (3 == type || 4 == type) {
                count = 4;
            }
			for(int i = 0; i < count; i++){
				std::string value;
				if(0 == type){
					std::string key = "omega_delta";
					key.append(std::to_string(i));
					auto r = cfg.read("horizontal_angular_offset",key,value);
					if(r < 0) return r;
				}else if(1 == type){
					std::string key = "d";
					key.append(std::to_string(i));
					key.append("_delta");
					auto r = cfg.read("zero_distance_offset",key,value);
					if(r < 0) return r;
				}else if(2 == type){
					std::string key = "a";
					key.append(std::to_string(i));
					auto r = cfg.read("distance_factor",key,value);
					if(r < 0) return r;
                }
                else if (3 == type) {
                    std::string key = "side";
                    key.append(std::to_string(i));
                    key.append("_angular_offset");
                    auto r = cfg.read("side_angular_offset", key, value);
                    if (r < 0) return r;
                }
                else if (4 == type) {
                    std::string key = "angle_face_";
                    key.append(std::to_string(i+1));
                    auto r = cfg.read("standard_encoder", key, value);
                    if (r < 0) return r;
                }
                else {
					return ret;
				}
				out_data.push_back(std::stof(value));
			}
			ret = out_data.size();
		}
		return ret;
	}

    std::string Common::durationToMillisecondsString(const std::chrono::milliseconds& duration)
    {
        std::ostringstream oss;
        oss << duration.count() << " ms";
        return oss.str();
    }

	std::string Common::getUrl()
	{
		Config cfg;
		std::string url_addr;
		//save_dir = Common::creatFileDir();
		std::string cfg_file = "D:/work/cfg/ys_app_calibration.ini";
		auto ret = cfg.open(cfg_file);
		if(ret > 0){
			std::string url_value;
			if (0 == cfg.read("parameter", "dev_ip", url_value)) {
				if (!url_value.empty()) {
					url_addr = "http://" + url_value + ":8080";
				}
			}
		}
		return url_addr;
	}

	std::string Common::getCurrentTimeWithMilliseconds()
	{
    	// 获取当前时间点
	    auto now = std::chrono::system_clock::now();
	    
	    // 转换为系统时间
	    std::time_t t = std::chrono::system_clock::to_time_t(now);
	    
	    // 将时间转换为 tm 结构体
	    std::tm tm = *std::localtime(&t);
	    
        auto timestamp = std::chrono::system_clock::to_time_t(now);

	    // 使用 stringstream 格式化输出
	    std::ostringstream oss;
	    oss << std::put_time(&tm, "%Y-%m-%d") << "-" << timestamp;
	    
	    return oss.str();
	}

    int Common::getFace()
    {
        Config cfg;
        int ret = 4;
        std::string cfg_file = "D:/work/cfg/ys_app_calibration.ini";
        auto r = cfg.open(cfg_file);
        if (r > 0) {
            std::string value;
            if (0 == cfg.read("camera", "face", value)) {
                if (!value.empty()) {
                    ret = std::atoi(value.c_str());
                }
            }
        }
        return ret;
    }

    int Common::getSaveCount()
    {
        Config cfg;
        int ret = 2;
        std::string cfg_file = "D:/work/cfg/ys_app_calibration.ini";
        auto r = cfg.open(cfg_file);
        if (r > 0) {
            std::string value;
            if (0 == cfg.read("camera", "save", value)) {
                if (!value.empty()) {
                    ret = std::atoi(value.c_str());
                }
            }
        }
        return ret;
    }

    int Common::getLightSpotDetection()
    {
        Config cfg;
        int ret = 0;
        std::string cfg_file = "D:/work/cfg/ys_app_calibration.ini";
        auto r = cfg.open(cfg_file);
        if (r > 0) {
            std::string value;
            if (0 == cfg.read("camera", "gb", value)) {
                if (!value.empty()) {
                    ret = std::atoi(value.c_str());
                }
            }
        }
        return ret;
    }

    int Common::getModel()
    {
        Config cfg;
        int ret = 0;
        std::string cfg_file = "D:/work/cfg/ys_app_calibration.ini";
        auto r = cfg.open(cfg_file);
        if (r > 0) {
            std::string value;
            if (0 == cfg.read("camera", "model", value)) {
                if (!value.empty()) {
                    ret = std::atoi(value.c_str());
                }
            }
        }
        return ret;
    }

    int Common::getIndex(int& out_v0, int& out_v1)
    {
        Config cfg;
        int ret = 0;
        std::string cfg_file = "D:/work/cfg/ys_app_calibration.ini";
        auto r = cfg.open(cfg_file);
        if (r > 0) {
            std::string value;
            if (0 == cfg.read("camera", "index0", value)) {
                if (!value.empty()) {
                    out_v0 = std::atoi(value.c_str());
                }
            }
            if (0 == cfg.read("camera", "index1", value)) {
                if (!value.empty()) {
                    out_v1 = std::atoi(value.c_str());
                }
            }
        }
        return ret;
    }

    int Common::getUdpIpAndPort(std::string& ip, int& port)
    {
        Config cfg;
        int ret = 0;
        std::string cfg_file = "./ys_app_calibration.ini";
        auto r = cfg.open(cfg_file);
        if (r > 0) {
            std::string value;
            if (0 == cfg.read("parameter", "dev_ip", value)) {
                if (!value.empty()) {
                    ip = value;
                }
            }
            if (0 == cfg.read("parameter", "udp_port", value)) {
                if (!value.empty()) {
                    port = std::atoi(value.c_str());
                }
            }
        }
        return ret;
    }

    std::string Common::getUrl2()
    {
        Config cfg;
        std::string url_addr = "http://127.0.0.1:8080";
        //save_dir = Common::creatFileDir();
        std::string cfg_file = "D:/work/cfg/ys_app_calibration.ini";
        auto ret = cfg.open(cfg_file);
        if (ret > 0) {
            std::string url_value;
            if (0 == cfg.read("parameter", "dev_ip2", url_value)) {
                if (!url_value.empty()) {
                    url_addr = "http://" + url_value + ":8080";
                }
            }
        }
        return url_addr;
    }

    std::string Common::getContorDevUrl()
    {
        Config cfg;
        std::string url_addr = "http://127.0.0.1:44444";
        //save_dir = Common::creatFileDir();
        std::string cfg_file = "./ys_app_calibration.ini";
        auto ret = cfg.open(cfg_file);
        if (ret > 0) {
            std::string url_value;
            if (0 == cfg.read("parameter", "motor_ip", url_value)) {
                if (!url_value.empty()) {
                    url_addr = "http://" + url_value + ":44444";
                }
            }
        }
        return url_addr;
    }



