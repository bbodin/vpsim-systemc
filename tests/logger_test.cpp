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
 
#include <filesystem>
#include <gtest/gtest.h>
#include <logger/logger.hpp>
#include <logger/loggerCore.hpp>

using namespace vpsim;

int sc_main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(Logger, construction) {
    __attribute__((unused))
            Logger logger("testLoggerConstruction", "output.log");
    SUCCEED(); //Constructor did not crash: fine
}

TEST(Logger, name) {
    Logger logger("testLoggerName", "output.log");

    EXPECT_EQ("testLoggerName", logger.name());
}

TEST(Logger, logName) {
    Logger logger("testLoggerName", "output.log");

    EXPECT_EQ("output.log", logger.statLogName());
}

TEST(Logger, canLog) {
    Logger logger("testLoggerCanLog", "output.log");

    LoggerCore::get().enableLogging(false);
    EXPECT_FALSE(logger.canLogInfo());
    EXPECT_FALSE(logger.canLogWarning());
    EXPECT_FALSE(logger.canLogError());
    EXPECT_FALSE(logger.canLogStats());
    EXPECT_FALSE(logger.canLogDebug(dbg0));

    LoggerCore::get().enableLogging(true);
    EXPECT_TRUE(logger.canLogInfo());
    EXPECT_TRUE(logger.canLogWarning());
    EXPECT_TRUE(logger.canLogError());
    EXPECT_TRUE(logger.canLogStats());
    EXPECT_FALSE(logger.canLogDebug(dbg1));

    LoggerCore::get().setDebugLvl("testLoggerCanLog", dbg1);
    EXPECT_TRUE(logger.canLogDebug(dbg1));
}

TEST(Logger, getOftreams) {
    Logger logger("testLoggerGetOfstreams", "output.log");

    LoggerCore::get().enableLogging(false);
    EXPECT_FALSE(logger.logInfo().good());
    EXPECT_FALSE(logger.logWarning().good());
    EXPECT_FALSE(logger.logError().good());
    EXPECT_FALSE(logger.logStats().good());
    EXPECT_FALSE(logger.logDebug(dbg0).good());

    LoggerCore::get().enableLogging(true);
    EXPECT_TRUE(logger.logInfo().good());
    EXPECT_TRUE(logger.logWarning().good());
    EXPECT_TRUE(logger.logError().good());
    EXPECT_TRUE(logger.logStats().good());
    EXPECT_TRUE(logger.logDebug(dbg0).good());
    EXPECT_FALSE(logger.logDebug(dbg1).good());

    LoggerCore::get().setDebugLvl("testLoggerGetOfstreams", dbg1);
    EXPECT_TRUE(logger.logDebug(dbg1).good());
}

TEST(Logger, writeLog) {

    // most log are not in the log file, but in the standard output or a specific stream

    std::string logger_name = "testLoggerWriteLog";
    std::ostringstream oss;
    Logger logger(logger_name, "", oss);

    std::string lineIn, lineOut;
    LoggerCore::get().enableLogging(true);

    oss.str("");
    lineIn = "test log info";
    logger.logInfo() << lineIn << std::endl;
    lineOut = oss.str();
    EXPECT_EQ(lineIn + '\n', lineOut);

    oss.str("");
    lineIn =  "test log Warning";
    logger.logWarning() << lineIn << std::endl;
    lineOut = oss.str();
    EXPECT_EQ(lineIn + '\n', lineOut);

    oss.str("");
    lineIn =  "test log error";
    logger.logError() << lineIn << std::endl;
    lineOut = oss.str();
    EXPECT_EQ(lineIn + '\n', lineOut);


    oss.str("");
    lineIn =  "test log debug";
    logger.logDebug(dbg0) << lineIn << std::endl;
    lineOut = oss.str();
    EXPECT_EQ(lineIn + '\n', lineOut);


    // Stats are in a separate scope to make sure to saves the file.
    std::string outputlogfile = "testLoggerWriteLog.log";
    {
        // The log file should not exist...
        if (std::filesystem::exists(outputlogfile)) {
            std::filesystem::remove(outputlogfile);
        }

        Logger logger(logger_name + "bis", outputlogfile, oss); // Two loggers cannot have the same name
        EXPECT_FALSE(std::filesystem::exists(logger.statLogName()));
        lineIn =  "test log stats is here";
        logger.logStats() << lineIn << std::endl;
    }
    
    std::ifstream stats_ifstream(outputlogfile);
    if (std::getline(stats_ifstream, lineOut)) {
        // Successfully read a line
        std::cout << "Line: " << lineOut << "\n";
    } else {
        if (stats_ifstream.eof()) {
            std::cerr << "Reached end of file\n";
        } else if (stats_ifstream.fail()) {
            std::cerr << "Logical error while reading\n";
        } else if (stats_ifstream.bad()) {
            std::cerr << "Read/write error on i/o operation\n";
        }
    }
    EXPECT_EQ(lineIn, lineOut);


}

