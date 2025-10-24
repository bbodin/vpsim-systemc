#ifndef VPSIM_DYNAMIC_DYNAMICARM64_HPP
#define VPSIM_DYNAMIC_DYNAMICARM64_HPP
#include <sstream>

#include <atomic>
#include "VpsimIp.hpp"
#include "InitiatorIf.hpp"
#include "components/SmartUart.hpp"
#include "gic.hpp"
#include "compute/arm64.hpp"


namespace vpsim {
    typedef tlm::tlm_target_socket<> InPortType;
    typedef tlm::tlm_initiator_socket<> OutPortType;

    struct DynamicArm64
            : public VpsimIp<InPortType, OutPortType> {
    public:
        DynamicArm64(std::string name) : VpsimIp(std::move(name)),
                                         mModulePtr(nullptr) {
            registerRequiredAttribute("model");
            registerRequiredAttribute("iss");
            registerRequiredAttribute("cpu_id");
            registerRequiredAttribute("quantum");
            registerRequiredAttribute("gdb_enable");
            registerRequiredAttribute("stop_on_first_core_done");
            registerRequiredAttribute("ram_size");
            registerRequiredAttribute("kernel");
            registerRequiredAttribute("reset_pc");
            registerRequiredAttribute("io_only");
            registerRequiredAttribute("delay_before_boot");
            registerRequiredAttribute("log");
            registerRequiredAttribute("log_file");

            registerOptionalAttribute("force_lt", "0");
            registerOptionalAttribute("quantum_enable", "1");

            registerOptionalAttribute("wait_for_interrupt", "0");
            registerOptionalAttribute("gic", "none");

            //registerOptionalAttribute("cmdline", "");
            //registerOptionalAttribute("initrd", "");
        }


        NEEDS_DMI_OVERRIDE;
        PROCESSOR_OVERRIDE;

        N_IN_PORTS_OVERRIDE(0);
        N_OUT_PORTS_OVERRIDE(2);

        InPortType *getNextInPort() override {
            throw runtime_error("No input ports for CPU.");
        }

        OutPortType *getNextOutPort() override {
            if (!mModulePtr) {
                throw runtime_error("Please call make() before handling ports.");
            }
            return mModulePtr->mInitiatorSocket[mOutPortCounter];
        }

        void pushStats() override {
            if (mSegmentedStats.empty()) {
                mSegmentedStats.push_back({
                    {"instructions", "0"},
                    {"data_access", "0"}
                });
            }

            const auto &back = mSegmentedStats.back();
            auto instructions = to_string(mModulePtr->getInstructionCount() - stoull(back.at("instructions")));
            auto dataAccesses = to_string(mModulePtr->getDataAccessCount() - stoull(back.at("data_access")));

            mSegmentedStats.push_back({
                {"instructions", instructions},
                {"data_access", dataAccesses}
            });
        }

        void make() override {
            if (mModulePtr != nullptr) {
                throw runtime_error("make() already called for DynamicArm");
            }
            checkAttributes();
            mModulePtr = new arm64(getName().c_str(),
                                   getAttr("model"),
                                   IssFinder(getAttr("iss")),
                                   getAttrAsUInt64("cpu_id"),
                                   getAttrAsUInt64("quantum") / 1000,
                                   (bool) getAttrAsUInt64("gdb_enable"),
                                   (bool) getAttrAsUInt64("stop_on_first_core_done"),
                                   getAttrAsUInt64("reset_pc"),
                                   getAttrAsUInt64("log"), getAttr("log_file").c_str());

            if (getAttrAsUInt64("quantum_enable")) {
                mModulePtr->setQuantumEnable(true);
            } else {
                mModulePtr->setQuantumEnable(false);
            }

            mModulePtr->setIoOnly(getAttrAsUInt64("io_only"));
            //mModulePtr->setDelayBeforeBoot(sc_time(getAttrAsUInt64("delay_before_boot"),SC_PS));
            mModulePtr->setDelayBeforeBoot(sc_time(getAttrAsUInt64("delay_before_boot"), SC_NS));
            //mModulePtr->setLog(getAttrAsUInt64("log"), getAttr("log_file").c_str());

            if (getAttrAsUInt64("force_lt")) {
                mModulePtr->setForceLt(true);
            } else {
                mModulePtr->setForceLt(false);
            }

            mModulePtr->setWaitForInterrupt(getAttrAsUInt64("wait_for_interrupt"));


            addOutPort("to_icache");
            addOutPort("to_dcache");

            auto getDoTLM = [this](uint64_t base, uint64_t end, bool isFetch) {
                //icache is on port 0, dcache is on port 1
                AddrSpace addr(base, end);

                const auto &paramF = this->mVpsimModule->getBlockingTLMEnabled(0, addr);
                const auto &paramRW = this->mVpsimModule->getBlockingTLMEnabled(1, addr);

                this->mIssTLMParam.emplace_back(make_pair(addr, uint64_t(paramF) | (uint64_t(paramRW) << 1)));
                return &this->mIssTLMParam.rbegin()->second;
            };
            mModulePtr->registerIssGetDoTLM(move(getDoTLM));

            auto updateIssDoTLM = [this] {
                //icache is on port 0, dcache is on port 1
                for (auto &param: this->mIssTLMParam) {
                    const auto &addr = param.first;
                    param.second = static_cast<uint64_t>(mVpsimModule->getBlockingTLMEnabled(0, addr)) |
                                   (static_cast<uint64_t>(mVpsimModule->getBlockingTLMEnabled(1, addr)) << 1);
                }
            };

            ParamManager::get().registerUpdateHook(getName(), move(updateIssDoTLM));
        }

        void addDmiAddress(std::string targetIpName, uint64_t baseAddr, uint64_t size, unsigned char *pointer,
                                   bool cached, bool has_dmi) override {
            if (mModulePtr == nullptr) {
                throw runtime_error(getName() + " : calling addDmiAddress() before make() !!!");
            }
            mModulePtr->add_map_dmi(targetIpName, baseAddr, size, pointer);
            if (has_dmi) {
                // mModulePtr->setDmiRange(0, baseAddr, size, pointer);
            }
        }

        void addMonitor(uint64_t base, uint64_t size) override {
            mModulePtr->monitorRange(base, size);
        }

        void removeMonitor(uint64_t base, uint64_t size) override {
            mModulePtr->removeMonitor(base, size);
        }

        void showMonitor() override {
            mModulePtr->showMonitor();
        }

        void finalize() override {
            auto nCores = VpsimIp<InPortType, OutPortType>::AllInstances.find("Arm64")->second.size();
            std::cout << "Number of cores: " << nCores << endl;
            if (getAttr("kernel") != "")
                mModulePtr->iss_load_elf(getAttrAsUInt64("ram_size"), (char *) getAttr("kernel").c_str(),
                                         nullptr, nullptr);
            VpsimIp *par = VpsimIp::Find(this->getAttr("gic"));
            if (par == nullptr)
                throw runtime_error("AARCH64: Please specify the gic attribute to point to an actual GIC.");
            this->mModulePtr->setGic(par->getIrqIf());
            if (!getAttrAsUInt64("io_only"))
                dynamic_cast<DynamicGIC *>(par)->connectCpu(mModulePtr, mModulePtr->get_cpu_id());
            else
                VpsimIp::MapTypeIf("Arm64",
                                   [](VpsimIp *ip) { return true; },
                                   [this](VpsimIp *ip) {
                                       dynamic_cast<DynamicArm64 *>(ip)->mModulePtr->addIoEvent(
                                           this->mModulePtr->getIoEvent());
                                   }
                );
        }

        /*virtual VpsimModule* asModule() override {
           return mModulePtr;
     }*/

        void setStatsAndDie() override {
            if (mModulePtr) {
                mStats["instructions"] = std::to_string(mModulePtr->getInstructionCount());
                mStats["data_access"] = std::to_string(mModulePtr->getDataAccessCount());

                delete mModulePtr;
            }
        }

        InterruptIf *getIrqIf() override { return mModulePtr; }

        IssWrapper *getIssHandle() { return mModulePtr; }

    private:
        arm64 *mModulePtr;
        std::deque<std::pair<AddrSpace, uint64_t> > mIssTLMParam;
    };
}

#endif  // VPSIM_DYNAMIC_DYNAMICARM64_HPP
