#ifndef VPSIM_DYNAMIC_DYNAMICNOCMEMORYCONTROLLER_HPP
#define VPSIM_DYNAMIC_DYNAMICNOCMEMORYCONTROLLER_HPP
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


struct DynamicNoCMemoryController
        : public VpsimIp<InPortType, OutPortType> {
public:
    DynamicNoCMemoryController(std::string name) : VpsimIp(std::move(name)) {
        registerRequiredAttribute("size");
        registerRequiredAttribute("base_address");
        //registerRequiredAttribute("cpu_affinity");
        registerRequiredAttribute("noc");
        registerRequiredAttribute("x_id");
        registerRequiredAttribute("y_id");
    }

    N_IN_PORTS_OVERRIDE(0);
    N_OUT_PORTS_OVERRIDE(0);

    InPortType *getNextInPort() override {
        throw runtime_error(getName() + " : MemoryView has no in sockets.");
    }

    OutPortType *getNextOutPort() override {
        throw runtime_error(getName() + " : Memory has no out sockets.");
    }

    void make() override {
        checkAttributes();
    }

    void finalize() override {
        VpsimIp *ip = VpsimIp::Find(getAttr("noc"));
        //DynamicInterconnect* noc=dynamic_cast<DynamicInterconnect*>(ip);
        DynamicCoherenceInterconnect *noc = dynamic_cast<DynamicCoherenceInterconnect *>(ip);
        noc->mModulePtr->set_first_memory_controller();
        //call first and then register in order to capture the right index in the vector
        noc->mModulePtr->register_mem_ctrl(getAttrAsUInt64("base_address"),
                                           getAttrAsUInt64("size"),
                                           getAttrAsUInt64("x_id"),
                                           getAttrAsUInt64("y_id"));
        //getAttrAsUInt64("cpu_affinity"));
        //cout << "memory base address= " << getAttrAsUInt64("base_address") << " memory mesh coordinates: x= " << getAttrAsUInt64("x_id") << " y= " << getAttrAsUInt64("y_id") << endl;
        if (noc->mModulePtr->get_ram_base_addr() > getAttrAsUInt64("base_address")) noc->mModulePtr->
                set_ram_base_addr(getAttrAsUInt64("base_address"));
        if (noc->mModulePtr->get_ram_last_addr() < getAttrAsUInt64("base_address") + getAttrAsUInt64("size")) noc->
                mModulePtr->set_ram_last_addr(getAttrAsUInt64("base_address") + getAttrAsUInt64("size"));
    }
};
}

#endif  // VPSIM_DYNAMIC_DYNAMICNOCMEMORYCONTROLLER_HPP
