#ifndef VPSIM_DYNAMIC_REMOTE_TARGET_HPP
#define VPSIM_DYNAMIC_REMOTE_TARGET_HPP

#include <sstream>
#include <atomic>
#include "VpsimIp.hpp"
#include "RemoteTarget.hpp"

#define tostr(x) dynamic_cast<std::stringstream&&>(std::stringstream{}<<(x)).str()

namespace vpsim {
    typedef tlm::tlm_target_socket<> InPortType;
    typedef tlm::tlm_initiator_socket<> OutPortType;


    struct DynamicRemoteTarget : public VpsimIp<InPortType, OutPortType> {
        DynamicRemoteTarget(string name) : VpsimIp(std::move(name)), mModulePtr(nullptr) {
            registerRequiredAttribute("base_address");
            registerRequiredAttribute("size");
            registerRequiredAttribute("channel");
            registerRequiredAttribute("irq_channel");
            registerRequiredAttribute("irq_n");
            registerRequiredAttribute("interrupt_parent");
            registerOptionalAttribute("poll_period", "1000");
        }

        N_IN_PORTS_OVERRIDE(1);
        N_OUT_PORTS_OVERRIDE(0);
        MEMORY_MAPPED_OVERRIDE;

        InPortType *getNextInPort() override {
            return &mModulePtr->mTargetSocket;
        }

        OutPortType *getNextOutPort() override {
            throw runtime_error(VpsimIp::getName() + " : Memory has no out sockets.");
        }

        void make() override {
            checkAttributes();
            mModulePtr = new RemoteTarget(getName().c_str(), getSize());
            mModulePtr->setBaseAddress(getBaseAddress());
            mModulePtr->setChannel(getAttr("channel"));
            mModulePtr->setIrqChannel(getAttr("irq_channel"));
            mModulePtr->setInterruptLine(getAttrAsUInt64("irq_n"));
            mModulePtr->setPollPeriod(getAttrAsUInt64("poll_period"));
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

        void finalize() override {
            VpsimIp *intp = VpsimIp::Find(getAttr("interrupt_parent"));
            if (intp == nullptr)
                throw runtime_error(getAttr("interrupt_parent") + " is not a valid interrupt parent for " + getName());
            mModulePtr->setInterruptParent(intp->getIrqIf());
        }

        RemoteTarget *mModulePtr;
    };


}

#endif // VPSIM_DYNAMIC_REMOTE_TARGET_HPP
