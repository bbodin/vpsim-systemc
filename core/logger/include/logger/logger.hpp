/*
 * Copyright (C) 2024 Commissariat à l'énergie atomique et aux énergies alternatives (CEA)

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 *    http://www.apache.org/licenses/LICENSE-2.0 

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#ifndef _LOGGER_HPP_
#define _LOGGER_HPP_

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include "logResources.hpp"



namespace vpsim {
    //! @brief Class to inherit from to use the logging macros below.
    class Logger {
        //! @brief Allows the LoggerCore to change the debug level of the Logger object
        friend class LoggerCore;

    private:
        //! @brief Name of the logger
        std::string mName;

        //! @brief Name of the log output file of the logger
        std::string mStatLogName;

        //! @brief Current level of debug of the logger
        DebugLvl mDebugLvl;

        //! @brief Output stream to the mLogName file
        std::ostream &mOfstream;
        std::ofstream mStatStream;

        //! @brief Tells if the output is enabled for this logger
        bool mEnabled;

    public:
        //! @brief Only public Constructor
        //! @param[in] name Name to give to the logger
        Logger(const std::string& name, const std::string& statslogfile = "", std::ostream &stream = std::cout);

        //! @brief Destructor
        ~Logger();

        //! @brief Access the name of the logger
        //! @return The name of the logger
        std::string name() const;

        //! @brief Access the name of the output file of the logger
        //! @return The name of the output file of the logger
        std::string statLogName() const;

        //! @brief Change the name of the output file of the logger
        //! @return true if the name is correctly change (fail if already opened)
        bool setStatLogName(std::string);

        //! @brief Tells if the logger can log info messages
        //! @return True if it can log info messages, false otherwise
        //! @see LOG_INFO
        //! @see logInfo()
        bool canLogInfo() const;

        //! @brief Tells if the logger can log warning messages
        //! @return True if it can log warning messages, false otherwise
        //! @see LOG_WARNING
        //! @see logWarning()
        bool canLogWarning() const;

        //! @brief Tells if the logger can log Error messages
        //! @return True if it can log Error messages, false otherwise
        //! @see LOG_ERROR
        //! @see logError()
        bool canLogError() const;

        //! @brief Tells if the logger can log stats messages
        //! @return True if it can log stats messages, false otherwise
        //! @see LOG_STATS
        //! @see logStats()
        bool canLogStats() const;

        //! @brief Tells if the logger can log debug messages
        //! @param[in] lvl Level of the debug requiered
        //! @return True if it can log debug messages of the given level, false otherwise
        //! @see LOG_INFO
        //! @see logDebug(DebugLvl)
        bool canLogDebug(DebugLvl lvl) const;

        //! @brief Access the output stream to log info messages
        //! @return The stream to log info
        std::ostream &logInfo();

        //! @brief Access the output stream to log warning messages
        //! @return The stream to log warnings
        std::ostream &logWarning();

        //! @brief Access the output stream to log error messages
        //! @return The stream to log errors
        std::ostream &logError();

        //! @brief Access the output stream to log stats messages
        //! @return The stream to log stats
        std::ofstream &logStats();

        //! @brief Access the output stream to log debug messages
        //! @param[in] lvl level of the debug messages to be loggeg
        //! @return The stream to log debugs
        std::ostream &logDebug(DebugLvl lvl);

    private:
        Logger();
    };
}


namespace vpsim {
    extern Logger globalLogger;
}

//! @brief provides a stream to the logging file for a line of INFO in the global log file
#define LOG_GLOBAL_INFO        if(globalLogger.canLogInfo())       globalLogger.logInfo()       << "[Info] "

//! @brief provides a stream to the logging file for a line of WARNING in the global log file
#define LOG_GLOBAL_WARNING     if(globalLogger.canLogWarning())    globalLogger.logWarning()    << "[Warning] "

//! @brief provides a stream to the logging file for a line of ERROR in the global log file
#define LOG_GLOBAL_STATS       if(globalLogger.canLogStats())      globalLogger.logStats()      << "[Stats] "

//! @brief provides a stream to the logging file for a line of ERROR in the global log file
#define LOG_GLOBAL_ERROR       if(globalLogger.canLogError())      globalLogger.logError()      << "[Error] "

#ifdef ENABLE_DEBUG
    //! @brief provides a stream to the logging file for a line of DEBUG in the global log file
    //! @param[in] lvl Level of debug of the message
    #define LOG_GLOBAL_DEBUG(lvl)  if(globalLogger.canLogDebug((lvl))) globalLogger.logDebug((lvl)) << "[Debug" << (lvl) << "] "
#else
    struct NullStream : std::ostream {
        NullStream() : std::ostream(nullptr) {}
    };
    static NullStream nullStream;

    #define LOG_GLOBAL_DEBUG(lvl) nullStream
#endif






//! @brief provides a stream to the logging file for a line of INFO
#define LOG_INFO        if(Logger::canLogInfo())       Logger::logInfo()       << "[Info] "

//! @brief provides a stream to the logging file for a line of WARNING
#define LOG_WARNING     if(Logger::canLogWarning())    Logger::logWarning()    << "[Warning] "

//! @brief provides a stream to the logging file for a line of STATS
#define LOG_STATS       if(Logger::canLogStats())      Logger::logStats()      << "[Stats] "

//! @brief provides a stream to the logging file for a line of ERROR
#define LOG_ERROR       if(Logger::canLogError())      Logger::logError()      << "[Error] "

//! @brief provides a stream to the logging file for a line of DEBUG
//! @param[in] lvl Level of debug of the message
#define LOG_DEBUG(lvl)  if(Logger::canLogDebug((lvl))) Logger::logDebug((lvl)) << "[Debug" << (lvl) << "] "

#endif /* end of include guard: _LOGGER_HPP_ */
