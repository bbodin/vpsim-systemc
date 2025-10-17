#ifndef VPSIM_DYNAMIC_DYNAMICELFLOADER_HPP
#define VPSIM_DYNAMIC_DYNAMICELFLOADER_HPP
#include <sstream>

#include <atomic>
#include "VpsimIp.hpp"
#include "memory/memory.hpp"



namespace vpsim {
    typedef tlm::tlm_target_socket<> InPortType;
    typedef tlm::tlm_initiator_socket<> OutPortType;

struct DynamicElfLoader : public VpsimIp<InPortType, OutPortType> {
    N_IN_PORTS_OVERRIDE(0);
    N_OUT_PORTS_OVERRIDE(0);

    NEEDS_DMI_OVERRIDE;

    DynamicElfLoader(string name) : VpsimIp(std::move(name)) {
        registerRequiredAttribute("path");
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

    void addDmiAddress(std::string targetIpName, uint64_t baseAddr, uint64_t size, unsigned char *pointer,
                               bool cached, bool has_dmi) override {
        if (has_dmi) {
            elfloader loader;
            loader.elfloader_init((char *) pointer, size);
            loader.load_elf_file(getAttr("path"), baseAddr, size, false);
        }
    }

    void finalize() override {
    }

    void setStatsAndDie() override {
    }
};
}


#endif  // VPSIM_DYNAMIC_DYNAMICELFLOADER_HPP
