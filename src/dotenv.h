#ifndef DOTENV_H
#define DOTENV_H

#include <string>
#include <map>
#include <fstream>
#include <iostream>
#include <regex>

namespace dotenv {
    // Global variables map
    static std::map<std::string, std::string> env;
    
    // Load .env file
    inline void init(const std::string& path = ".env") {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "Warning: Could not open " << path << std::endl;
            return;
        }

        std::string line;
        std::regex keyValuePair("^\\s*(\\w+)\\s*=\\s*(.*)\\s*$");
        
        while (std::getline(file, line)) {
            // Skip comments and empty lines
            if (line.empty() || line[0] == '#') {
                continue;
            }
            
            std::smatch match;
            if (std::regex_match(line, match, keyValuePair)) {
                std::string key = match[1];
                std::string value = match[2];
                
                // Remove quotes if present
                if (value.size() >= 2 && 
                    ((value.front() == '"' && value.back() == '"') || 
                     (value.front() == '\'' && value.back() == '\''))) {
                    value = value.substr(1, value.size() - 2);
                }
                
                env[key] = value;
            }
        }
    }
}

#endif // DOTENV_H