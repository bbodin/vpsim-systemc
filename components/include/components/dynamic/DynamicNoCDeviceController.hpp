#ifndef VPSIM_DYNAMIC_DYNAMICNOCDEVICECONTROLLER_HPP
#define VPSIM_DYNAMIC_DYNAMICNOCDEVICECONTROLLER_HPP

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

struct DynamicNoCDeviceController
        : public VpsimIp<InPortType, OutPortType> {
public:
    DynamicNoCDeviceController(std::string name) : VpsimIp(std::move(name)) {
        registerRequiredAttribute("id_dev");
        registerRequiredAttribute("x_id");
        registerRequiredAttribute("y_id");
        registerRequiredAttribute("noc");
    }

    N_IN_PORTS_OVERRIDE(0);
    N_OUT_PORTS_OVERRIDE(0);

    InPortType *getNextInPort() override {
        throw runtime_error(getName() + " : IOAccess Device has no in sockets.");
    }

    OutPortType *getNextOutPort() override {
        throw runtime_error(getName() + " : IOAccess Device has no out sockets.");
    }

    void make() override {
        checkAttributes();
    }

    void finalize() override {
        VpsimIp *ip = VpsimIp::Find(getAttr("noc"));
        DynamicCoherenceInterconnect *noc = dynamic_cast<DynamicCoherenceInterconnect *>(ip);
        noc->mModulePtr->register_device_ctrl(getAttrAsUInt64("id_dev"),
                                              getAttrAsUInt64("x_id"),
                                              getAttrAsUInt64("y_id"));
    }
};
}

#endif  // VPSIM_DYNAMIC_DYNAMICNOCDEVICECONTROLLER_HPP
