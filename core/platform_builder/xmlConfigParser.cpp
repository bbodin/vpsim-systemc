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

#include <string>
#include <log.hpp>
#include <filesystem>
#include "platform_builder/xmlConfigParser.hpp"


namespace vpsim {
    XmlConfigParser::XmlConfigParser(const std::string &xmlFile) : mSource(xmlFile.c_str()),
                                                                   mXml() {
        mXml.parse<0>(mSource.data());
    }

    bool XmlConfigParser::read() {
        try {
            rapidxml::xml_node<> *node = mXml.first_node("vpsim");
            if (node && (std::string(node->name()) == "vpsim")) {
                readVpsim(node);
            } else {
                XmlConfigParser::unsupportedXmlFile("Unsupported node: " + std::string(node->name()));
            }
        } catch (const std::exception &e) {
            LOG_GLOBAL_ERROR << e.what() << std::endl;
            std::cerr << e.what() << std::endl;
            throw;
        }
        return true;
    }

    void XmlConfigParser::readVpsim(rapidxml::xml_node<> *node) {
        const char *source = node->first_attribute("source")->value();
        if (source && std::string(source) == "python") {
            readFromPythonXml();
        } else {
            std::cerr << "Only python generated VPSim xml files are supported" << std::endl;
        }
    }

    void XmlConfigParser::readFromPythonXml() {
        assert(mXml.first_node("vpsim"));
        
        // Read 
        rapidxml::xml_node<> *node = mXml.first_node("vpsim")->first_node("simulation");
        if (node && std::string(node->name()) == "simulation") {
            readSimulation(node);
        } else {
            XmlConfigParser::unsupportedXmlFile("Unsupported node: " + std::string(node->name()));
        }

        node = mXml.first_node("vpsim")->first_node("platform");
        if (node && std::string(node->name()) == "platform") {
            readPlatform(node);
        } else {
            XmlConfigParser::unsupportedXmlFile("Unsupported node: " + std::string(node->name()));
        }
        
    }

    void XmlConfigParser::readPlatform(rapidxml::xml_node<> *platformNode) {
        assert(platformNode->first_node("ips"));

        rapidxml::xml_node<> *ipsNode = platformNode->first_node("ips");
        if (ipsNode) {
            readIps(ipsNode);
        } else {
            XmlConfigParser::unsupportedXmlFile("Node is not found: ips");
        }

        rapidxml::xml_node<> *linksNode = platformNode->first_node("links");
        if (linksNode) {
            readLinks(linksNode);
        } else {
            XmlConfigParser::unsupportedXmlFile("Node is not found: links");
        }

        mBuilder.finalize();
    }


    void XmlConfigParser::readIps(rapidxml::xml_node<> *ipsNode) {
        assert(ipsNode && std::strcmp(ipsNode->name(), "ips") == 0);

        for (rapidxml::xml_node<> *ipNode = ipsNode->first_node(); ipNode; ipNode = ipNode->next_sibling()) {
            std::string ipName(ipNode->name());
            std::string name(ipNode->first_attribute("name")->value());

            mBuilder.beginBuild(ipName, name);
            readIpAttributes(ipNode);
            mBuilder.endBuild();

            if (ipName == "SmartUart") {
                mSmartUartName = name;
            }
            if (ipName == "CallbackRegister32" || ipName == "CallbackRegister64") {
                mCallbackRegisterType = ipName;
                mCallbackRegisterName = name;
            }
        }
    }

    void XmlConfigParser::readLinks(rapidxml::xml_node<> *linksNode) {
        assert(linksNode && std::strcmp(linksNode->name(), "links") == 0);
        for (rapidxml::xml_node<> *linkNode = linksNode->first_node(); linkNode; linkNode = linkNode->next_sibling()) {
            if (std::string(linkNode->name()) == "link") {
                readLink(linkNode);
            } else {
                XmlConfigParser::unsupportedXmlFile("Node is not found: link");
            }
        }
    }

    void XmlConfigParser::readIpAttributes(rapidxml::xml_node<> *ipNode) {
        for (rapidxml::xml_node<> *attrNode = ipNode->first_node(); attrNode; attrNode = attrNode->next_sibling()) {
            std::string attr(attrNode->name());
            std::string value(attrNode->value());
            mBuilder.setAttribute(attr, value);
        }
    }

    void XmlConfigParser::readLink(rapidxml::xml_node<> *linkNode) {
        assert(linkNode && std::strcmp(linkNode->name(), "link") == 0);

        std::string fromName, fromPort, toName, toPort;
        rapidxml::xml_node<> *fromNode = linkNode->first_node("from");
        if (fromNode) {
            fromPort = std::string(fromNode->first_attribute("port")->value());
            fromName = std::string(fromNode->value());
        } else {
            XmlConfigParser::unsupportedXmlFile("Node is not found: from");
        }

        rapidxml::xml_node<> *toNode = linkNode->first_node("to");
        if (toNode) {
            toPort = std::string(toNode->first_attribute("port")->value());
            toName = std::string(toNode->value());
        } else {
            XmlConfigParser::unsupportedXmlFile("Node is not found: to");
        }

        mBuilder.connect(fromName, fromPort, toName, toPort);
    }

