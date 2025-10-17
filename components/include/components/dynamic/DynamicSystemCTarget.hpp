#ifndef VPSIM_DYNAMIC_SYSTEMC_TARGET_HPP
#define VPSIM_DYNAMIC_SYSTEMC_TARGET_HPP

#include <sstream>

#include <atomic>
#include "VpsimIp.hpp"
#include "TargetIf.hpp"
#include "peripherals/uart.hpp"
#include "compute/arm64.hpp"
#include "SystemCTarget.hpp"


namespace vpsim {
    typedef tlm::tlm_target_socket<> InPortType;
    typedef tlm::tlm_initiator_socket<> OutPortType;

    struct DynamicSystemCTarget : public VpsimIp<InPortType, OutPortType> {
        DynamicSystemCTarget(string name) : VpsimIp(std::move(name)), mModulePtr(nullptr) {
            registerRequiredAttribute("base_address");
            registerRequiredAttribute("size");
            registerRequiredAttribute("interrupt_parent");
        }

        N_IN_PORTS_OVERRIDE(1);
        N_OUT_PORTS_OVERRIDE(1);
        MEMORY_MAPPED_OVERRIDE;

        InPortType *getNextInPort() override {
            return &mModulePtr->mTargetSocket;
        }

        OutPortType *getNextOutPort() override {
            return &mModulePtr->_out;
        }

        void make() override {
            checkAttributes();
            mModulePtr = new SystemCTarget(getName().c_str(), getSize());
            mModulePtr->setBaseAddress(getBaseAddress());
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
            if (intp == nullptr && getAttr("interrupt_parent") != "none")
                throw runtime_error(getAttr("interrupt_parent") + " is not a valid interrupt parent for " + getName());
            else if (intp)
                mModulePtr->setInterruptParent(intp->getIrqIf());
        }

        SystemCTarget *mModulePtr;
    };

}

#endif // VPSIM_DYNAMIC_SYSTEMC_TARGET_HPP
