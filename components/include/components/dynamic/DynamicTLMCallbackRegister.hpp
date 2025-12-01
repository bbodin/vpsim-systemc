#ifndef VPSIM_DYNAMIC_DYNAMICTLMCALLBACKREGISTER_HPP
#define VPSIM_DYNAMIC_DYNAMICTLMCALLBACKREGISTER_HPP

#include <sstream>

#include <atomic>
#include "VpsimIp.hpp"
#include "components/CallbackRegister.hpp"


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
             LOG_GLOBAL_DEBUG(dbg2) << "Your component " << this->getName() << " is asked to push stats.\n";
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

        void setStats() override {
            mStats["nb_reads"] = std::to_string(this->getNbReads());
            mStats["nb_writes"] = std::to_string(this->getNbWrites());
        }
    

        void registerCallback(uint64_t val, const string &callback) override {
            TLMCallbackRegister<T>::registerCallback(val, callback);
        }
    };
}
#endif  // VPSIM_DYNAMIC_DYNAMICTLMCALLBACKREGISTER_HPP
