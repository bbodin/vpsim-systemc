#ifndef VPSIM_DYNAMIC_DYNAMICCACHEIDCONTROLLER_HPP
#define VPSIM_DYNAMIC_DYNAMICCACHEIDCONTROLLER_HPP
#include <sstream>

#include <atomic>
#include "VpsimIp.hpp"
#include "CoherenceInterconnect.hpp"



namespace vpsim {
    typedef tlm::tlm_target_socket<> InPortType;
    typedef tlm::tlm_initiator_socket<> OutPortType;

struct DynamicCacheIdController
        : public VpsimIp<InPortType, OutPortType> {
public:
    DynamicCacheIdController(std::string name) : VpsimIp(std::move(name)) {
        registerRequiredAttribute("noc");
        registerRequiredAttribute("cache");
        registerRequiredAttribute("x_id");
        registerRequiredAttribute("y_id");
    }

    N_IN_PORTS_OVERRIDE(0);
    N_OUT_PORTS_OVERRIDE(0);

    InPortType *getNextInPort() override {
        throw runtime_error(getName() + " : Cache Controller has no in sockets.");
    }

    OutPortType *getNextOutPort() override {
        throw runtime_error(getName() + " : Cache Controller has no out sockets.");
    }

    void make() override {
        checkAttributes();
    }

    void finalize() override {
        VpsimIp *ip = VpsimIp::Find(getAttr("noc"));
        DynamicCoherenceInterconnect *noc = dynamic_cast<DynamicCoherenceInterconnect *>(ip);
        VpsimIp *ip1 = VpsimIp::Find(getAttr("cache"));
        DynamicCache *cache = dynamic_cast<DynamicCache *>(ip1);
        noc->mModulePtr->register_cpu_ctrl(cache->getId(),
                                           getAttrAsUInt64("x_id"),
                                           getAttrAsUInt64("y_id"));
        //cout << "cacheId= " << cache->getId() << " cache mesh coordinates: x= " << getAttrAsUInt64("x_id") << " y= " << getAttrAsUInt64("y_id") << endl;
    }
};
}


#endif  // VPSIM_DYNAMIC_DYNAMICCACHEIDCONTROLLER_HPP
