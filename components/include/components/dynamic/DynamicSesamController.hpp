#ifndef VPSIM_DYNAMIC_DYNAMICSESAMCONTROLLER_HPP
#define VPSIM_DYNAMIC_DYNAMICSESAMCONTROLLER_HPP

#include <vpsimModule/VpsimIp.hpp>
#include <core/TargetIf.hpp>
#include <peripherals/SesamController.hpp>
#include <components/MainMemCosim.hpp>
#include <peripherals/ChannelManager.hpp>
#include <components/dynamic/DynamicSystemCCosimulator.hpp>

#include <sstream>
#include <stdexcept>
#include <atomic>
#include <csignal>
#include <logger/loggerCore.hpp>

namespace vpsim {

    typedef tlm::tlm_target_socket<> InPortType;
    typedef tlm::tlm_initiator_socket<> OutPortType;

    struct DynamicSesamController final :
            public VpsimIp<InPortType, OutPortType>,
            public SesamController {
    private:
        monitorState mState;
        uint64_t mBytesPerLine;
        uint32_t mCurrentDomain;
        static bool InstanceExists;
        bool mValid;

        SystemCCosimulator *MainMemPtr;

        // benchmarking data
        string appName;
        bool mInBenchmark;
        uint32_t mBenchDomain;
        sc_time mBenchStartTime;

        // checkpoint related data
        map<string, int> mCheckpoints;

    public:
        explicit DynamicSesamController(const std::string& name) : VpsimIp(name),
                                                            SesamController(name.c_str()) {
            
            VpsimIp<InPortType, OutPortType>::registerOptionalAttribute("log_directory", "");
            VpsimIp<InPortType, OutPortType>::registerRequiredAttribute("base_address");
            VpsimIp<InPortType, OutPortType>::registerOptionalAttribute("size", "4");


            mState = RUN;
            mBytesPerLine = 8;
            if (!InstanceExists) {
                InstanceExists = true;
                mValid = true;
            } else {
                mValid = false;
            }
            mInBenchmark = false;
            MainMemPtr = nullptr;
        }

        SC_HAS_PROCESS(DynamicSesamController);

        ~DynamicSesamController() override {
        }

        N_IN_PORTS_OVERRIDE(1);
        N_OUT_PORTS_OVERRIDE(0);
        MEMORY_MAPPED_OVERRIDE;

        InPortType *getNextInPort() override {
            return &mTargetSocket;
        }

        OutPortType *getNextOutPort() override {
            throw runtime_error(VpsimIp::getName() + " : SesamController has no out sockets.");
        }

        void make() override {
            checkAttributes();
            setBaseAddress(getAttrAsUInt64("base_address"));
            setPtrState(&mState);
            mCurrentDomain = this->getAttrAsUInt64("domain");
            LOG_GLOBAL_INFO << "DynamicSesamController in the making, getLogDirectory is '" << this->getLogDirectory() << "'" << std::endl;
        }

        virtual uint64_t getBaseAddress() override {
            return getAttrAsUInt64("base_address");
        }

        virtual uint64_t getSize() override {
            return getAttrAsUInt64("size");
        }

        std::string getLogDirectory() {
                try {
                return getAttr("log_directory");
            } catch (std::runtime_error& e) {
                return "";
            }
        }

        static bool ready() {
            return ChannelManager::fdCheckReady(0);
        }

        static string getLine() {
            char line[1024];
            fgets(line, 1024, stdin);
            return string(line);
        }

        static string prompt() {
            stringstream ss;
            ss << "\n" << "@" << sc_time_stamp();
            printf("%s sesam # ", ss.str().c_str());
            return getLine();
        }

        static vector<string> getArgv(istringstream &ss) {
            vector<string> argv;
            ss >> std::skipws;
            string curr;
            while (ss >> curr) {
                argv.push_back(curr);
            }
            return argv;
        }

        void finalize() override {
            VpsimIp *ip = VpsimIp::Find("SystemCCosim0");
            if (ip) {
                const auto cosim = dynamic_cast<DynamicSystemCCosimulator *>(ip);
                MainMemPtr = cosim->mModulePtr;
                cosim->mModulePtr->setMonitorPtr(this);
            };
        }

        void process_start_capture() const {
            VpsimIp::MapIf(
                [this](VpsimIp *ip) {
                    return (ip->getAttrAsUInt64("domain") == this->mCurrentDomain && ip->getDelayStatCapture());
                }, // Delayed IPs
                [](VpsimIp *ip) {
                    ip->pushStats();
                }
            );
        }

        void process_end_capture(size_t counter) {
            // Use the same logger formatting as the global log to ensure consistency
            const std::string baseName = this->getLogDirectory() + "/" + std::string("sesamCapture_") + appName + std::string("_") + std::to_string(counter - 1);
            
            LOG_GLOBAL_INFO << "inside process_end_capture " << std::endl;
            LOG_GLOBAL_INFO << "Logdir is " << this->getLogDirectory() << std::endl;
            LOG_GLOBAL_INFO << "End of capture, saved to " << baseName << std::endl;

            vpsim::Logger benchLogger(baseName);

            // 1) Dump stats for delayed IPs participating in the benchmark
            VpsimIp::MapIf(
                [this](VpsimIp *ip) {
                    return (ip->getAttrAsUInt64("domain") == this->mBenchDomain && ip->getDelayStatCapture());
                }, // Delayed IPs
                [&benchLogger](VpsimIp *ip) {
                    ip->pushStats();
                    auto &stats = ip->getSegStats().back();
                    if (!stats.empty()) {
                        for (auto &stat: stats) {
                            VpsimIp::WriteStatToLogger(benchLogger, ip->getName(), stat.first, stat.second);
                        }
                        ip->clearSegStats();
                    }
                }
            );

            // 2) Dump stats for non-delayed IPs (e.g., CPUs) as well to keep previous behavior
            VpsimIp::MapIf(
                [this](VpsimIp *ip) {
                    return (ip->getAttrAsUInt64("domain") == this->mBenchDomain && !ip->getDelayStatCapture());
                }, // Non delayed IPs
                [&benchLogger](VpsimIp *ip) {
                    ip->pushStats();
                    auto &stats = ip->getSegStats().back();
                    if (!stats.empty()) {
                        for (auto &stat: stats) {
                            VpsimIp::WriteStatToLogger(benchLogger, ip->getName(), stat.first, stat.second);
                        }
                        ip->clearSegStats();
                    }
                }
            );

            delayedCaptureRunning = false;
        }

        void end_benchmark() {

            mInBenchmark = false;
            sc_time diff;

            if (MainMemPtr) {
                MainMemPtr->NotifySesamCommand(nbCommandCounter + 1, false);
                diff = MainMemPtr->getCurrentTime() - mBenchStartTime;
            } else diff = sc_time_stamp() - mBenchStartTime;


            // get and display stats
            //printf("Checking all IPs of domain %ld\n", this->mBenchDomain);
            //fflush(stdout);

            mCommandOutputBuffer = string();
            stringstream ss;
            ss << diff;
            mCommandOutputBuffer += "Simulated time: ";
            mCommandOutputBuffer += ss.str() + "\n";

            VpsimIp::MapIf(
                [this](VpsimIp *ip) {
                    //printf("ip %s is of domain %ld\n", ip->getName().c_str(),ip->getAttrAsUInt64("domain"));
                    return (ip->getAttrAsUInt64("domain") == this->mBenchDomain && !ip->
                            getDelayStatCapture()); // Non delayed IPs
                },
                [this](VpsimIp *ip) {
                    // Go back to fast mode !
                    /*if (ip->isMemoryMapped() && ip->hasDmi()) {
                                          AddrSpace as(ip->getBaseAddress(),ip->getBaseAddress()+ip->getSize()-1);
                                           ParamManager::get().setParameter(ip->getName(), as, BlockingTLMEnabledParameter::bt_disabled);
                                 }*/
                    // now segment stats
                    ip->pushStats();

                    auto &stats = ip->getSegStats().back();



                    if (stats.size()) {
                        mCommandOutputBuffer += "-----------------------------------\n";
                        mCommandOutputBuffer += "\nStatistics from ";
                        mCommandOutputBuffer += ip->getName() + "\n";
                        for (auto &stat: stats) {
                            mCommandOutputBuffer += "\t";
                            mCommandOutputBuffer += stat.first + " = ";
                            mCommandOutputBuffer += stat.second + "\n";
                        }
                        ip->clearSegStats();
                    }
                }
            );


            std::string baseName = this->getLogDirectory() + "/sesamBench_" + appName + "_" + std::to_string(nbCommandCounter++) + ".log";

            LOG_GLOBAL_INFO << "inside end_benchmark " << std::endl;
            LOG_GLOBAL_INFO << "Logdir is " << this->getLogDirectory() << std::endl;
            LOG_GLOBAL_INFO << "Saving ..." << std::endl;

            std::FILE *LogFile = fopen(baseName.c_str(), "w");
            fprintf(LogFile, "%s", mCommandOutputBuffer.c_str());
            fclose(LogFile);
            LOG_GLOBAL_INFO << "End of capture, saved to " << baseName << std::endl;
        }

        static void process_quit_command() {
            sc_stop();
            return;
        }

        static bool process_show_cmd(const vector<string> &args) {
            LOG_GLOBAL_INFO << "Sesam Controller process the SHOW command." << std::endl;
            if (args.size() - 1 < 1) {
                printf("Usage: show component1_name component2_name ...\n");
                return true;
            }
            for (unsigned i = 1; i < args.size(); i++) {
                string component = args.at(i);
                VpsimIp *ip = VpsimIp::Find(component);
                if (!ip) {
                    LOG_GLOBAL_ERROR << "Component " << component << " not known to VPSim.\n";
                    printf("Error: Component %s not known to VPSim.\n", component.c_str());
                } else {
                    ip->show();
                }
            }
            return false;
        }

        bool process_showmem_cmd(const vector<string> &args) const {
            LOG_GLOBAL_INFO << "Sesam Controller process the SHOWMEM command." << std::endl;
            if (args.size() - 1 != 2) {
                printf("Usage: showmem start_addr size\n");
                return true;
            }
            bool covered = false;
            istringstream st(args.at(1));
            istringstream sz(args.at(2));
            uint64_t start, size;
            st >> hex >> start;
            sz >> hex >> size;

            while (!covered) {
                uint64_t end = start + size - 1;
                bool found = false;
                VpsimIp::MapIf([this,&start]
                       (VpsimIp *ip) {
                                   return ip->getAttrAsUInt64("domain") == this->mCurrentDomain
                                          && ip->isMemoryMapped()
                                          && ip->getActualAddress() != nullptr
                                          && ip->getActualAddress() != reinterpret_cast<unsigned char *>(-1)
                                          && ip->getBaseAddress() <= start
                                          && start < ip->getBaseAddress() + ip->getSize();
                               }
                               ,
                               [this,&start,&size,&found,&end ](VpsimIp *ip) {
                                   uint64_t actualEnd = std::min(
                                       end, ip->getBaseAddress() + ip->getSize() - 1);
                                   uint64_t actualSize = actualEnd - start + 1;
                                   uint64_t lines =  actualSize <= mBytesPerLine ? 1 : actualSize / mBytesPerLine;
                                   uint64_t left = actualSize;
                                   // getActualAddress and getBaseAddress are not aligned, we need to shift
                                   uint64_t offset = start - ip->getBaseAddress();
                                   found = true;
                                   LOG_GLOBAL_INFO << std::hex 
                                                    << "Address " << start 
                                                    << " of " << size 
                                                    << " found in " << ip->getName() 
                                                    << " with base address = " << ip->getBaseAddress() 
                                                    << std::dec // reset to decimal
                                                    << std::endl;
                                   for (uint64_t i = 0; i < lines; i++) {
                                       printf("\n%016" PRIX64 "\t", start + i * mBytesPerLine);
                                       uint64_t Left = std::min(mBytesPerLine, left);
                                       for (uint64_t j = 0; j < Left; j++) {
                                           printf("%02X\t",
                                                  ip->getActualAddress()[offset + i * mBytesPerLine + j]);
                                           left--;
                                       }
                                   }
                                   start = actualEnd + 1;
                               }
                );
                if (!found) {
                    printf("\nWarning: address space %" PRIu64 " to %" PRIu64 " not covered.\n", start, end);
                    
                    break;
                }
                if (start > end) {
                    covered = true;
                }
            }
            printf("\n");
            return false;
        }

        bool process_list_cmd(const vector<string> &args) {
            if (args.size() - 1 != 0) {
                printf("Usage: list\n");
                return true;
            }
            mCommandOutputBuffer = string();

            VpsimIp::MapIf(
                [this](VpsimIp *ip) { return ip->getAttrAsUInt64("domain") == this->mCurrentDomain; },
                // all IPs
                [this](VpsimIp *ip) {
                    mCommandOutputBuffer += ip->getName() + "\n";
                }
            );
            return false;
        }

        static bool process_config_cmd(const vector<string> &args) {
            // TODO : This does nothing meaningful, the parameter value are not even passed.
            if (args.size() - 1 < 3) {
                printf("Usage: configure component_family parameter value\n");
                return true;
            }

            string component = args.at(1);
            VpsimIp *ip = VpsimIp::Find(component);
            if (!ip) {
                printf("Error: Component %s not known to VPSim.\n", component.c_str());
            } else {
                // change component configuration
                ip->configure();
            }
            return false;
        }


        static bool process_debug_cmd(const vector<string> &args) {
            if (args.size() - 1 < 2) {
                printf("Usage: debug lvl component1 component2 component3 ... \n");
                return true;
            }
            istringstream lv(args[1]);
            uint64_t lvl;
            lv >> lvl;
            for (unsigned i = 2; i < args.size(); i++) {
                LoggerCore::get().enableLogging(true);
                LoggerCore::get().setDebugLvl(args[i], static_cast<DebugLvl>(lvl));

                // Also make sure accesses reach the component.
            }
            return false;
        }

        bool process_watch_cmd(const vector<string> &args) const {
            if (args.size() - 1 < 2) {
                printf("Usage: watch base size\n");
                return true;
            }
            istringstream st(args.at(1));
            istringstream sz(args.at(2));
            uint64_t start, size;
            st >> hex >> start;
            sz >> hex >> size;

            LOG_GLOBAL_INFO << "Now monitoring following ranges: \n";


            // Ask all IPs to watch out for this address space !
            VpsimIp::MapIf(
                [this](VpsimIp *ip) { return ip->getAttrAsUInt64("domain") == this->mCurrentDomain; },
                [&start,&size](VpsimIp *ip) {
                    const AddrSpace as(start, start + size - 1);
                    // First make sure accesses reach their targets (i.e. No DMI !)
                    try {
                        ParamManager::get().setParameter(
                            ip->getName(), as, BlockingTLMEnabledParameter::bt_enabled);
                    } catch (exception &ex) {
                        LOG_GLOBAL_WARNING << "Fail bt_enabled with " << ip->getName() << std::endl;
                    }

                    ip->addMonitor(start, size);
                    ip->showMonitor();
                }
            );
            return false;
        }

        bool process_unwatch_cmd(const vector<string> &args) const {
            if (args.size() - 1 < 2) {
                printf("Usage: unwatch base size\n");
                return true;
            }
            istringstream st(args.at(1));
            istringstream sz(args.at(2));
            uint64_t start, size;
            st >> hex >> start;
            sz >> hex >> size;

            LOG_GLOBAL_INFO << "Now monitoring following ranges: \n";

            // Ask all IPs to watch out for this address space !
            VpsimIp::MapIf(
                [this](VpsimIp *ip) { return ip->getAttrAsUInt64("domain") == this->mCurrentDomain; },
                // all IPs
                [&start,&size](VpsimIp *ip) {
                    const AddrSpace as(start, start + size - 1);
                    // First make sure accesses reach their targets (i.e. No DMI !)

                    // broken, should use default param instead.
                    try {
                        if (ip->hasDmi())
                            ParamManager::get().setParameter(
                                ip->getName(), as, BlockingTLMEnabledParameter::bt_disabled);
                    } catch (exception &ex) {
                    }
                    ip->removeMonitor(start, size);
                    ip->showMonitor();
                }
            );
            return false;
        }

        bool process_benchmark_cmd(const vector<string> &args) {

            if (args.size() - 2 != 0) {
                printf("Usage: benchmark app\n");
                return true;
            }

            if (delayedCaptureRunning) {
                // Tests did not show any occurrence of overlapping "sesam benchmark" commands
                LOG_GLOBAL_ERROR << "Wait for the previous benchmark counters to be captured entirely!\n";
                fprintf(stderr, "Wait for the previous benchmark counters to be captured entirely!\n");
                return true;
            }

            if (MainMemPtr) {
                delayedCaptureRunning = true;
                MainMemPtr->NotifySesamCommand(nbCommandCounter + 1, true);
                mBenchStartTime = MainMemPtr->getCurrentTime();
            } else mBenchStartTime = sc_time_stamp();

            // First, create a new stats segment
            VpsimIp::MapIf(
                [this](VpsimIp *ip) {
                    return (ip->getAttrAsUInt64("domain") == this->mCurrentDomain && !ip->
                            getDelayStatCapture());
                }, // Non delayed IPs
                [](VpsimIp *ip) {
                  
                    ip->pushStats();
                    //printf("Push ok.\n");
                }
            );
            appName = args.at(1);
            mInBenchmark = true;
            mBenchDomain = mCurrentDomain;
            
            //fprintf(stderr, "Benchmark mode started.\n");
            LOG_GLOBAL_INFO << "Benchmark mode started" << std::endl;
            return false;
        }

        
        bool process_snapshot_cmd(const vector<string> &args) {

            if (args.size() != 2) {
                printf("Usage: snapshot app\n");
                LOG_GLOBAL_ERROR << "argument count given to snapshot is " << args.size() << std::endl;
                for (auto arg : args) {
                    LOG_GLOBAL_ERROR << " - " << arg << std::endl;
                }
                return true;
            }

            if (MainMemPtr) {
                delayedCaptureRunning = true;
                MainMemPtr->NotifySesamCommand(nbCommandCounter + 1, true);
                mBenchStartTime = MainMemPtr->getCurrentTime();
            } else mBenchStartTime = sc_time_stamp();


            appName = args.at(1);
            //mInBenchmark = true;
            mBenchDomain = mCurrentDomain;
            

            mCommandOutputBuffer = string();
            stringstream ss;
            ss << mBenchStartTime;
            mCommandOutputBuffer += "Snapshot time: ";
            mCommandOutputBuffer += ss.str() + "\n";

            VpsimIp::MapIf(
                [this](VpsimIp *ip) {
                    //printf("ip %s is of domain %ld\n", ip->getName().c_str(),ip->getAttrAsUInt64("domain"));
                    return (ip->getAttrAsUInt64("domain") == this->mBenchDomain && !ip->
                            getDelayStatCapture()); // Non delayed IPs
                },
                [this](VpsimIp *ip) {
                    ip->pushStats();
                    auto &stats = ip->getSegStats().back();

                    if (stats.size()) {
                        mCommandOutputBuffer += "-----------------------------------\n";
                        mCommandOutputBuffer += "\nStatistics from ";
                        mCommandOutputBuffer += ip->getName() + "\n";
                        for (auto &stat: stats) {
                            mCommandOutputBuffer += "\t";
                            mCommandOutputBuffer += stat.first + " = ";
                            mCommandOutputBuffer += stat.second + "\n";
                        }
                        //ip->clearSegStats();
                    }
                }
            );


            std::string baseName = this->getLogDirectory() + "/sesamSnap_" + appName + "_" + std::to_string(nbCommandCounter++) + ".log";

            LOG_GLOBAL_INFO << "inside snapshot " << std::endl;
            LOG_GLOBAL_INFO << "Logdir is " << this->getLogDirectory() << std::endl;
            LOG_GLOBAL_INFO << "Saving ..." << std::endl;

            std::FILE *LogFile = fopen(baseName.c_str(), "w");
            fprintf(LogFile, "%s", mCommandOutputBuffer.c_str());
            fclose(LogFile);
            LOG_GLOBAL_INFO << "End of capture, saved to " << baseName << std::endl;

            return false;
            
        }


        // Attention: 'counter' manages only one domain, mCurrentDomain and mBenchDomain are then equal
        void sesamCommand(vector<string> &args, size_t counter) override {
            if (counter) {
                // counter != 0 when sesamComand is called by MainMem
                string cmd = args.at(0);
                if (cmd == "StartCapture") {
                    this->process_start_capture();
                } else if (cmd == "EndCapture") {
                    this->process_end_capture(counter);
                } else {
                    // unknown cmd
                }
            } else {
                switch (mState) {
                    case RUN: {
                        if (mInBenchmark) {
                            end_benchmark();
                        } else {
                            LOG_GLOBAL_WARNING << "Unknow situation state is RUN with arg " <<  args.at(0) << std::endl;
                        }
                    }
                        break;
                    case TAKE_CMD: {
                        string cmd = args.at(0);
                        if (cmd == "quit") {
                            process_quit_command();
                        } else if (cmd == "show") {
                            if (process_show_cmd(args)) return;
                        } else if (cmd == "showmem") {
                            if (process_showmem_cmd(args)) return;
                        } else if (cmd == "list") {
                            if (process_list_cmd(args)) return;
                        } else if (cmd == "configure") {
                            if (process_config_cmd(args)) return;
                        } else if (cmd == "debug") {
                            if (process_debug_cmd(args)) return;
                        } else if (cmd == "watch") {
                            if (process_watch_cmd(args)) return;
                        } else if (cmd == "unwatch") {
                            if (process_unwatch_cmd(args)) return;
                        } else if (cmd == "benchmark") {
                            if (process_benchmark_cmd(args)) return;
                        } else if (cmd == "snapshot") {
                            if (process_snapshot_cmd(args)) return;
                        } else {
                            LOG_GLOBAL_WARNING << "Unknow command " << cmd << std::endl;
                        }
                    }
                        break;
                    default:
                        throw runtime_error("SesamController in unknown state.");
                }
            }
        }


    };
}

#endif  // VPSIM_DYNAMIC_DYNAMICSESAMCONTROLLER_HPP
