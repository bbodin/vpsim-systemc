#ifndef VPSIM_DYNAMIC_DYNAMICMEMORY_HPP
#define VPSIM_DYNAMIC_DYNAMICMEMORY_HPP
#include <sstream>

#include <atomic>
#include "VpsimIp.hpp"
#include "TargetIf.hpp"
#include "memory/memory.hpp"



namespace vpsim {
    typedef tlm::tlm_target_socket<> InPortType;
    typedef tlm::tlm_initiator_socket<> OutPortType;

    struct DynamicMemory
            : public VpsimIp<InPortType, OutPortType> {
    public:
        DynamicMemory(std::string name) : VpsimIp(std::move(name)),
                                          mModulePtr(nullptr) {
            registerRequiredAttribute("size");
            registerRequiredAttribute("base_address");

            registerRequiredAttribute("cycle_duration");
            registerRequiredAttribute("write_cycles");
            registerRequiredAttribute("read_cycles");

            registerRequiredAttribute("channel_width");

            registerRequiredAttribute("load_elf");
            registerRequiredAttribute("elf_file");

            //registerOptionalAttribute("load_blob", "0");
            //registerOptionalAttribute("blob_file", "0");
            //registerOptionalAttribute("blob_offset", "0");

            registerOptionalAttribute("latency_enable", "1");
            registerRequiredAttribute("dmi_enable");

            //registerRequiredAttribute("noc");
        }

        void pushStats() override {
             LOG_GLOBAL_DEBUG(dbg0) << "Your component " << this->getName() << " is asked to push stats.\n";
            if (mSegmentedStats.empty()) {
                mSegmentedStats.push_back({
                    {"reads", "0"},
                    {"writes", "0"}
                });
            }

            const auto &back = mSegmentedStats.back();
            auto reads = to_string(mModulePtr->getReadCount() - stoull(back.at("reads")));
            auto writes = to_string(mModulePtr->getWriteCount() - stoull(back.at("writes")));

            mSegmentedStats.push_back({
                {"reads", reads},
                {"writes", writes}
            });
        }

        void setStats() override {
            if (mModulePtr) {
                mStats["reads"] = std::to_string(mModulePtr->getReadCount());
                mStats["writes"] = std::to_string(mModulePtr->getWriteCount());
            }
        }
        void terminate() override {
            if (mModulePtr) {
                delete mModulePtr;
            }
        }

        MEMORY_MAPPED_OVERRIDE;
        CACHED_OVERRIDE;
        bool hasDmi() override { return getAttrAsUInt64("dmi_enable"); }

        N_IN_PORTS_OVERRIDE(1);
        N_OUT_PORTS_OVERRIDE(0);

        InPortType *getNextInPort() override {
            return &mModulePtr->mTargetSocket;
        }

        OutPortType *getNextOutPort() override {
            throw runtime_error(getName() + " : Memory has no out sockets.");
        }

        void make() override {
            if (mModulePtr != nullptr) {
                throw runtime_error(getName() + " make() already called !!");
            }
            checkAttributes();
            mModulePtr = new memory(getName().c_str(),
                                    getAttrAsUInt64("size"));

            setDelayStatCapture(true);
            mModulePtr->setBaseAddress(getAttrAsUInt64("base_address"));
            //mModulePtr->setCycleDuration(sc_time(getAttrAsUInt64("cycle_duration"), SC_PS));
            mModulePtr->setCycleDuration(sc_time(getAttrAsUInt64("cycle_duration"), SC_NS));
            mModulePtr->setCyclesPerRead(getAttrAsUInt64("read_cycles"));
            mModulePtr->setCyclesPerWrite(getAttrAsUInt64("write_cycles"));
            mModulePtr->setChannelWidth(getAttrAsUInt64("channel_width"));
            mModulePtr->ReadLatency = sc_time(getAttrAsUInt64("read_cycles"), SC_NS);
            mModulePtr->WriteLatency = sc_time(getAttrAsUInt64("write_cycles"), SC_NS);

            if (getAttrAsUInt64("latency_enable")) {
                mModulePtr->setEnableLatency(true);
            } else {
                mModulePtr->setEnableLatency(false);
            }

            if (getAttrAsUInt64("dmi_enable")) {
                mModulePtr->setDmiEnable(true);
            } else {
                mModulePtr->setDmiEnable(false);
            }
        }


        uint64_t getBaseAddress() override {
            return getAttrAsUInt64("base_address");
        }

        uint64_t getSize() override {
            return getAttrAsUInt64("size");
        }

        unsigned char *getActualAddress() override {
            return mModulePtr->getLocalMem();
        }

        void finalize() override {
            if (getAttrAsUInt64("load_elf")) {
                cout << "Loading ELF: " << getAttr("elf_file") << endl;
                mModulePtr->loadElfFile(getAttr("elf_file").c_str());
            }
            //VpsimIp* ip = VpsimIp::Find(getAttr("noc"));
            //DynamicCoherenceInterconnect* noc=dynamic_cast<DynamicCoherenceInterconnect*>(ip);
            //noc->mModulePtr->set_memory_word_length (getAttrAsUInt64("channel_width"));
        }

    private:
        memory *mModulePtr;

        friend struct DynamicBlobLoader;
    };
}

#endif  // VPSIM_DYNAMIC_DYNAMICMEMORY_HPP
