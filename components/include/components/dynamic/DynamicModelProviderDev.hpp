#ifndef VPSIM_DYNAMIC_DYNAMICMODELPROVIDERDEV_HPP
#define VPSIM_DYNAMIC_DYNAMICMODELPROVIDERDEV_HPP

#include "VpsimIp.hpp"
#include "ModelProvider.hpp"

namespace vpsim {
      typedef tlm::tlm_target_socket<> InPortType;
    typedef tlm::tlm_initiator_socket<> OutPortType;
    struct DynamicModelProviderDev
            : public VpsimIp<InPortType, OutPortType> {
    public:
        DynamicModelProviderDev(std::string name) : VpsimIp(std::move(name)),
                                                    mModulePtr(nullptr) {
            registerRequiredAttribute("model");
            registerRequiredAttribute("base_address");
            registerRequiredAttribute("size");
            registerRequiredAttribute("irq");
            registerRequiredAttribute("provider");
        }


        // MEMORY_MAPPED;

        N_IN_PORTS_OVERRIDE (
        0
        );
        N_OUT_PORTS_OVERRIDE (
        0
        );

        InPortType *getNextInPort() override {
            throw runtime_error("model provider device has no output ports.");
            // return &mModulePtr->mTargetSocket;
        }

        OutPortType *getNextOutPort() override {
            throw runtime_error("model provider device has no output ports.");
        }

        void make() override {
            if (mModulePtr != nullptr) {
                throw runtime_error("make() already called for DynamicArm");
            }
            checkAttributes();
            mModulePtr = new ModelProviderDev(getName().c_str(),
                                              getAttr("model"), getAttrAsUInt64("base_address"),
                                              getAttrAsUInt64("size"), getAttrAsUInt64("irq"));
        }

        /*
                virtual uint64_t getBaseAddress() override {
                        return getAttrAsUInt64("base_address");
                }

                virtual uint64_t getSize() override {
                        return getAttrAsUInt64("size");
                }*/

        void addDmiAddress(std::string targetIpName, uint64_t baseAddr, uint64_t size, unsigned char *pointer,
                                   bool cached, bool has_dmi) override {
        }

        void addMonitor(uint64_t base, uint64_t size) override {
        }

        void removeMonitor(uint64_t base, uint64_t size) override {
        }

        void showMonitor() override {
        }

        void finalize() override {
        }

        void setStatsAndDie() override {
            if (mModulePtr) {
                delete mModulePtr;
            }
        }


        ModelProviderDev *mModulePtr;
    };

}

#endif /* VPSIM_DYNAMIC_DYNAMICMODELPROVIDERDEV_HPP */