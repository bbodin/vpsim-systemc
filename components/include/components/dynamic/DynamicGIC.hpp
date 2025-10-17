#ifndef VPSIM_DYNAMIC_GIC_HPP
#define VPSIM_DYNAMIC_GIC_HPP

#include <sstream>

#include <atomic>
#include "VpsimIp.hpp"
#include "TargetIf.hpp"
#include "components/SmartUart.hpp"
#include "gic.hpp"



namespace vpsim {
    typedef tlm::tlm_target_socket<> InPortType;
    typedef tlm::tlm_initiator_socket<> OutPortType;


    struct DynamicGIC :
            public gic,
            public VpsimIp<InPortType, OutPortType> {
    public:
        explicit DynamicGIC(const std::string& name) : gic(name.c_str()),
                                                VpsimIp(name) {
            registerRequiredAttribute("base_address");

            registerRequiredAttribute("cpu_if_base");
            registerRequiredAttribute("cpu_if_size");
            registerRequiredAttribute("distributor_base");
            registerRequiredAttribute("distributor_size");

            registerRequiredAttribute("vdistributor_base");
            registerRequiredAttribute("vdistributor_size");
            registerRequiredAttribute("vcpu_if_base");
            registerRequiredAttribute("vcpu_if_size");
            registerRequiredAttribute("filter");
            registerRequiredAttribute("maintenance_irq");
        }

        ~DynamicGIC() override {
        }

        MEMORY_MAPPED_OVERRIDE;
        INTERRUPT_CONTROLLER_OVERRIDE;

        unsigned getMaxInPortCount() override {
            return 1;
        }

        unsigned getMaxOutPortCount() override {
            return 0;
        }

        InPortType *getNextInPort() override {
            return &mTargetSocket;
        }

        OutPortType *getNextOutPort() override {
            throw runtime_error(VpsimIp::getName() + " : GIC has no out sockets.");
        }

        void make() override {
            checkAttributes();
            setBaseAddress(getAttrAsUInt64("base_address"));
            setDistBase(getAttrAsUInt64("distributor_base"));
            setCPUBase(getAttrAsUInt64("cpu_if_base"));
            setDistSize(getAttrAsUInt64("distributor_size"));
            setCPUSize(getAttrAsUInt64("cpu_if_size"));

            setVDistBase(getAttrAsUInt64("vdistributor_base"));
            setVCPUBase(getAttrAsUInt64("vcpu_if_base"));
            setVDistSize(getAttrAsUInt64("vdistributor_size"));
            setVCPUSize(getAttrAsUInt64("vcpu_if_size"));

            setMaintenanceInterrupt(getAttrAsUInt64("maintenance_irq"));
        }

        uint64_t getBaseAddress() override {
            return getAttrAsUInt64("base_address");
        }

        uint64_t getSize() override {
            return 0x100000;
        }

        unsigned char *getActualAddress() override {
            return (unsigned char *) getLocalMem();
        }


        void finalize() override {
            if (AllInstances.find(getAttr("filter")) != AllInstances.end()) {
                VpsimIp<InPortType, OutPortType>::MapTypeIf(getAttr("filter"),
                                                            [](VpsimIp<InPortType, OutPortType> *ip) {
                                                                return ip->isProcessor();
                                                            },
                                                            [this](VpsimIp<InPortType, OutPortType> *ip) {
                                                                this->connectCpu(
                                                                    ip->getIrqIf(), ip->getAttrAsUInt64("cpu_id"));
                                                            }
                );
            }
        }

        InterruptIf *getIrqIf() override { return this; }

        void setStatsAndDie() override {
        }
    };
}

#endif // VPSIM_DYNAMIC_GIC_HPP
