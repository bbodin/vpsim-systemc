#ifndef VPSIM_DYNAMIC_DYNAMICITCTRL_HPP
#define VPSIM_DYNAMIC_DYNAMICITCTRL_HPP
#include <sstream>
#include <signal.h>
#include <atomic>
#include "VpsimIp.hpp"
#include "TargetIf.hpp"
#include "InitiatorIf.hpp"
#include "components/SmartUart.hpp"
#include "PL011Uart.hpp"
#include "gic.hpp"
#include "VirtioTlm.hpp"
#include "xuartps.hpp"
#include "AddressTranslator.hpp"
#include "SesamController.hpp"
#include "components/CallbackRegister.hpp"
#include <vpsimModule/ForwardSimpleSocket.hpp>
#include "peripherals/ItCtrl.hpp"
#include "peripherals/uart.hpp"
#include "memory/memory.hpp"
#include "connect/interconnect.hpp"
#include "memory/Cache.hpp"
#include "compute/arm.hpp"
#include "compute/arm64.hpp"
#include "RemoteInitiator.hpp"
#include "RemoteTarget.hpp"
#include "ExternalSimulator.hpp"
#include "SystemCTarget.hpp"
#include "MainMemCosim.hpp"
#include "IOAccessCosim.hpp"
#include "CoherenceInterconnect.hpp"

#define tostr(x) dynamic_cast<std::stringstream&&>(std::stringstream{}<<(x)).str()

namespace vpsim {
    typedef tlm::tlm_target_socket<> InPortType;
    typedef tlm::tlm_initiator_socket<> OutPortType;



    struct DynamicItCtrl
            : public VpsimIp<InPortType, OutPortType> {
    public:
        DynamicItCtrl(std::string name) : VpsimIp(std::move(name)),
                                          mModulePtr(nullptr) {
            registerRequiredAttribute("size");
            registerRequiredAttribute("base_address");

            registerRequiredAttribute("line_size");
            registerRequiredAttribute("size_per_cpu");
            //registerRequiredAttribute("ipi_irq_idx");
            //registerRequiredAttribute("ipi_line_offset");
            //registerRequiredAttribute("irc_size_per_cpu");
        }

        void pushStats() override {
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

        void setStatsAndDie() override {
            if (mModulePtr) {
                mStats["reads"] = tostr(mModulePtr->getReadCount());
                mStats["writes"] = tostr(mModulePtr->getWriteCount());
                delete mModulePtr;
            }
        }

        MEMORY_MAPPED_OVERRIDE;
        // HAS_DMI_OVERRIDE;

        N_IN_PORTS_OVERRIDE(1);
        N_OUT_PORTS_OVERRIDE(0);

        InPortType *getNextInPort() override {
            return &(mModulePtr->mTargetSocket);
        }

        OutPortType *getNextOutPort() override {
            throw runtime_error(getName() + " : itctrl has no out sockets.");
        }

        void make() override {
            if (mModulePtr != nullptr) {
                throw runtime_error(getName() + " : make() already called.");
            }
            checkAttributes();
            mModulePtr = new ItCtrl(getName().c_str(),
                                    getAttrAsUInt64("size") / getAttrAsUInt64("line_size"),
                                    getAttrAsUInt64("line_size"));
            mModulePtr->setBaseAddress(getAttrAsUInt64("base_address"));
        }

        uint64_t getBaseAddress() override {
            return getAttrAsUInt64("base_address");
        }

        uint64_t getSize() override {
            return getAttrAsUInt64("size");
        }

        unsigned char *getActualAddress() override {
            return (unsigned char *) -1;
            // reinterpret_cast<unsigned char*>(mModulePtr->getLocalMem()); // We don't have to provide address as we don't support DMI
        }

        void finalize() override {
            if (AllInstances.find("Arm") != AllInstances.end()) {
                std::cout << "FIXME: Auto-mapping arm interrupt lines" << endl;
                VpsimIp<InPortType, OutPortType>::MapTypeIf("Arm",
                                                            [](VpsimIp<InPortType, OutPortType> *ip) {
                                                                return ip->isProcessor();
                                                            },
                                                            [this](VpsimIp<InPortType, OutPortType> *ip) {
                                                                unsigned nLines =
                                                                        this->getAttrAsUInt64("size") / this->
                                                                        getAttrAsUInt64("line_size");
                                                                for (unsigned i = 0; i < nLines; i++) {
                                                                    this->mModulePtr->Map(
                                                                        ip->getAttrAsUInt64("cpu_id") * this->
                                                                        getAttrAsUInt64("size_per_cpu") /
                                                                        this->getAttrAsUInt64("line_size") + i,
                                                                        ip->getIrqIf(),
                                                                        i
                                                                    );
                                                                }
                                                            }
                );
            }
        }

    private:
        ItCtrl *mModulePtr;
    };
}


#endif  // VPSIM_DYNAMIC_DYNAMICITCTRL_HPP