    void readWorkingDirectory(rapidxml::xml_node<> *node) {
        std::string dir = std::string(node->value());
        if (!dir.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            if (ec) {
                std::cerr << "[WARNING] Failed to create working directory '" << dir << "': " << ec.message() << std::endl;
            }
            std::filesystem::current_path(dir, ec);
            if (ec) {
                std::cerr << "[WARNING] Failed to change working directory to '" << dir << "': " << ec.message() << std::endl;
            } else {
                std::cout << "[INFO] working directory set to '" << std::filesystem::current_path().string() << "'" << std::endl;
            }
        }
    }
    
    void readTraceFile(rapidxml::xml_node<> *node) {
    }

    
    void readStatsFile(rapidxml::xml_node<> *node) {
        
        std::string filename = std::string(node->value());
            if (!filename.empty()) {
            std::error_code ec;
            std::filesystem::path dir = std::filesystem::path(filename).parent_path(); // extracts "/path/to/some"
            std::filesystem::create_directories(dir, ec);
            if (ec) {
                std::cerr << "[WARNING] Failed to create log directory '" << dir << "': " << ec.message() << std::endl;
            }

            if (globalLogger.setStatLogName(filename)) {
                std::cout << "[INFO] Stats logging file set to '" << globalLogger.statLogName() << "'" << std::endl;
            } else {
                std::cerr << "[WARNING] Failed to change  Stats logging file to '" << filename  << std::endl;
            }
        }
    }


    void readLoggingLevel(rapidxml::xml_node<> *node) {
        
        std::string val = node->value();
        vpsim::DebugLvl level;
        if (val == "enable") {
            LoggerCore::get().enableLogging(true);
        } else if (val == "disable") {
            LoggerCore::get().enableLogging(false);
        } else {
                     try {
                        int lvl = std::stoi(val);
                        level = static_cast<vpsim::DebugLvl>(lvl);
                        LoggerCore::get().setDebugLvl(level);
                        LoggerCore::get().enableLogging(true);
                    }
                    catch (...) {
                        // Handle unexpected input
                        std::cerr << "Invalid logger setting: '" << val << "'\n";
                    }
        }

        LOG_GLOBAL_DEBUG(dbg0) << "Logging debug level 0 or more is working" << endl;
        LOG_GLOBAL_INFO << "Logging info is working" << endl;
        LOG_GLOBAL_WARNING << "Logging warning is working" << endl;
        LOG_GLOBAL_ERROR << "Logging error is working" << endl;

    }

    void XmlConfigParser::readSimulation(rapidxml::xml_node<> *node) {
        assert(node && std::strcmp(node->name(), "simulation") == 0);
        for (rapidxml::xml_node<> *simNode = node->first_node(); simNode; simNode = simNode->next_sibling()) {
            std::string simNodeName(simNode->name());
            if (simNodeName == "quantum") {
                //simNode->skip_children();
                cerr << "Global quantum is not currently supported" << endl;
                LOG_GLOBAL_INFO << "Global quantum is not currently supported" << endl;
            } else if (simNodeName == "log_level") {
                readLoggingLevel(simNode);
            } else if (simNodeName == "defaultBlockingTLM") {
                auto defaultBTLM = std::string(simNode->value()) == "enable"
                                       ? BlockingTLMEnabledParameter::BT_ENABLED
                                       : BlockingTLMEnabledParameter::BT_DISABLED;
                BlockingTLMEnabledParameter::setDefault(defaultBTLM);
            } else if (simNodeName == "stats_file") {
               readStatsFile(simNode);
            } else if (simNodeName == "working_directory") {
                readWorkingDirectory(simNode);
            } else if (simNodeName == "logSchedule") {
                readLogSchedule(simNode);
            } else if (simNodeName == "blockingTLMSchedule") {
                readBlockingTLMSchedule(simNode);
            } else if (simNodeName == "callback") {
                readCallback(simNode);
            } else {
                XmlConfigParser::unsupportedXmlFile("Unsupported node: " + simNodeName);
            }
        }
    }

