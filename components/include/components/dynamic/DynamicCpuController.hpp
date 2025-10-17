#ifndef VPSIM_DYNAMIC_DYNAMICCPUCONTROLLER_HPP
#define VPSIM_DYNAMIC_DYNAMICCPUCONTROLLER_HPP
#include <sstream>

#include <atomic>
#include "VpsimIp.hpp"
#include "CoherenceInterconnect.hpp"



namespace vpsim {
    typedef tlm::tlm_target_socket<> InPortType;
    typedef tlm::tlm_initiator_socket<> OutPortType;

    struct DynamicCpuController
            : public VpsimIp<InPortType, OutPortType> {
    public:
        DynamicCpuController(std::string name) : VpsimIp(std::move(name)) {
            registerRequiredAttribute("id");
            registerRequiredAttribute("noc");
            registerRequiredAttribute("x_id");
            registerRequiredAttribute("y_id");
        }

        N_IN_PORTS_OVERRIDE(0);
        N_OUT_PORTS_OVERRIDE(0);

        InPortType *getNextInPort() override {
            throw runtime_error(getName() + " : Cpu Controller has no in sockets.");
        }

        OutPortType *getNextOutPort() override {
            throw runtime_error(getName() + " : Cpu Controller has no out sockets.");
        }

        void make() override {
            checkAttributes();
        }

        void finalize() override {
            VpsimIp *ip = VpsimIp::Find(getAttr("noc"));
            DynamicCoherenceInterconnect *noc = dynamic_cast<DynamicCoherenceInterconnect *>(ip);
            noc->mModulePtr->register_cpu_ctrl(getAttrAsUInt64("id"),
                                               getAttrAsUInt64("x_id"),
                                               getAttrAsUInt64("y_id"));
            //cout << "cpuId= " << getAttrAsUInt64("id") << " cpu mesh coordinates: x= " << getAttrAsUInt64("x_id") << " y= " << getAttrAsUInt64("y_id") << endl;
        }
    };
}

#endif  // VPSIM_DYNAMIC_DYNAMICCPUCONTROLLER_HPP
