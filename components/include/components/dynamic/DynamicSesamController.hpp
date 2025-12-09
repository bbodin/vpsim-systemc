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

    typedef enum StatsFlags {
        STATS_NONE        = 0,
        STATS_DELAYED     = 1 << 0,  // 0x01
        STATS_NONDELAYED  = 1 << 1,  // 0x02
        STATS_ALL         = STATS_DELAYED | STATS_NONDELAYED
    } StatsFlags;


    typedef tlm::tlm_target_socket<> InPortType;
    typedef tlm::tlm_initiator_socket<> OutPortType;

    struct DynamicSesamController final :
            public VpsimIp<InPortType, OutPortType>,
            public SesamController {
    private:
        uint64_t mBytesPerLine;
        uint32_t mCurrentDomain;
        static bool InstanceExists;
        bool mValid;

        SystemCCosimulator *MainMemPtr;

        // benchmarking data
        string benchmarkName;
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

        void finalize() override {
            if (!get_cosim()) {
                LOG_GLOBAL_WARNING << " DynamicSesamControler: There is no Cosim." << std::endl;
            };
        }

        inline vpsim::SystemCCosimulator *get_cosim() {
            if (MainMemPtr) return MainMemPtr;

             VpsimIp *ip = VpsimIp::Find("SystemCCosim0");
            if (ip) {
                const auto cosim = dynamic_cast<DynamicSystemCCosimulator *>(ip);
                MainMemPtr = cosim->mModulePtr;
                cosim->mModulePtr->setMonitorPtr(this);
                return cosim->mModulePtr;
            } else {
                return nullptr;
            };
        }
        


        bool process_quit_cmd() {
            if (captureModeActivated) {
                LOG_GLOBAL_INFO << "VPSim is going to quit while running capture mode... this is not good." << std::endl;
            }
            LOG_GLOBAL_INFO << "Request to quit" << std::endl;

            return trigger_quit();
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
                    LOG_GLOBAL_DEBUG(dbg1) << "Address space for "  << ip->getName() << " is from " << start  << " for about " << size  << std::endl;
                    // First make sure accesses reach their targets (i.e. No DMI !)
                    try {
                        ParamManager::get().setParameter(
                            ip->getName(), as, BlockingTLMEnabledParameter::bt_enabled);
                    } catch (exception &ex) {
                        LOG_GLOBAL_WARNING << "Fail bt_enabled with " << ip->getName() << std::endl;
                    }

                    LOG_GLOBAL_DEBUG(dbg1) << "Call addMonitor for " << ip->getName()  << std::endl; 
                    ip->addMonitor(start, size);
                    LOG_GLOBAL_DEBUG(dbg1) << "Call showMonitor for "  << ip->getName()  << std::endl; 
                    ip->showMonitor();
                    LOG_GLOBAL_DEBUG(dbg1) << "Done with monitoring for "  << ip->getName()  << std::endl;
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





        bool process_start_benchmark_cmd(const vector<string> &args) {

            if (args.size() != 2) {
                printf("Usage: benchmark app\n");
                return false;
            }

            if (captureModeActivated && mInBenchmark) {
                LOG_GLOBAL_ERROR << "Wait for the previous benchmark counters to be captured entirely!\n";
                return false;
            }

            if (captureModeActivated && !mInBenchmark) {
                LOG_GLOBAL_ERROR << "benchmark mode cannot be run in parallel with snapshot mode!\n";
                return false;
            }

            mInBenchmark = true;
            benchmarkName = args.at(1);
            uint64 benchmarkCounter = current_counter++;
            mBenchDomain = mCurrentDomain;
            benchmark_last_counter[benchmarkName] = benchmarkCounter;
            statistics_files[benchmarkCounter] = this->getLogDirectory() + "/" + std::string("sesamBenchmark_") + benchmarkName + std::string("_") + std::to_string(benchmarkCounter) + ".log";
               
            start_capture_mode();
            set_seg_stats(STATS_NONDELAYED); // The delayed stats  will be pushed later, when the capture is initialized
            trigger_delay_capture(benchmarkCounter);

            LOG_GLOBAL_INFO << "Benchmark mode started" << std::endl;
            return true;
        }


        bool process_end_benchmark_cmd(const vector<string> &args) {

            LOG_GLOBAL_INFO << "Finishing the benchmark mode." << std::endl;
            benchmarkName = args.at(1);
            uint64 benchmarkCounter = benchmark_last_counter[benchmarkName];
            std::string baseName = statistics_files[benchmarkCounter];

            set_seg_stats(STATS_NONDELAYED);
            append_seg_stats(statistics_files[benchmarkCounter], STATS_NONDELAYED);
            trigger_delay_capture(benchmarkCounter);
            return stop_capture_mode();


        }

        bool start_capture_mode() {

            if (!get_cosim()) {
                LOG_GLOBAL_WARNING << "Capture mode is not available. " << std::endl;
                 return false;
            }
            if (!captureModeActivated) {
                captureModeActivated = true;
                LOG_GLOBAL_INFO << "Notify the capture to start. " << std::endl;
                get_cosim()->NotifySesamCommand(0, true);
                 return true;
            } else  {
                LOG_GLOBAL_WARNING << "Capture mode already started. " << std::endl;
                 return false;
            }
        }


        bool stop_capture_mode() {
        
            if (get_cosim() && captureModeActivated) {
                captureModeActivated = false;
                LOG_GLOBAL_INFO << "Notify the capture to stop." << std::endl;
                get_cosim()->NotifySesamCommand(0, false); 
                 return true;
            } else if (captureModeActivated) {
                captureModeActivated = false;
                 return false;
            } else {
                LOG_GLOBAL_WARNING << "Capture mode already stopped. " << std::endl;
                 return false;
            }
        }
        



        bool process_start_cmd() {
            LOG_GLOBAL_INFO << "process_start_cmd started " << std::endl;
            return start_capture_mode();
           
        }

        bool process_stop_cmd() {
            LOG_GLOBAL_INFO << "process_stop_cmd started " << std::endl;
            return stop_capture_mode();
        }
        
        
        void set_seg_stats(StatsFlags flags = STATS_ALL) {

            VpsimIp::MapIf(
                [this,flags](VpsimIp *ip) {
                    return (ip->getAttrAsUInt64("domain") == this->mBenchDomain 
                    && ((ip->getDelayStatCapture() && (flags & STATS_DELAYED))
                    || (!ip->getDelayStatCapture() && (flags & STATS_NONDELAYED)))
                );
                },
                [](VpsimIp *ip) {
                    ip->pushStats();
                }
            );
        }

        bool set_instant_stats(StatsFlags flags = STATS_ALL) {
            
            VpsimIp::MapIf(
                [this,flags](VpsimIp *ip) {
                    return (ip->getAttrAsUInt64("domain") == this->mBenchDomain 
                    && ((ip->getDelayStatCapture() && (flags & STATS_DELAYED))
                    || (!ip->getDelayStatCapture() && (flags & STATS_NONDELAYED)))
                );
                },
                [this](VpsimIp *ip) {
                    ip->setStats();
                }
            );

            return true;
        }

         bool append_seg_stats(std::string baseName, StatsFlags flags = STATS_ALL) {

            
            LOG_GLOBAL_INFO << "inside save_diff_stats " << std::endl;
            LOG_GLOBAL_INFO << "Logdir is " << this->getLogDirectory() << std::endl;
            LOG_GLOBAL_INFO << "End of capture, saved to " << baseName << std::endl;

            vpsim::Logger benchLogger(baseName);

            VpsimIp::MapIf(
                [this,flags](VpsimIp *ip) {
                    return (ip->getAttrAsUInt64("domain") == this->mCurrentDomain
                    && ((ip->getDelayStatCapture() && (flags & STATS_DELAYED))
                    || (!ip->getDelayStatCapture() && (flags & STATS_NONDELAYED)))
                );
                },
                [&benchLogger](VpsimIp *ip) {
                    auto &stats = ip->getSegStats().back();
                    if (!stats.empty()) {
                        for (auto &stat: stats) {
                            benchLogger.logStats() << "[Stats] (" << ip->getName() << ") " << stat.first << " " << stat.second << std::endl;
                        }
                        ip->clearSegStats();
                    }
                }
            );

            return true;
        }


        bool append_instant_stats(std::string baseName, StatsFlags flags = STATS_ALL) {

            auto mSnapshotTime = get_cosim()->getCurrentTime(); // TODO : Need to cehck the time here is correct
            mCommandOutputBuffer = string();
            stringstream ss;
            ss << mSnapshotTime;
            mCommandOutputBuffer += "Snapshot time: ";
            mCommandOutputBuffer += ss.str() + "\n";

            VpsimIp::MapIf(
                [this,flags](VpsimIp *ip) {
                    return (ip->getAttrAsUInt64("domain") == this->mCurrentDomain
                    && ((ip->getDelayStatCapture() && (flags & STATS_DELAYED))
                    || (!ip->getDelayStatCapture() && (flags & STATS_NONDELAYED)))
                );
                },
                [this](VpsimIp *ip) {
                    auto &stats = ip->getStats();

                    if (stats.size()) {
                        for (auto &stat: stats) {
                            mCommandOutputBuffer += "(" + ip->getName() + ")\t"+ stat.first + " = "+ stat.second + "\n";
                        }
                    }
                }
            );


            FILE* LogFile = fopen(baseName.c_str(), "a");
            if (!LogFile)
            {
                LOG_GLOBAL_ERROR << "Failed to open log file: " << baseName << std::endl;
                return false;
            }

            if (std::fprintf(LogFile, "%s", mCommandOutputBuffer.c_str()) < 0)
            {
                LOG_GLOBAL_ERROR << "Failed to write to log file" << std::endl;
                return false;
            }

            std::fclose(LogFile);

            LOG_GLOBAL_INFO << "Snapshot saved to " << baseName << std::endl;
            return true;
        }

        bool trigger_quit() {
            if(!get_cosim()) {
                LOG_GLOBAL_WARNING << "Quitting without cosim..." << std::endl;
                sc_stop();
                return true;
            } else {
                LOG_GLOBAL_INFO << "Call finish " << std::endl;
                 get_cosim()->Finish();
                 return true;
            }
        }

        bool trigger_delay_capture(uint64 counter) {
            if (get_cosim() && captureModeActivated) {
                get_cosim()->NotifySesamCommand(counter, true);
                 return true;
            } else  {
                LOG_GLOBAL_WARNING << "Capture mode has not beeen started already. " << std::endl;
                 return false;
            }
        }

        bool process_snapshot_cmd(const vector<string> &args) {
            LOG_GLOBAL_INFO << "process_snapshot_cmd started " << std::endl;

            if (args.size() != 2) {
                printf("Usage: snapshot app\n");
                LOG_GLOBAL_ERROR << "argument count given to snapshot is " << args.size() << std::endl;
                for (auto arg : args) {
                    LOG_GLOBAL_ERROR << " - " << arg << std::endl;
                }
                return false;
            }

            if (!get_cosim()) {
                LOG_GLOBAL_WARNING << "Internal error get_cosim() is null " << std::endl;
                return false;
            } 

            if (mInBenchmark) {
                LOG_GLOBAL_WARNING << "Snapshot is not compatible  with benchmark mode " << std::endl;
                return false;
            } 

            if (!captureModeActivated) {
                LOG_GLOBAL_WARNING << "Capture mode is not started " << std::endl;
                start_capture_mode();
            } 

            benchmarkName = args.at(1);
            uint64 benchmarkCounter = current_counter++;
            statistics_files[benchmarkCounter] = this->getLogDirectory() + "/" + std::string("sesamSnapshot_") + benchmarkName + std::string("_") + std::to_string(benchmarkCounter) + ".log";
               
            set_instant_stats(STATS_NONDELAYED);
            append_instant_stats(statistics_files[benchmarkCounter], STATS_NONDELAYED);
            trigger_delay_capture(benchmarkCounter);
            return true;
            
        }

       
        bool process_capture_stopped_cmd(size_t counter) {

            if (counter == 0) { // nothing was expected apart from stopping the RoI
                LOG_GLOBAL_INFO << "Capture mode finished, nothing else to do. " << std::endl;
                return true;
            }

            LOG_GLOBAL_INFO << "Benchmark mode finished, saving the segment. " << std::endl;
            
            // We are in benchmark mode, we notified the end of capture, we received the go for delay ips.

            // Use the same logger formatting as the global log to ensure consistency
            const std::string baseName =  statistics_files[counter];
            set_seg_stats(STATS_DELAYED);
            append_seg_stats(baseName, STATS_DELAYED);

            return true;

        }

        bool process_delayed_ready_cmd(size_t counter) {

            // The first truth, if this is triggered, the capture mode is running or still running 
            captureModeActivated = true;

            // the second truth, if this is triggered, somewhere the delay stats ar needed (either benchmark or snapshot).

            if (mInBenchmark) {
                set_seg_stats(STATS_DELAYED);
                LOG_GLOBAL_INFO << "Initial delayed capture for benchmarking are done." << std::endl;
            } else {
                set_instant_stats(STATS_DELAYED);
                LOG_GLOBAL_INFO << "Delayed capture for snapshot are done." << std::endl;
                const std::string baseName =  statistics_files[counter];
                if (append_instant_stats(baseName, STATS_DELAYED)) {
                    LOG_GLOBAL_INFO << "Snapshot file " << baseName << " is saved." << std::endl;
                } else {
                    LOG_GLOBAL_ERROR << "Error while saving snapshot file '" << baseName << "'." << std::endl;
                    return false;
                }
            }
            return true;
        }

        // Attention: 'counter' manages only one domain, mCurrentDomain and mBenchDomain are then equal
        void sesamCommand(vector<string> &args, size_t counter) override {
            
            /*
             * Some discussion about this command, 
             *  This command can be triggered by either the user from the guest (to actually command the SesamController), or the CoSim (to feedback on capture mode)
             *  So when CaptureRunning is received, it means the capture start or is still running, and it is a good time to push the stats of delayed IP (because it is a feedback from a notify).
             *
             */

            std::string cmd = args.at(0);

            LOG_GLOBAL_INFO << "DynamicSesamController sesamCommand "  << " cmd=" << cmd << " counter = " << counter << " captureModeActivated = " << captureModeActivated << std::endl;
            bool res = false;

            // feedback from Cosim
            //*************************************** 
            if (cmd == "CaptureStopped" && counter > 0) {
                res = this->process_capture_stopped_cmd(counter); // update capturemode var, if benchmark mode then save delayed stats to counter file
            } else if (cmd == "DelayedReady" && counter > 0) {
                res = this->process_delayed_ready_cmd(counter); // update capturemode var, push/set delayed and save delayed  to counter file if snapshop 
            } else if (cmd == "quit") {
                res = process_quit_cmd(); // sc_stop
            } else if (cmd == "show") {
                res = process_show_cmd(args); // call ip->show();
            } else if (cmd == "showmem") {
                res =  (process_showmem_cmd(args)) ; // based on ip->getBaseAddress, fprintf values
            } else if (cmd == "list") {
                res =  (process_list_cmd(args)) ; // MapIf (get name)
            } else if (cmd == "configure") {
                res =  (process_config_cmd(args)) ; // configure for ip not fully implemented
            } else if (cmd == "debug") {
                res =  (process_debug_cmd(args)) ; // enable logging and setDebugLvl for each comp 
            } else if (cmd == "watch") {
                res =  (process_watch_cmd(args)) ;  // MapIf (add monitor  name)
            } else if (cmd == "unwatch") {
                res =  (process_unwatch_cmd(args)) ;  // MapIf (remove monitor name)
            } else if (cmd == "benchmark") {
                res =  (process_start_benchmark_cmd(args)) ; //  push_non_delay_stats(); start_capture_mode();  trigger_delay(); 
            } else if (cmd == "endBenchmark") {
                res =  (process_end_benchmark_cmd(args)) ; //  push_non_delay_stats(); save_non_delay_diff(to counter file); stop_capture_mode();
            } else if (cmd == "start") {
                res =  (process_start_cmd()) ; // start_capture_mode();
            } else if (cmd == "stop") {
                res =  (process_stop_cmd()) ; //  stop_capture_mode();
            } else if (cmd == "snapshot") {
                res =  (process_snapshot_cmd(args)) ; //  set_non_delay_stats(); save_non_delay(to counter file);   and trigger_delay(to same counter);
            } else {
                LOG_GLOBAL_WARNING << "Unknow command " << cmd << std::endl;
                res = false;
            }
            if (!res) {
                LOG_GLOBAL_WARNING << "Error while runnning the command " << cmd << std::endl;
            }
        }
    };
}

#endif  // VPSIM_DYNAMIC_DYNAMICSESAMCONTROLLER_HPP
