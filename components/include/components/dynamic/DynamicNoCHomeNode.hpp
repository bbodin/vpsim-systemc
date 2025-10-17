#ifndef VPSIM_DYNAMIC_DYNAMICNOCHOMENODE_HPP
#define VPSIM_DYNAMIC_DYNAMICNOCHOMENODE_HPP
#include <sstream>

#include <atomic>
#include "VpsimIp.hpp"
#include "connect/interconnect.hpp"



namespace vpsim {
    typedef tlm::tlm_target_socket<> InPortType;
    typedef tlm::tlm_initiator_socket<> OutPortType;

struct DynamicNoCHomeNode
        : public VpsimIp<InPortType, OutPortType> {
public:
    DynamicNoCHomeNode(std::string name) : VpsimIp(std::move(name)) {
        registerRequiredAttribute("size");
        registerRequiredAttribute("base_address");
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
        noc->mModulePtr->register_hn_input(
            getAttrAsUInt64("base_address"),
            getAttrAsUInt64("size"),
            getAttrAsUInt64("noc_id"));
    }
};
}

#endif  // VPSIM_DYNAMIC_DYNAMICNOCHOMENODE_HPP
