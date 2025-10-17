#ifndef VPSIM_DYNAMIC_DYNAMICNOCSOURCE_HPP
#define VPSIM_DYNAMIC_DYNAMICNOCSOURCE_HPP
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

struct DynamicNoCSource
        : public VpsimIp<InPortType, OutPortType> {
public:
    DynamicNoCSource(std::string name) : VpsimIp(std::move(name)) {
        registerRequiredAttribute("src_id");
        registerRequiredAttribute("noc_id");
        registerRequiredAttribute("noc");
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
        DynamicInterconnect *noc = dynamic_cast<DynamicInterconnect *>(ip);
        noc->mModulePtr->register_source(
            getAttrAsUInt64("src_id"),
            getAttrAsUInt64("noc_id"));
    }
};
}

#endif  // VPSIM_DYNAMIC_DYNAMICNOCSOURCE_HPP
