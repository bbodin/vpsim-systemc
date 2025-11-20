#ifndef VPSIM_DYNAMIC_DYNAMICMODELPROVIDERPARAM_HPP
#define VPSIM_DYNAMIC_DYNAMICMODELPROVIDERPARAM_HPP

#include "VpsimIp.hpp"
#include "ModelProvider.hpp"

namespace vpsim {
    typedef tlm::tlm_target_socket<> InPortType;
    typedef tlm::tlm_initiator_socket<> OutPortType;
    struct DynamicModelProviderParam1 : public VpsimIp<InPortType, OutPortType> {
        DynamicModelProviderParam1(std::string name) : VpsimIp(std::move(name)) {
            registerRequiredAttribute("option");
            registerRequiredAttribute("provider");
        }

        N_IN_PORTS_OVERRIDE (
        0
        );
        N_OUT_PORTS_OVERRIDE (
        0
        );

        InPortType *getNextInPort() override {
            throw runtime_error("No input ports for model provider param.");
        }

        OutPortType *getNextOutPort() override {
            throw runtime_error("no output ports for model provider param.");
        }

        void make() override {
            checkAttributes();
        }

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
            /*VpsimIp *par=VpsimIp::Find(this->getAttr("provider"));
            if (par==nullptr)
                    throw runtime_error(getName() + ": Unfound provider :" + getAttr("provider"));
            DynamicModelProvider* mp = dynamic_cast<DynamicModelProvider*>(par);
            mp->mModulePtr->addParam1(getAttr("option"));*/
        }

        void setStatsAndDie() override {
        }
    };

    struct DynamicModelProviderParam2 : public VpsimIp<InPortType, OutPortType> {
        DynamicModelProviderParam2(std::string name) : VpsimIp(std::move(name)) {
            registerRequiredAttribute("option");
            registerRequiredAttribute("value");
            registerRequiredAttribute("provider");
        }

        N_IN_PORTS_OVERRIDE (
        0
        );
        N_OUT_PORTS_OVERRIDE (
        0
        );

        InPortType *getNextInPort() override {
            throw runtime_error("No input ports for model provider param.");
        }

        OutPortType *getNextOutPort() override {
            throw runtime_error("no output ports for model provider param.");
        }

        void make() override {
            checkAttributes();
        }

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
            /*VpsimIp *par=VpsimIp::Find(this->getAttr("provider"));
            if (par==nullptr)
                    throw runtime_error(getName() + ": Unfound provider :" + getAttr("provider"));
            DynamicModelProvider* mp = dynamic_cast<DynamicModelProvider*>(par);
            mp->mModulePtr->addParam2(getAttr("option"),getAttr("value"));*/
        }

        void setStatsAndDie() override {
        }
    };


}

#endif /* VPSIM_DYNAMIC_DYNAMICMODELPROVIDERPARAM_HPP */