#ifndef VPSIM_DYNAMIC_DYNAMICUART_HPP
#define VPSIM_DYNAMIC_DYNAMICUART_HPP
#include <sstream>

#include <atomic>
#include <vpsimModule/VpsimIp.hpp>
#include "TargetIf.hpp"
#include "peripherals/uart.hpp"


namespace vpsim {
    typedef tlm::tlm_target_socket<> InPortType;
    typedef tlm::tlm_initiator_socket<> OutPortType;


    struct DynamicUart
            : public VpsimIp<InPortType, OutPortType> {
    public:
        DynamicUart(std::string name) : VpsimIp(std::move(name)),
                                        mModulePtr(nullptr) {
            registerRequiredAttribute("size");
            registerRequiredAttribute("base_address");
            registerRequiredAttribute("cycle_duration");
            registerRequiredAttribute("write_cycles");
            // registerRequiredAttribute("read_cycles");

            registerOptionalAttribute("latency_enable", "1");
        }

        void pushStats() override {
             LOG_GLOBAL_DEBUG(dbg2) << "Your component " << this->getName() << " is asked to push stats.\n";
            
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

        N_IN_PORTS_OVERRIDE(1);
        N_OUT_PORTS_OVERRIDE(0);

        InPortType *getNextInPort() override {
            return &(mModulePtr->mTargetSocket);
        }

        OutPortType *getNextOutPort() override {
            throw runtime_error(getName() + " : uart has no out sockets.");
        }

        void make() override {
            if (mModulePtr != nullptr) {
                throw runtime_error(getName() + " make() already called.");
            }
            checkAttributes();
            mModulePtr = new uart(getName().c_str());
            mModulePtr->setBaseAddress(getAttrAsUInt64("base_address"));
            //mModulePtr->setCycleDuration(sc_time(getAttrAsUInt64("cycle_duration"), SC_PS));
            mModulePtr->setCycleDuration(sc_time(getAttrAsUInt64("cycle_duration"), SC_NS));
            mModulePtr->setCyclesPerWrite(getAttrAsUInt64("write_cycles"));

            if (getAttrAsUInt64("latency_enable")) {
                mModulePtr->setEnableLatency(true);
            } else {
                mModulePtr->setEnableLatency(false);
            }
        }

        uint64_t getBaseAddress() override {
            return getAttrAsUInt64("base_address");
        }

        uint64_t getSize() override {
            return getAttrAsUInt64("size");
        }

        unsigned char *getActualAddress() override {
            return (unsigned char *) -1;
        }

    private:
        uart *mModulePtr;
    };
}

#endif  // VPSIM_DYNAMIC_DYNAMICUART_HPP
