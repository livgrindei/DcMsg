#pragma once

#include <string>
#include <mutex> 

namespace DS
{
    class Logger
    {
        public:
            enum Level { Trace, Debug, Info, Warning, Error, Critical };
            enum Target : unsigned int
            {
                Console   = 0b00000001,
                File      = 0b00000010
            };
        private:
            std::string lvl_strings[6];
            std::string lvl_colors[6];
            Logger::Level console_lvl;
            Logger::Level file_lvl;
            unsigned int trg;
            std::mutex mtx;
            bool isFile;
        private:
            Logger() 
            {
                this->lvl_strings[0] = "TRACE";
                this->lvl_strings[1] = "DEBUG";
                this->lvl_strings[2] = "INFO";
                this->lvl_strings[3] = "WARNING";
                this->lvl_strings[4] = "ERROR";
                this->lvl_strings[5] = "CRITICAL";
                this->lvl_colors[1] = "\x1b[34m";
                this->lvl_colors[2] = "\x1b[32m";
                this->lvl_colors[3] = "\x1b[33m";
                this->lvl_colors[4] = "\x1b[31m";
                this->lvl_colors[5] = "\x1b[35m";
                this->console_lvl = Logger::Level::Info;
                this->trg = Logger::Target::Console;
                this->isFile = false;
                this->file_lvl = Logger::Level::Info;
            };
            void vLog(Logger::Level level, unsigned int src, std::string message, va_list args);
        public:
            Logger(Logger const&) = delete;
            void operator=(Logger const&) = delete;

            static Logger& Instance()
            {
                static Logger _instance;
                return _instance;
            };
            void SetLevel(Logger::Level level);
            Logger::Level GetConsoleLevel();
            void SetConsoleLevel(Logger::Level level);
            void SetFileLevel(Logger::Level level);
            void SetTarget(unsigned int target);
            void Log(Logger::Level level, unsigned int src, std::string message, ...);
            void Log(Level level, std::string message, ...);
            void LogTrace(std::string message, ...);
            void LogDebug(std::string message, ...);
            void LogInfo(std::string message, ...);
            void LogWarning(std::string message, ...);
            void LogError(std::string message, ...);
            void LogCritical(std::string message, ...);
    };
} // namespace DS
