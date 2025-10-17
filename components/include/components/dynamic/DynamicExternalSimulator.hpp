#ifndef VPSIM_DYNAMIC_EXTERNAL_SIMULATOR_HPP
#define VPSIM_DYNAMIC_EXTERNAL_SIMULATOR_HPP

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


    struct DynamicExternalSimulator : public VpsimIp<InPortType, OutPortType> {
        DynamicExternalSimulator(string name) : VpsimIp(std::move(name)), mModulePtr(nullptr) {
            registerRequiredAttribute("base_address");
            registerRequiredAttribute("size");
            registerRequiredAttribute("lib_path");
            registerRequiredAttribute("irq_n");
            registerRequiredAttribute("to_configure");
            registerRequiredAttribute("param");
            registerRequiredAttribute("interrupt_parent");
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
            mModulePtr = new ExternalSimulator(getName().c_str(), getSize(), getAttr("lib_path"));
            mModulePtr->setBaseAddress(getBaseAddress());
            mModulePtr->setInterruptLine(getAttrAsUInt64("irq_n"));
            mModulePtr->set_external_simulator(this->mModulePtr);
            mModulePtr->register_sync_cb(external_simulator_sync_cb);
            mModulePtr->register_irq_cb(external_simulator_interrupt_cb);
            //Gather all configuration parameters for the external simulator
            if (!mModulePtr->configured && getAttrAsUInt64("to_configure")) {
                string params = getAttr("param");
                stringstream ss(params);

                while (ss.good()) {
                    string substr;
                    getline(ss, substr, ',');
                    mModulePtr->addParam(substr);
                }

                mModulePtr->config();
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

        void finalize() override {
            VpsimIp *intp = VpsimIp::Find(getAttr("interrupt_parent"));
            if (intp == nullptr)
                throw runtime_error(getAttr("interrupt_parent") + " is not a valid interrupt parent for " + getName());
            mModulePtr->setInterruptParent(intp->getIrqIf());
        }

        ExternalSimulator *mModulePtr;
    };
}

#endif // VPSIM_DYNAMIC_EXTERNAL_SIMULATOR_HPP
