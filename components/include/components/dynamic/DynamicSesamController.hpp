#ifndef VPSIM_DYNAMIC_DYNAMICSESAMCONTROLLER_HPP
#define VPSIM_DYNAMIC_DYNAMICSESAMCONTROLLER_HPP
#include <sstream>

#include <atomic>
#include <csignal>
#include "VpsimIp.hpp"
#include "TargetIf.hpp"
#include "SesamController.hpp"
#include "RemoteInitiator.hpp"
#include "MainMemCosim.hpp"


namespace vpsim {
    typedef tlm::tlm_target_socket<> InPortType;
    typedef tlm::tlm_initiator_socket<> OutPortType;

    struct DynamicSesamController :
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
            registerRequiredAttribute("base_address");
            registerOptionalAttribute("size", "4");


            //SC_THREAD(monitorSimulation);
            mState = RUN;
            mBytesPerLine = 8;
            if (!InstanceExists) {
                InstanceExists = true;
                mValid = true;
            } else
                mValid = false;
            mInBenchmark = false;
            //dont_initialize();
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
        }

        uint64_t getBaseAddress() override {
            return getAttrAsUInt64("base_address");
        }

        uint64_t getSize() override {
            return getAttrAsUInt64("size");
        }

        void monitorSimulation() {
            if (!mValid)
                return;
            wait(100000, SC_NS);
            system("clear");
            printf("Press Enter to start VPSim monitor.\n");
            while (true) {
                if (ready()) {
                    process();
                }
                wait(100000, SC_NS);
            }
        }

        bool ready() {
            return ChannelManager::fdCheckReady(0);
        }

        string getLine() {
            char line[1024];
            fgets(line, 1024, stdin);
            return string(line);
        }

        string prompt() {
            stringstream ss;
            ss << "\n" << "@" << sc_time_stamp();
            printf("%s sesam # ", ss.str().c_str());
            return getLine();
        }

        vector<string> getArgv(istringstream &ss) {
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
                DynamicSystemCCosimulator *cosim = dynamic_cast<DynamicSystemCCosimulator *>(ip);
                MainMemPtr = cosim->mModulePtr;
                cosim->mModulePtr->setMonitorPtr(this);
            };
        }

        // Attention: 'counter' manages only one domain, mCurrentDomain and mBenchDomain are then equal
        void sesamCommand(vector<string> &args, size_t counter) override {
            if (counter) {
                // counter != 0 when sesamComand is called by MainMem
                string cmd = args.at(0);
                if (cmd == "StartCapture") {
                    VpsimIp::MapIf(
                        [this](VpsimIp *ip) {
                            return (ip->getAttrAsUInt64("domain") == this->mCurrentDomain && ip->getDelayStatCapture());
                        }, // Delayed IPs
                        [this](VpsimIp *ip) {
                            ip->pushStats();
                        }
                    );
                } else if (cmd == "EndCapture") {
                    string outputBuffer;
                    VpsimIp::MapIf(
                        [this](VpsimIp *ip) {
                            return (ip->getAttrAsUInt64("domain") == this->mBenchDomain && ip->getDelayStatCapture());
                        }, // Delayed IPs
                        [this,&outputBuffer](VpsimIp *ip) {
                            ip->pushStats();
                            auto &stats = ip->getSegStats().back();
                            if (stats.size()) {
                                outputBuffer += "-----------------------------------\n";
                                outputBuffer += "\nStatistics from ";
                                outputBuffer += ip->getName() + "\n";
                                for (auto &stat: stats) {
                                    outputBuffer += "\t";
                                    outputBuffer += stat.first + " = ";
                                    outputBuffer += stat.second + "\n";
                                }
                                ip->clearSegStats();
                            }
                        }
                    );
                    std::FILE *LogFile = fopen(
                        (std::string("sesamBench_") + appName + std::string("_") + std::to_string(counter - 1) + ".log")
                        .c_str(), "a");
                    fprintf(LogFile, "%s", outputBuffer.c_str());
                    fclose(LogFile);
                    delayedCaptureRunning = false;
                }
            } else
                switch (mState) {
                    case RUN: {
                        if (mInBenchmark) {
                            mInBenchmark = false;
                            sc_time diff;
                            if (MainMemPtr) {
                                MainMemPtr->NotifySesamCommand(nbCommandCounter + 1, false);
                                diff = MainMemPtr->getCurrentTime() - mBenchStartTime;
                            } else diff = sc_time_stamp() - mBenchStartTime;
                            // get and display stats
                            //printf("Checking all IPs of domain %ld\n", this->mBenchDomain);
                            fflush(stdout);
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

                                    //printf("Fetching stats for IP %s, has %ld stats\n", ip->getName().c_str(), stats.size());
                                    //fflush(stdout);


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
                            std::FILE *LogFile = fopen(
                                (std::string("sesamBench_") + appName + std::string("_") + std::to_string(
                                     nbCommandCounter++) + ".log").c_str(), "w");
                            fprintf(LogFile, "%s", mCommandOutputBuffer.c_str());
                            fclose(LogFile);
                        }
                    }
                    break;
                    case TAKE_CMD: {
                        string cmd = args.at(0);
                        if (cmd == "quit") {
                            sc_stop();
                            return;
                        } else if (cmd == "show") {
                            if (args.size() - 1 < 1) {
                                printf("Usage: show component1_name component2_name ...\n");
                                return;
                            }
                            for (unsigned i = 1; i < args.size(); i++) {
                                string component = args.at(i);
                                VpsimIp *ip = VpsimIp::Find(component);
                                if (!ip) {
                                    printf("Error: Component %s not known to VPSim.\n", component.c_str());
                                } else {
                                    ip->show();
                                }
                            }
                        } else if (cmd == "showmem") {
                            if (args.size() - 1 != 2) {
                                printf("Usage: showmem start_addr size\n");
                                return;
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
                                                          && ip->getActualAddress() != (unsigned char *) -1
                                                          && ip->getBaseAddress() <= start
                                                          && start < ip->getBaseAddress() + ip->getSize();
                                               }
                                               ,
                                               [this,&start,&found,&end ](VpsimIp *ip) {
                                                   uint64_t actualEnd = std::min(
                                                       end, ip->getBaseAddress() + ip->getSize() - 1);
                                                   uint64_t actualSize = actualEnd - start + 1;
                                                   uint64_t lines = actualSize / mBytesPerLine;
                                                   uint64_t left = actualSize;
                                                   found = true;
                                                   for (uint64_t i = 0; i < lines; i++) {
                                                       printf("\n%016" PRIu64 "\t", start + i * mBytesPerLine);
                                                       uint64_t Left = std::min(mBytesPerLine, left);
                                                       for (uint64_t j = 0; j < Left; j++) {
                                                           printf("%02X\t",
                                                                  ip->getActualAddress()[i * mBytesPerLine + j]);
                                                           left--;
                                                       }
                                                   }
                                                   start = actualEnd + 1;
                                               }
                                );
                                if (!found) {
                                    printf("\nWarning: address space %" PRIu64 " to %" PRIu64 " not covered.\n", start,
                                           end);
                                    break;
                                }
                                if (start > end) {
                                    covered = true;
                                }
                            }
                            printf("\n");
                        } else if (cmd == "list") {
                            if (args.size() - 1 != 0) {
                                printf("Usage: list\n");
                                return;
                            }
                            mCommandOutputBuffer = string();

                            VpsimIp::MapIf(
                                [this](VpsimIp *ip) { return ip->getAttrAsUInt64("domain") == this->mCurrentDomain; },
                                // all IPs
                                [this](VpsimIp *ip) {
                                    mCommandOutputBuffer += ip->getName() + "\n";
                                }
                            );
                        } else if (cmd == "configure") {
                            if (args.size() - 1 < 3) {
                                printf("Usage: configure component_family parameter value\n");
                                return;
                            }

                            string component = args.at(1);
                            VpsimIp *ip = VpsimIp::Find(component);
                            if (!ip) {
                                printf("Error: Component %s not known to VPSim.\n", component.c_str());
                            } else {
                                // change component configuration
                                ip->configure();
                            }
                        } else if (cmd == "debug") {
                            if (args.size() - 1 < 2) {
                                printf("Usage: debug lvl component1 component2 component3 ... \n");
                                return;
                            }
                            istringstream lv(args[1]);
                            uint64_t lvl;
                            lv >> lvl;
                            for (unsigned i = 2; i < args.size(); i++) {
                                LoggerCore::get().enableLogging(true);
                                LoggerCore::get().setDebugLvl(args[i], (DebugLvl) lvl);

                                // Also make sure accesses reach the component.
                            }
                        } else if (cmd == "watch") {
                            if (args.size() - 1 < 2) {
                                printf("Usage: watch base size\n");
                                return;
                            }
                            istringstream st(args.at(1));
                            istringstream sz(args.at(2));
                            uint64_t start, size;
                            st >> hex >> start;
                            sz >> hex >> size;
                            printf("Now monitoring following ranges: \n");


                            // Ask all IPs to watch out for this address space !
                            VpsimIp::MapIf(
                                [this](VpsimIp *ip) { return ip->getAttrAsUInt64("domain") == this->mCurrentDomain; },
                                [&start,&size](VpsimIp *ip) {
                                    AddrSpace as(start, start + size - 1);
                                    // First make sure accesses reach their targets (i.e. No DMI !)
                                    try {
                                        ParamManager::get().setParameter(
                                            ip->getName(), as, BlockingTLMEnabledParameter::bt_enabled);
                                    } catch (exception &ex) {
                                    }

                                    ip->addMonitor(start, size);
                                    ip->showMonitor();
                                }
                            );
                        } else if (cmd == "unwatch") {
                            if (args.size() - 1 < 2) {
                                printf("Usage: unwatch base size\n");
                                return;
                            }
                            istringstream st(args.at(1));
                            istringstream sz(args.at(2));
                            uint64_t start, size;
                            st >> hex >> start;
                            sz >> hex >> size;

                            printf("Now monitoring following ranges: \n");

                            // Ask all IPs to watch out for this address space !
                            VpsimIp::MapIf(
                                [this](VpsimIp *ip) { return ip->getAttrAsUInt64("domain") == this->mCurrentDomain; },
                                // all IPs
                                [&start,&size](VpsimIp *ip) {
                                    AddrSpace as(start, start + size - 1);
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
                        } else if (cmd == "benchmark") {
                            if (args.size() - 2 != 0) {
                                printf("Usage: benchmark app\n");
                                return;
                            }
                            if (delayedCaptureRunning) {
                                // Tests did not show any occurrence of overlapping "sesam benchmark" commands
                                fprintf(stderr, "Wait for the previous benchmark counters to be captured entirely!\n");
                                return;
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
                                    // Enable access simulation to all IPs
                                    /*if (ip->isMemoryMapped()) {
                                          AddrSpace as(ip->getBaseAddress(),ip->getBaseAddress()+ip->getSize()-1);
                                           ParamManager::get().setParameter(ip->getName(), as, BlockingTLMEnabledParameter::bt_enabled);
                                  }*/
                                    // now segment stats
                                    //printf("Pushing stats for IP %s\n", ip->getName().c_str());
                                    ip->pushStats();
                                    //printf("Push ok.\n");
                                }
                            );
                            appName = args.at(1);
                            mInBenchmark = true;
                            mBenchDomain = mCurrentDomain;
                            fprintf(stderr, "Benchmark mode started.\n");
                        }
                    }
                    break;
                    default:
                        throw runtime_error("SesamController in unknown state.");
                }
        }


        void process() {
            switch (mState) {
                case RUN: {
                    getLine();
                    if (mInBenchmark) {
                        mInBenchmark = false;
                        // get and display stats
                        VpsimIp::MapIf(
                            [this](VpsimIp *ip) { return ip->getAttrAsUInt64("domain") == this->mBenchDomain; },
                            [](VpsimIp *ip) {
                                // Go back to fast mode !
                                if (ip->isMemoryMapped() && ip->hasDmi()) {
                                    AddrSpace as(ip->getBaseAddress(), ip->getBaseAddress() + ip->getSize() - 1);
                                    ParamManager::get().setParameter(ip->getName(), as,
                                                                     BlockingTLMEnabledParameter::bt_disabled);
                                }
                                // now segment stats
                                ip->pushStats();
                                auto &stats = ip->getSegStats().back();
                                if (stats.size()) {
                                    printf("-----------------------------------\n");
                                    printf("\nStatistics from %s:\n", ip->getName().c_str());
                                    for (auto &stat: stats) {
                                        printf("\t%s = %s\n", stat.first.c_str(), stat.second.c_str());
                                    }
                                }
                            }
                        );
                        sc_time diff = sc_time_stamp() - mBenchStartTime;
                        stringstream ss;
                        ss << diff;
                        printf("Simulated time: %s\n", ss.str().c_str());
                    }
                    mState = TAKE_CMD;
                    process();
                }
                break;
                case TAKE_CMD: {
                    do {
                        istringstream userInput(prompt());
                        vector<string> argv = getArgv(userInput);
                        if (argv.size() == 0)
                            continue;
                        string cmd = argv.at(0);
                        if (cmd == "quit") {
                            sc_stop();
                            return;
                        } else if (cmd == "go") {
                            mState = RUN;
                        } else if (cmd == "show") {
                            if (argv.size() - 1 < 1) {
                                printf("Usage: show component1_name component2_name ...\n");
                                continue;
                            }
                            for (unsigned i = 1; i < argv.size(); i++) {
                                string component = argv.at(i);
                                VpsimIp *ip = VpsimIp::Find(component);
                                if (!ip) {
                                    printf("Error: Component %s not known to VPSim.\n", component.c_str());
                                } else {
                                    ip->show();
                                }
                            }
                        } else if (cmd == "showmem") {
                            if (argv.size() - 1 != 2) {
                                printf("Usage: showmem start_addr size\n");
                                continue;
                            }
                            bool covered = false;
                            istringstream st(argv.at(1));
                            istringstream sz(argv.at(2));
                            uint64_t start, size;
                            st >> hex >> start;
                            sz >> hex >> size;

                            while (!covered) {
                                uint64_t end = start + size - 1;
                                bool found = false;
                                VpsimIp::MapIf(
                                    [this,&start](VpsimIp *ip) {
                                        return ip->getAttrAsUInt64("domain") == this->mCurrentDomain
                                               && ip->isMemoryMapped()
                                               && ip->getActualAddress() != nullptr
                                               && ip->getActualAddress() != (unsigned char *) -1
                                               && ip->getBaseAddress() <= start
                                               && start < ip->getBaseAddress() + ip->getSize();
                                    }
                                    ,
                                    [this,&start,&found, &end](VpsimIp *ip) {
                                        uint64_t actualEnd = std::min(end, ip->getBaseAddress() + ip->getSize() - 1);
                                        uint64_t actualSize = actualEnd - start + 1;
                                        uint64_t lines = actualSize / mBytesPerLine;
                                        uint64_t left = actualSize;
                                        found = true;
                                        for (uint64_t i = 0; i < lines; i++) {
                                            printf("\n%016" PRIu64 "\t", start + i * mBytesPerLine);
                                            uint64_t Left = std::min(mBytesPerLine, left);
                                            for (uint64_t j = 0; j < Left; j++) {
                                                printf("%02X\t", ip->getActualAddress()[i * mBytesPerLine + j]);
                                                left--;
                                            }
                                        }
                                        start = actualEnd + 1;
                                    }
                                );
                                if (!found) {
                                    printf("\nWarning: address space %016" PRIu64 " to %016" PRIu64 " not covered.\n",
                                           start, end);
                                    break;
                                }
                                if (start > end) {
                                    covered = true;
                                }
                            }
                            printf("\n");
                        } else if (cmd == "help") {
                            printf("Available commands are:\n");
                            printf(
                                "show component1 component2 component3 ... : Dump some components' current status\n");
                            printf("showmem base size : Display 'size' bytes starting from address 'base'\n");
                            printf(
                                "debug lvl component1 component2 component3 ... : Set debug level for some components to 'lvl'\n");
                            printf("watch base size : Log all accesses between base and base+size-1\n");
                            printf("unwatch base size : Stop logging all accesses between base and base+size-1\n");
                            printf("benchmark : Enter precise simulation mode to benchmark an application\n");
                            printf("domainof c: Switch address domain to that of component 'c'\n");
                            printf("\n");
                        } else if (cmd == "debug") {
                            if (argv.size() - 1 < 2) {
                                printf("Usage: debug lvl component1 component2 component3 ... \n");
                                continue;
                            }
                            istringstream lv(argv[1]);
                            uint64_t lvl;
                            lv >> lvl;
                            for (unsigned i = 2; i < argv.size(); i++) {
                                LoggerCore::get().enableLogging(true);
                                LoggerCore::get().setDebugLvl(argv[i], (DebugLvl) lvl);

                                // Also make sure accesses reach the component.
                            }
                        } else if (cmd == "watch") {
                            if (argv.size() - 1 < 2) {
                                printf("Usage: watch base size\n");
                                continue;
                            }
                            istringstream st(argv.at(1));
                            istringstream sz(argv.at(2));
                            uint64_t start, size;
                            st >> hex >> start;
                            sz >> hex >> size;
                            printf("Now monitoring following ranges: \n");


                            // Ask all IPs to watch out for this address space !
                            VpsimIp::MapIf(
                                [this](VpsimIp *ip) { return ip->getAttrAsUInt64("domain") == this->mCurrentDomain; },
                                [&start,&size](VpsimIp *ip) {
                                    AddrSpace as(start, start + size - 1);
                                    // First make sure accesses reach their targets (i.e. No DMI !)
                                    try {
                                        ParamManager::get().setParameter(
                                            ip->getName(), as, BlockingTLMEnabledParameter::bt_enabled);
                                    } catch (exception &ex) {
                                    }

                                    ip->addMonitor(start, size);
                                    ip->showMonitor();
                                }
                            );
                        } else if (cmd == "unwatch") {
                            if (argv.size() - 1 < 2) {
                                printf("Usage: unwatch base size\n");
                                continue;
                            }
                            istringstream st(argv.at(1));
                            istringstream sz(argv.at(2));
                            uint64_t start, size;
                            st >> hex >> start;
                            sz >> hex >> size;

                            printf("Now monitoring following ranges: \n");

                            // Ask all IPs to watch out for this address space !
                            VpsimIp::MapIf(
                                [this](VpsimIp *ip) { return ip->getAttrAsUInt64("domain") == this->mCurrentDomain; },
                                // all IPs
                                [&start,&size](VpsimIp *ip) {
                                    AddrSpace as(start, start + size - 1);
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
                        } else if (cmd == "domainof") {
                            if (argv.size() - 1 != 1) {
                                printf("Usage: domainof component_name\n");
                                continue;
                            }
                            string cmp(argv.at(1));
                            VpsimIp *ip = VpsimIp::Find(cmp);
                            if (ip == nullptr) {
                                printf("No component named %s\n", cmp.c_str());
                            } else {
                                mCurrentDomain = ip->getAttrAsUInt64("domain");
                            }
                        } else if (cmd == "benchmark") {
                            if (argv.size() - 1 != 0) {
                                printf("Usage: benchmark\n");
                                continue;
                            }
                            // First, create a new stats segment
                            VpsimIp::MapIf(
                                [this](VpsimIp *ip) { return ip->getAttrAsUInt64("domain") == this->mCurrentDomain; },
                                // all IPs
                                [](VpsimIp *ip) {
                                    // Enable access simulation to all IPs
                                    //fprintf(stderr, "now with %s\n",ip->getName().c_str());
                                    if (ip->isMemoryMapped()) {
                                        //fprintf(stderr, "enabling TLM of %s\n",ip->getName().c_str());

                                        AddrSpace as(ip->getBaseAddress(), ip->getBaseAddress() + ip->getSize() - 1);
                                        ParamManager::get().setParameter(
                                            ip->getName(), as, BlockingTLMEnabledParameter::bt_enabled);
                                        //fprintf(stderr, "enabled TLM of %s\n",ip->getName().c_str());
                                    }
                                    // now segment stats
                                    //fprintf(stderr, "pushing stats of %s\n",ip->getName().c_str());
                                    ip->pushStats();
                                    //fprintf(stderr, "pushed stats of %s\n",ip->getName().c_str());
                                }
                            );

                            mInBenchmark = true;
                            mBenchDomain = mCurrentDomain;
                            mBenchStartTime = sc_time_stamp();
                            fprintf(stderr, "Benchmark mode started.\n");
                        } else if (cmd == "mips") {
                            if (argv.size() - 1 != 0) {
                                printf("Usage: mips\n");
                                continue;
                            }
                        } else if (cmd == "checkpoint") {
                            if (argv.size() - 1 != 1) {
                                printf("Usage: checkpoint id\n");
                                continue;
                            }

                            mCheckpoints[argv[1]] = getpid();

                            if (fork()) {
                                // parent, wait for signal
                                sigset_t s;
                                sigemptyset(&s);
                                sigaddset(&s, SIGUSR1);
                                int received_sig;
                                sigwait(&s, &received_sig);

                                // here after rollback
                                for (auto it = mCheckpoints.begin(); it != mCheckpoints.end(); it++) {
                                    if (it->second == getpid()) {
                                        mCheckpoints.erase(it);
                                        break;
                                    }
                                }
                            }
                        } else if (cmd == "rollback") {
                            if (argv.size() - 1 != 1) {
                                printf("Usage: rollback id\n");
                                continue;
                            }
                            if (mCheckpoints.find(argv[1]) != mCheckpoints.end()) {
                                int pid = mCheckpoints[argv[1]];
                                kill(pid, SIGUSR1);
                                sc_stop();
                                return;
                            } else {
                                printf("Unknown checkpoint: %s\n", argv[1].c_str());
                            }
                        }
                    } while (mState == TAKE_CMD && sc_core::sc_is_running());
                }
                break;
                default:
                    throw runtime_error("SesamController in unknown state.");
            }
        }
    };
}

#endif  // VPSIM_DYNAMIC_DYNAMICSESAMCONTROLLER_HPP
