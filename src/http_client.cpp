
#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include "http_client.h"
#include "httplib.h"


#include <string>
#include <stdexcept> // For std::exception

// Helper function to trim leading/trailing whitespace (remains unchanged)
static std::string trim_string(const std::string& str) {
    const std::string WHITESPACE = " \t\n\r\f\v";
    size_t start = str.find_first_not_of(WHITESPACE);
    if (start == std::string::npos) {
        return ""; // String is empty or contains only whitespace
    }
    size_t end = str.find_last_not_of(WHITESPACE);
    return str.substr(start, end - start + 1);
}

// Helper function to remove leading and trailing double quotes (remains unchanged)
static std::string remove_outer_quotes(const std::string& str) {
    if (str.length() >= 2 && str.front() == '"' && str.back() == '"') {
        return str.substr(1, str.length() - 2);
    }
    return str;
}

/**
 * @brief Processes an INI-like string.
 * For lines with '=', quotes are removed from their values, and trailing comments are preserved.
 * The line is NOT commented out with a leading ';'.
 * Example: 'key = "value" ; comment' becomes 'key = value ; comment'.
 * Lines without '=' are unchanged.
 *
 * If the input string is empty, the function returns 0 and out_data is empty.
 *
 * @param input The input string containing INI-like content.
 * @param out_data The output string with processed content.
 * @return 0 on success, -1 on failure (e.g., due to an unexpected exception).
 */
int transform_ini_string(const std::string& input, std::string& out_data) {
    out_data.clear(); // Clear output string once at the beginning.

    if (input.empty()) {
        return 0; // Input is empty, output is empty, success.
    }

    try {
        size_t current_pos = 0;
        while (current_pos < input.length()) {
            size_t newline_pos = input.find('\n', current_pos);
            std::string line_to_process;

            if (newline_pos != std::string::npos) {
                line_to_process = input.substr(current_pos, newline_pos - current_pos);
            }
            else {
                line_to_process = input.substr(current_pos);
            }

            std::string processed_segment;
            size_t eq_pos = line_to_process.find('=');

            if (eq_pos != std::string::npos) { // Line contains an equals sign
                std::string key_original_untrimmed = line_to_process.substr(0, eq_pos);
                std::string value_and_comment_part_untrimmed = line_to_process.substr(eq_pos + 1);

                std::string final_trimmed_key = trim_string(key_original_untrimmed);

                std::string value_part_for_processing;
                std::string comment_part = ""; // Default to no comment

                // Find the start of the comment part (everything from the first ';').
                // This basic approach assumes that a semicolon intended as part of a value
                // would be inside the quotes that `remove_outer_quotes` handles, or values
                // simply don't contain semicolons that are not comment starters.
                size_t comment_char_pos = value_and_comment_part_untrimmed.find(';');

                if (comment_char_pos != std::string::npos) {
                    value_part_for_processing = value_and_comment_part_untrimmed.substr(0, comment_char_pos);
                    // comment_part includes the semicolon and any original spacing around it that was part of the comment.
                    comment_part = value_and_comment_part_untrimmed.substr(comment_char_pos);
                }
                else {
                    value_part_for_processing = value_and_comment_part_untrimmed;
                }

                std::string value_part_trimmed_maybe_quoted = trim_string(value_part_for_processing);
                std::string final_value_no_quotes = remove_outer_quotes(value_part_trimmed_maybe_quoted);

                // Reconstruct the line: key = value_no_quotes<original_comment_part_with_its_spacing>
                processed_segment = final_trimmed_key + " = " + final_value_no_quotes + comment_part;

            }
            else { // Line does not contain '=', keep it as is
                processed_segment = line_to_process;
            }

            out_data += processed_segment;

            if (newline_pos != std::string::npos) {
                out_data += "\n"; // Append the newline if one was found
                current_pos = newline_pos + 1;
            }
            else {
                current_pos = input.length(); // Reached the end of the input
            }
        }
        return 0; // Success
    }
    catch (const std::exception& /* e */) {
        // If any exception occurs, clear out_data to ensure it's not in a corrupt state.
        out_data.clear();
        return -1; // Failure
    }
}