TEST(Logger, writeLogMacro) {
    class MyIp : public Logger {
    public:
        MyIp(std::string name, std::ostringstream& oss) : Logger(name, name + ".log", oss) {
        }

        void writeAnInfo() { LOG_INFO << "An info" << std::endl; }
        void writeAWarning() { LOG_WARNING << "A warning" << std::endl; }
        void writeAStat() { LOG_STATS << "A stat" << std::endl; }
        void writeAnError() { LOG_ERROR << "An error" << std::endl; }
        void writeADebug(DebugLvl lvl) { LOG_DEBUG(lvl) << "A debug" << std::endl; }
    };

    std::ostringstream oss;
    MyIp myIp("testLoggerWriteLogMacro", oss);
    LoggerCore::get().enableLogging(true);
    LoggerCore::get().setDebugLvl(myIp, dbg6);
    std::ifstream stats_ifstream(myIp.statLogName());

    std::string lineIn, lineOut;

    oss.str("");
    myIp.writeAnInfo();
    lineIn =  "[Info] An info";
    lineOut = oss.str();
    EXPECT_EQ(lineIn + '\n', lineOut);

    oss.str("");
    myIp.writeAWarning();
    lineIn =  "[Warning] A warning";
    lineOut = oss.str();
    EXPECT_EQ(lineIn + '\n', lineOut);

    oss.str("");
    myIp.writeAnError();
    lineIn =  "[Error] An error";
    lineOut = oss.str();
    EXPECT_EQ(lineIn + '\n', lineOut);

    oss.str("");
    myIp.writeADebug(dbg1);
    lineIn =  "[Debug1] A debug";
    lineOut = oss.str();
    EXPECT_EQ(lineIn + '\n', lineOut);

    oss.str("");
    myIp.writeADebug(dbg6);
    lineIn =  "[Debug6] A debug";
    lineOut = oss.str();
    EXPECT_EQ(lineIn + '\n', lineOut);

    
    myIp.writeAStat();
    lineIn =  "[Stats] A stat";
    std::getline(stats_ifstream, lineOut);
    EXPECT_EQ(lineIn, lineOut);

}


TEST(Logger, writeGlobalLogMacro) {
    std::ostringstream oss;
    LoggerCore::get().enableLogging(true);
    LoggerCore::get().setDebugLvl("globalLog", dbg6);
    std::ifstream stats_ifstream(globalLogger.statLogName());
    std::string lineIn, lineOut;

    // intercept cout
    std::streambuf* oldCoutBuf = std::cout.rdbuf();
    std::cout.rdbuf(oss.rdbuf());

    oss.str("");
    LOG_GLOBAL_INFO << "An info" << std::endl;
    lineIn =  "[Info] An info";
    lineOut = oss.str();
    EXPECT_EQ(lineIn + '\n', lineOut);

    oss.str("");
    LOG_GLOBAL_WARNING << "A warning" << std::endl;
    lineIn = "[Warning] A warning";
    lineOut = oss.str();
    EXPECT_EQ(lineIn + '\n', lineOut);


    oss.str("");
    LOG_GLOBAL_ERROR << "An error" << std::endl;
    lineIn = "[Error] An error";
    lineOut = oss.str();
    EXPECT_EQ(lineIn + '\n', lineOut);

    oss.str("");
    LOG_GLOBAL_DEBUG(dbg1) << "A debug" << std::endl;
    lineIn = "[Debug1] A debug";
    lineOut = oss.str();
    EXPECT_EQ(lineIn + '\n', lineOut);

    oss.str("");
    LOG_GLOBAL_DEBUG(dbg6) << "A debug" << std::endl;
    lineIn = "[Debug6] A debug";
    lineOut = oss.str();
    EXPECT_EQ(lineIn + '\n', lineOut);

    // Restore original buffer
    std::cout.rdbuf(oldCoutBuf);

    
    LOG_GLOBAL_STATS << "A stat" << std::endl;
    lineIn =  "[Stats] A stat";
    std::getline(stats_ifstream, lineOut);
    EXPECT_EQ(lineIn , lineOut);


}
