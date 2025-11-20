#ifndef VPSIM_DYNAMIC_DYNAMICBLOBLOADER_HPP
#define VPSIM_DYNAMIC_DYNAMICBLOBLOADER_HPP
#include <sstream>

#include <atomic>
#include "VpsimIp.hpp"
#include "memory/memory.hpp"


namespace vpsim {
    typedef tlm::tlm_target_socket<> InPortType;
    typedef tlm::tlm_initiator_socket<> OutPortType;

struct DynamicBlobLoader : public VpsimIp<InPortType, OutPortType> {
    N_IN_PORTS_OVERRIDE(0);
    N_OUT_PORTS_OVERRIDE(0);

    DynamicBlobLoader(string name) : VpsimIp(std::move(name)) {
        registerRequiredAttribute("target_memory");
        registerRequiredAttribute("file");
        registerRequiredAttribute("offset");
    }

    InPortType *getNextInPort() override {
        throw runtime_error(getName() + " : BlobLoader has no sockets.");
    }

    OutPortType *getNextOutPort() override {
        throw runtime_error(getName() + " : BlobLoader has no sockets.");
    }

    void make() override {
        checkAttributes();
    }

    void finalize() override {
        VpsimIp *mem = VpsimIp::Find(getAttr("target_memory"));
        if (!mem) {
            throw runtime_error(getName() + ": Could not find target memory " + getAttr("target_memory"));
        }
        dynamic_cast<DynamicMemory *>(mem)->mModulePtr->loadBlob(
            getAttr("file").c_str(),
            getAttrAsUInt64("offset"));
        cout << getName() << " successfully loaded file " << getAttr("file") << " into memory " << mem->getName() <<
                endl;
    }


};
}

#endif  // VPSIM_DYNAMIC_DYNAMICBLOBLOADER_HPP
