#ifndef VPSIM_DYNAMIC_DYNAMICTLMCALLBACKREGISTER_HPP
#define VPSIM_DYNAMIC_DYNAMICTLMCALLBACKREGISTER_HPP

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




    template<class T>
    class DynamicTLMCallbackRegister :
            public TLMCallbackRegister<T>,
            public VpsimIp<InPortType, OutPortType> {
    public:
        explicit DynamicTLMCallbackRegister(std::string name) : TLMCallbackRegister<T>(name.c_str()),
                                                                VpsimIp(name) {
            registerRequiredAttribute("base_address");
            registerOptionalAttribute("cycle_duration", "1e3");
            registerOptionalAttribute("write_cycles", "1");
            registerOptionalAttribute("read_cycles", "1");
        }

        ~DynamicTLMCallbackRegister() override {
            this->PrintStatistics();
        }

        MEMORY_MAPPED_OVERRIDE;

        unsigned getMaxInPortCount() override {
            return 1;
        }

        unsigned getMaxOutPortCount() override {
            return 0;
        }

        InPortType *getNextInPort() override {
            return &this->mTargetSocket;
        }

        OutPortType *getNextOutPort() override {
            throw runtime_error(VpsimIp::getName() + " : CallbackRegister has no out sockets.");
        }

        void make() override {
            checkAttributes();
            this->setBaseAddress(getAttrAsUInt64("base_address"));
            //this->setCycleDuration(sc_time(getAttrAsUInt64("cycle_duration"), SC_PS));
            this->setCycleDuration(sc_time(getAttrAsUInt64("cycle_duration"), SC_NS));
            this->setCyclesPerWrite(static_cast<int>(getAttrAsUInt64("write_cycles")));
        }

        uint64_t getBaseAddress() override {
            return getAttrAsUInt64("base_address");
        }

        uint64_t getSize() override {
            return sizeof(T);
        }

        unsigned char *getActualAddress() override {
            return reinterpret_cast<unsigned char *>(this->getLocalMem());
        }

        void pushStats() override {
            if (mSegmentedStats.empty()) {
                mSegmentedStats.push_back({
                    {"reads", "0"},
                    {"writes", "0"}
                });
            }

            const auto &back = mSegmentedStats.back();
            auto reads = to_string(this->getReadCount() - stoull(back.at("reads")));
            auto writes = to_string(this->getWriteCount() - stoull(back.at("writes")));

            mSegmentedStats.push_back({
                {"reads", reads},
                {"writes", writes}
            });
        }

        void setStatsAndDie() override {
            mStats["nb_reads"] = tostr(this->getNbReads());
            mStats["nb_writes"] = tostr(this->getNbWrites());
        }

        void registerCallback(uint64_t val, const string &callback) override {
            TLMCallbackRegister<T>::registerCallback(val, callback);
        }
    };
}
#endif  // VPSIM_DYNAMIC_DYNAMICTLMCALLBACKREGISTER_HPP
