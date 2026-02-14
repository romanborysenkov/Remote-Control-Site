#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <ctime>
#include <iomanip>
#include <sstream>

class Logger {
public:
    enum class Level {
        INFO,
        WARNING,
        ERROR
    };
    
    static void init(const std::string& logPath = "/tmp/remotecontrol_client.log");
    static void log(Level level, const std::string& message);
    static void info(const std::string& message);
    static void warning(const std::string& message);
    static void error(const std::string& message);
    
private:
    static std::ofstream logFile_;
    static std::mutex mutex_;
    static std::string levelToString(Level level);
    static std::string getTimestamp();
};