namespace ys_scene {

	std::string HttpClient::readFile(const std::string& filePath)
	{
		std::ifstream file(filePath, std::ios::binary);
		if (!file) {
			throw std::runtime_error("Failed to open file: " + filePath);
		}
		std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		return content;
	}

	void HttpClient::sendFile(const std::string &url, const std::string &fileName)
	{
        std::string fileContent = readFile(fileName);
		std::string url_str;
		if(url.empty()){
			url_str = "http://localhost:8080";
		}else{
			url_str = url;
		}
        httplib::Client client(url_str);
        httplib::MultipartFormDataItems items = {
            {"file", fileContent, "example.txt", "text/plain"}
        };
        auto res = client.Post("/upload", items);
        if (res) {
            std::cout << "Status: " << res->status << "\n";
            std::cout << "Response Body: " << res->body << "\n";
        } else {
            std::cerr << "Error: " << res.error() << "\n";
        }

	}

	bool HttpClient::downloadFile(const std::string& url, const std::string& saveDir)
	{
		httplib::Client cli(url.c_str());
		std::string fileName = "ys_calibration.ini";
		std::string saveFilePath = saveDir + fileName;
		auto res = cli.Get("/download");
		if (res && res->status == 200) {
			if (std::remove(saveFilePath.c_str()) == 0) {
				std::cout << "Existing file removed: " << saveFilePath << std::endl;
			}
			else {
				if (errno != ENOENT) {
					std::cerr << "Failed to remove existing file: " << saveFilePath << std::endl;
					return false;
				}
			}
			std::ofstream outFile(saveFilePath, std::ios::binary);
			if (outFile.is_open()) {
                std::string chang_str = "";
                auto r = transform_ini_string(res->body,chang_str);
                if (r)return false;
				//outFile.write(res->body.c_str(), res->body.size());
                outFile.write(chang_str.c_str(), chang_str.size());
				outFile.close();
				std::cout << "File downloaded successfully: " << saveFilePath << std::endl;
				return true;
			}
			else {
				std::cerr << "Failed to open file for writing: " << saveFilePath << std::endl;
			}
		}
		else {
			std::cerr << "Failed to download file. Status: " << (res ? res->status : -1) << std::endl;
		}
		return false;
	}
	void HttpClient::sendFile2(const std::string& url, const std::string& fileName)
	{
		std::string fileContent = readFile(fileName);
		std::string url_str;
		if (url.empty()) {
			url_str = "http://localhost:8080";
		}
		else {
			url_str = url;
		}
		httplib::Client client(url_str);
		httplib::MultipartFormDataItems items = {
			{"file", fileContent, "example.txt", "text/plain"}
		};
		auto res = client.Post("/upload_table", items);
		if (res) {
			std::cout << "Status: " << res->status << "\n";
			std::cout << "Response Body: " << res->body << "\n";
		}
		else {
			std::cerr << "Error: " << res.error() << "\n";
		}
	}
    std::string HttpClient::sendData(const std::string& url, const std::string& path, const std::string& data)
    {
        std::string ret = "";
        httplib::Client client(url.c_str());
        httplib::Headers headers = {
            { "Content-Type", "application/json" }
        };
        printf("url is = %s\n data is = %s\n", url.c_str(), data.c_str());
        auto res = client.Post(path.c_str(), headers, data, "application/json");
        if (res && res->status == 200) {
            std::cout << "Successfully sent JSON to POS! Response: " << res->body << std::endl;
            ret = res->body;
        }
        else {
            std::cerr << "Failed to send JSON. Status: " << (res ? std::to_string(res->status) : "No Response") << std::endl;
        }
        return ret;
    }
}



