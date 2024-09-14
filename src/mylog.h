#ifndef __dbg_h__
#define __dbg_h__

#include <iostream>
#include <string>

enum class LogLevel {
    INFO,
    WARNING,
    LOGERROR
};

class Logger {
public:
    Logger() {}

    template <typename... Args>
    void log(LogLevel level, const std::string& functionName, int lineNumber, const char* fileName, const Args&... args) {
        std::string logLevelStr;
        switch (level) {
        case LogLevel::INFO:
            logLevelStr = "INFO";
            break;
        case LogLevel::WARNING:
            logLevelStr = "WARNING";
            break;
        case LogLevel::LOGERROR:
            logLevelStr = "ERROR";
            break;
        }

        std::cout << "[" << logLevelStr << "] " << fileName << " - " << functionName << " (Line " << lineNumber << "): ";
        logHelper(args...);
        std::cout << std::endl;
    }

private:
    void logHelper() {}

    template <typename T, typename... Args>
    void logHelper(const T& arg, const Args&... args) {
        std::cout << arg << " ";
        logHelper(args...);
    }
};

#endif