    void XmlConfigParser::readLogSchedule(rapidxml::xml_node<> *simNode) {
        assert(simNode && std::strcmp(simNode->name(), "logSchedule") == 0);

        bool stringTriggered = false;
        std::string ipName;
        DebugLvl dbgLvl{dbg0};
        sc_time triggerTime = SC_ZERO_TIME;
        std::string triggerString;
        for (rapidxml::xml_node<> *logNode = simNode->first_node(); logNode; logNode = logNode->next_sibling()) {
            std::string logNodeName(logNode->name());
            if (logNodeName == "timeTrigger") {
                stringTriggered = false;
                triggerTime = sc_time(std::stoul(logNode->value()), SC_PS);
            } else if (logNodeName == "stringTrigger") {
                //TODO: Handle string trigger with smart UART
                stringTriggered = true;
                triggerString = std::string(logNode->value());
            } else if (logNodeName == "ipName") {
                ipName = std::string(logNode->value());
            } else if (logNodeName == "debugLevel") {
                const auto &dbgLvlVec = std::vector<DebugLvl>({dbg0, dbg1, dbg2, dbg3, dbg4, dbg5, dbg6});
                dbgLvl = dbgLvlVec.at(std::stoul(logNode->value()));
            } else {
                XmlConfigParser::unsupportedXmlFile("Unsupported node: " + logNodeName);
            }
        }

        if (stringTriggered) {
            //TODO: Handle string trigger with smart UART
        } else {
            LoggerCore::get().addAppointment(ipName, triggerTime, dbgLvl);
        }
    }

    void XmlConfigParser::readBlockingTLMSchedule(rapidxml::xml_node<> *simNode) {
        assert(simNode && std::strcmp(simNode->name(), "blockingTLMSchedule") == 0);
        bool stringTriggered = false,
                timeTriggered = false,
                withAddrRange = false;
        string ipName;
        unique_ptr<ModuleParameter> param;
        sc_time triggerTime = SC_ZERO_TIME;
        string triggerString;
        AddrSpace as;

        for (rapidxml::xml_node<> *logNode = simNode->first_node(); logNode; logNode = logNode->next_sibling()) {
            std::string logNodeName(logNode->name());

            if (logNodeName == "timeTrigger") {
                timeTriggered = true;
                triggerTime = sc_time(std::stoul(logNode->value()), SC_PS);
            } else if (logNodeName == "stringTrigger") {
                stringTriggered = true;
                triggerString = std::string(logNode->value());
            } else if (logNodeName == "ipName") {
                ipName = std::string(logNode->value());
            } else if (logNodeName == "blockingTLM") {
                param = (std::string(logNode->value()) == "enable")
                            ? BlockingTLMEnabledParameter::bt_enabled.clone()
                            : BlockingTLMEnabledParameter::bt_disabled.clone();
            } else if (logNodeName == "addrRange") {
                withAddrRange = true;
                as = readAddrRange(logNode);
            } else {
                XmlConfigParser::unsupportedXmlFile("Unsupported node: " + logNodeName);
            }
        }

        if (stringTriggered && timeTriggered) {
            XmlConfigParser::unsupportedXmlFile("String and time triggers cannot be used together");
        }

        if (stringTriggered) {
            auto &smartUart = *VpsimIp<InPortType, OutPortType>::AllInstances.at("SmartUart").at(mSmartUartName);

            if (withAddrRange) {
                smartUart.registerStringParamTrigger(triggerString, ipName, as, *param);
            } else {
                smartUart.registerStringParamTrigger(triggerString, ipName, *param);
            }
        } else if (timeTriggered) {
            if (withAddrRange) {
                ParamManager::get().addAppointment(ipName, as, triggerTime, *param);
            } else {
                ParamManager::get().addAppointment(ipName, triggerTime, *param);
            }
        } else {
            if (withAddrRange) {
                ParamManager::get().setParameter(ipName, as, *param);
            } else {
                ParamManager::get().setParameter(ipName, *param);
            }
        }
    }

    AddrSpace XmlConfigParser::readAddrRange(rapidxml::xml_node<> *addrNode) {
        assert(addrNode && std::strcmp(addrNode->name(), "addrRange") == 0);

        uint64_t b{0}, e{0};
        for (rapidxml::xml_node<> *node = addrNode->first_node(); node; node = node->next_sibling()) {
            std::string nodeName(node->name());
            if (nodeName == "start") {
                b = std::stoul(node->value());
            } else if (nodeName == "end") {
                e = std::stoul(node->value());
            } else {
                throw std::runtime_error("Invalid XML element in addrRange");
            }
        }
        return AddrSpace(b, e);
    }

    void XmlConfigParser::readCallback(rapidxml::xml_node<> *callbackNode) {
        assert(callbackNode && std::strcmp(callbackNode->name(), "callback") == 0);

        auto &callbackRegister = *VpsimIp<InPortType, OutPortType>::AllInstances.at(mCallbackRegisterType).at(
            mCallbackRegisterName);
        uint64_t val{0};
        std::string callback;

        for (rapidxml::xml_node<> *node = callbackNode->first_node(); node; node = node->next_sibling()) {
            std::string nodeName(node->name());
            if (nodeName == "value") {
                val = std::stoul(node->value());
            } else if (nodeName == "call") {
                callback = std::string(node->value());
            } else {
                throw std::runtime_error("Invalid XML element in callback");
            }
        }
        callbackRegister.registerCallback(val, callback);
    }

    void XmlConfigParser::unsupportedXmlFile(const string &error_msg) {
        std::cerr << "Unsupported VPSim xml file:" << error_msg << std::endl;
    }
}
