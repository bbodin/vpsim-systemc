#ifndef VPSIM_DYNAMIC_REMOTE_INITIATOR_HPP
#define VPSIM_DYNAMIC_REMOTE_INITIATOR_HPP

#include <sstream>
#include <atomic>
#include "VpsimIp.hpp"
#include "components/SmartUart.hpp"
#include "RemoteInitiator.hpp"

#define tostr(x) dynamic_cast<std::stringstream&&>(std::stringstream{}<<(x)).str()

namespace vpsim {
    typedef tlm::tlm_target_socket<> InPortType;
    typedef tlm::tlm_initiator_socket<> OutPortType;


    struct DynamicRemoteInitiator
            : public VpsimIp<InPortType, OutPortType> {
    public:
        DynamicRemoteInitiator(std::string name) : VpsimIp(std::move(name)),
                                                   mModulePtr(nullptr) {
            registerRequiredAttribute("remote_ip");
            //registerRequiredAttribute("port");
            //registerRequiredAttribute("irq_ip");
            //registerRequiredAttribute("irq_port");
            registerOptionalAttribute("poll_period", "1000");
        }


        N_IN_PORTS_OVERRIDE(0);
        N_OUT_PORTS_OVERRIDE(1);

        InPortType *getNextInPort() override {
            throw runtime_error("No input ports for CPU.");
        }

        OutPortType *getNextOutPort() override {
            if (!mModulePtr) {
                throw runtime_error("Please call make() before handling ports.");
            }
            return mModulePtr->mInitiatorSocket[mOutPortCounter];
        }

        void make() override {
            if (mModulePtr != nullptr) {
                throw runtime_error("make() already called for DynamicRemoteInitiator");
            }
            checkAttributes();
            mModulePtr = new RemoteInitiator(getName().c_str());

            uint16_t port, irq_port;
            cout << "Port for remote transactions: ";
            cin >> port;

            mModulePtr->setIp(getAttr("remote_ip"));
            mModulePtr->setPort(port);
            mModulePtr->setChannel(getAttr("remote_ip") + ":" + to_string(port));

            cout << "Port for remote interrupts: ";
            cin >> irq_port;

            mModulePtr->setIrqIp(getAttr("remote_ip"));
            mModulePtr->setIrqPort(irq_port);
            mModulePtr->setIrqChannel(getAttr("remote_ip") + ":" + to_string(irq_port));

            mModulePtr->setPollPeriod(getAttrAsUInt64("poll_period"));
        }

        void addDmiAddress(std::string targetIpName, uint64_t baseAddr, uint64_t size, unsigned char *pointer,
                                   bool cached, bool has_dmi) override {
        }

        void finalize() override {
        }

        void pushStats() override {
        }

        void setStatsAndDie() override {
            if (mModulePtr) {
                delete mModulePtr;
            }
        }

        InterruptIf *getIrqIf() override {
            return mModulePtr;
        }

    private:
        RemoteInitiator *mModulePtr;
    };
}

#endif // VPSIM_DYNAMIC_REMOTE_INITIATOR_HPP
