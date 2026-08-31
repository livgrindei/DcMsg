#include "logger.h"

#include <cstdio>
#include <cstdarg>

namespace DS
{
    void Logger::SetLevel(Logger::Level level)
    {
        this->console_lvl = level;
        this->file_lvl = level;
    }

    Logger::Level Logger::GetConsoleLevel()
    {
        return this->console_lvl;
    }

    void Logger::SetConsoleLevel(Logger::Level level)
    {
        this->console_lvl = level;
    }

    void Logger::SetFileLevel(Logger::Level level)
    {
        this->file_lvl = level;
    }

    void Logger::SetTarget(unsigned int target)
    {
        this->trg = target;
    }

    void Logger::vLog(Logger::Level level, unsigned int target, std::string message, va_list args)
    {
        this->mtx.lock();
        if ((target & Logger::Target::Console) && level >= this->console_lvl)
        {
            std::printf("%s[%s]: ", lvl_colors[level].c_str(), lvl_strings[level].c_str());
            std::vprintf(message.c_str(), args);
            std::printf("\x1b[0m\n");
        }
        if (this->isFile && (target & Logger::Target::File) && level >= this->file_lvl)
        {

        }
        this->mtx.unlock();
    }

    void Logger::Log(Logger::Level level, unsigned int src, std::string message, ...)
    {
        va_list args;
        va_start(args, message);
        Logger::vLog(level, src, message, args);
        va_end(args);
    }

    void Logger::Log(Level level, std::string message, ...)
    {
        std::va_list args;
        va_start(args, message);
        Logger::vLog(level, this->trg, message, args);
        va_end(args);
    }

    void Logger::LogTrace(std::string message, ...)
    {
        va_list args;
        va_start(args, message);
        Logger::vLog(Level::Trace, this->trg, message, args);
        va_end(args);
    }
    
    void Logger::LogDebug(std::string message, ...)
    {
        va_list args;
        va_start(args, message);
        Logger::vLog(Level::Debug, this->trg, message, args);
        va_end(args);
    }

    void Logger::LogInfo(std::string message, ...)
    {
        va_list args;
        va_start(args, message);
        Logger::vLog(Level::Info, this->trg, message, args);
        va_end(args);
    }

    void Logger::LogWarning(std::string message, ...)
    {
        va_list args;
        va_start(args, message);
        Logger::vLog(Level::Warning, this->trg, message, args);
        va_end(args);
    }

    void Logger::LogError(std::string message, ...)
    {
        va_list args;
        va_start(args, message);
        Logger::vLog(Level::Error, this->trg, message, args);
        va_end(args);
    }

    void Logger::LogCritical(std::string message, ...)
    {
        va_list args;
        va_start(args, message);
        Logger::vLog(Level::Critical, this->trg, message, args);
        va_end(args);
    }
    
} // namespace DS
