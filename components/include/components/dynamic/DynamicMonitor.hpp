#ifndef VPSIM_DYNAMIC_DYNAMICMONITOR_HPP
#define VPSIM_DYNAMIC_DYNAMICMONITOR_HPP
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

struct DynamicMonitor : public VpsimIp<InPortType, OutPortType> {
    N_IN_PORTS_OVERRIDE(0);
    N_OUT_PORTS_OVERRIDE(0);

    DynamicMonitor(string name) : VpsimIp(std::move(name)) {
        registerRequiredAttribute("start_address");
        registerRequiredAttribute("size");
        registerRequiredAttribute("cpu");
    }

    InPortType *getNextInPort() override {
        throw runtime_error(getName() + " : Monitor has no sockets.");
    }

    OutPortType *getNextOutPort() override {
        throw runtime_error(getName() + " : Monitor has no sockets.");
    }

    void make() override {
        checkAttributes();
    }

    void finalize() override {
        VpsimIp *cpu = VpsimIp::Find(getAttr("cpu"));
        if (!cpu) {
            throw runtime_error(getName() + ": Could not find target cpu to monitor " + getAttr("cpu"));
        }
        IssWrapper *issProvider = dynamic_cast<DynamicArm64 *>(cpu)->getIssHandle();
        issProvider->monitorRange(getAttrAsUInt64("start_address"), getAttrAsUInt64("size"));
    }

    void setStatsAndDie() override {
    }
};
}

#endif  // VPSIM_DYNAMIC_DYNAMICMONITOR_HPP
