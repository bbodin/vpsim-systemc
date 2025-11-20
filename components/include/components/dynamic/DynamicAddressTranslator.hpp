#ifndef VPSIM_DYNAMIC_ADDRESS_TRANSLATOR_HPP
#define VPSIM_DYNAMIC_ADDRESS_TRANSLATOR_HPP

#include <sstream>
#include <string>

#include <atomic>
#include "VpsimIp.hpp"
#include "AddressTranslator.hpp"


namespace vpsim {
    typedef tlm::tlm_target_socket<> InPortType;
    typedef tlm::tlm_initiator_socket<> OutPortType;

    struct DynamicAddressTranslator :
            public VpsimIp<InPortType, OutPortType>,
            public AddressTranslator {
    public:
        explicit DynamicAddressTranslator(const std::string& name) : VpsimIp(name), AddressTranslator(name.c_str()) {
            registerRequiredAttribute("base_address");
            registerRequiredAttribute("size");
            registerRequiredAttribute("output_base_address");
        }


        MEMORY_MAPPED_OVERRIDE;

        unsigned getMaxInPortCount() override {
            return 1;
        }

        unsigned getMaxOutPortCount() override {
            return 1;
        }

        InPortType *getNextInPort() override {
            return &mSockIn;
        }

        OutPortType *getNextOutPort() override {
            return &mSockOut;
        }

        void make() override {
            checkAttributes();

            setShift(getAttrAsUInt64("output_base_address") - getAttrAsUInt64("base_address"));
        }

        uint64_t getBaseAddress() override {
            return getAttrAsUInt64("base_address");
        }

        uint64_t getSize() override {
            return getAttrAsUInt64("size");
        }

        unsigned char *getActualAddress() override {
            return (unsigned char *) nullptr;
        }

        void connect(std::string outPortAlias, VpsimIp<InPortType, OutPortType> *otherIp,
                            std::string inPortAlias) override {
            VpsimIp<InPortType, OutPortType>::connect(outPortAlias, otherIp, inPortAlias);
        }

        void finalize() override {
        }


    };
}
#endif