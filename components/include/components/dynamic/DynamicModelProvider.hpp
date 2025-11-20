#ifndef VPSIM_DYNAMIC_DYNAMICMODELPROVIDER_HPP
#define VPSIM_DYNAMIC_DYNAMICMODELPROVIDER_HPP

#include "VpsimIp.hpp"
#include "ModelProvider.hpp"
#include "DynamicModelProviderDev.hpp"
#include "DynamicModelProviderCpu.hpp"
#include "DynamicModelProviderParam.hpp"

namespace vpsim {

    inline void model_provider_unlock_cb(void *opaque) {
        const auto mp = static_cast<ModelProvider *>(opaque);
        mp->unlock();
    }

    inline void model_provider_wait_unlock_cb(void *opaque) {
        const auto mp = static_cast<ModelProvider *>(opaque);
        mp->wait_unlock();
    }


    inline uint64_t model_provider_read_cb(void *opaque,
                                    uint64_t addr,
                                    unsigned size) {
        ModelProviderCpu *cpu = (ModelProviderCpu *) opaque;
        return cpu->do_read(addr, size);
    }

    inline void model_provider_write_cb(void *opaque,
                                 uint64_t addr,
                                 uint64_t data,
                                 unsigned size) {
        ModelProviderCpu *cpu = (ModelProviderCpu *) opaque;
        cpu->do_write(addr, data, size);
    }

    inline uint64_t model_provider_fetch_miss_cb(void *opaque
                                          , uint64_t addr
                                          , unsigned size
                                          , int *tb_hit
    ) {
        ModelProviderCpu *cpu = (ModelProviderCpu *) opaque;
        sc_time null;
        uint64_t phaddr = 0;
        cpu->convertAddr((void *) addr, &phaddr);
        cpu->icache.ReadData(nullptr, phaddr, size, cpu->index, cpu->index, null, null, (void *) tb_hit);
        *(int **) tb_hit = &cpu->icache.mOne;
        return cpu->icache.MissCount;
    }


    inline void model_provider_sync(void *opaque, uint64_t executed, int wfi) {
        ModelProviderCpu *cpu = (ModelProviderCpu *) opaque;
        cpu->provider->sync(executed, wfi);
    }

    /* void model_provider_main_mem_cb(void* opaque,
    		int write, void* phys, uint64_t virt, uint64_t size) {
    	ModelProviderCpu* cpu = (ModelProviderCpu*) opaque;
    	MainMemCosim::Notify(cpu->index,write,phys,size);
    }*/
    inline void model_provider_main_mem_cb(void *opaque, uint64_t exec,
                                    uint8_t write, void *phys, uint64_t virt, unsigned int size) {
        ModelProviderCpu *cpu = (ModelProviderCpu *) opaque;
        MainMemCosim::Notify(cpu->index, exec, write, phys, size);
    }

    inline uint64_t model_provider_outer_stat_cb(uint32_t index, enum OuterStat stat) {
        return MainMemCosim::GetStat(index, stat);
    }

    inline void model_provider_ioaccess_cb(uint32_t device, uint64_t exec,
                                    uint8_t write, void *phys, uint64_t virt, unsigned int size, uint64_t tag) {
        MainMemCosim::NotifyIO(device, exec, write, phys, virt, size, tag);
    }

    inline uint8_t model_provider_ioaccess_get_delay_cb(uint32_t device, uint64_t *time_stamp, uint64_t *delay,
                                                 uint64_t *tag) {
        return IOAccessCosim::GetDelay(device, time_stamp, delay, tag);
    }

    inline uint64_t model_provider_ioaccess_stat_cb(uint32_t device, enum IOAccessStat stat) {
        return IOAccessCosim::GetStat(device, stat);
    }





      typedef tlm::tlm_target_socket<> InPortType;
    typedef tlm::tlm_initiator_socket<> OutPortType;
    struct DynamicModelProvider : public VpsimIp<InPortType, OutPortType> {
        DynamicModelProvider(std::string name) : VpsimIp(std::move(name)),
                                                 mModulePtr(nullptr) {
            registerRequiredAttribute("path");
            registerRequiredAttribute("io_poll_period");
            registerOptionalAttribute("quantum", "1000");
            registerOptionalAttribute("conversion_factor", "1.0");
            registerRequiredAttribute("notify_main_memory_access");
            registerOptionalAttribute("roi_only", "1");


            registerRequiredAttribute("simulate_icache");

            registerRequiredAttribute("notify_ioaccess");
        }

        NEEDS_DMI_OVERRIDE;

        N_IN_PORTS_OVERRIDE (
        0
        );
        N_OUT_PORTS_OVERRIDE (
        0
        );

        InPortType *getNextInPort() override {
            throw runtime_error("No input ports for model provider.");
        }

        OutPortType *getNextOutPort() override {
            throw runtime_error("no output ports for model provider.");
        }

        void make() override {
            if (mModulePtr != nullptr) {
                throw runtime_error("make() already called for DynamicArm");
            }
            checkAttributes();
            mModulePtr = new ModelProvider(getName().c_str(), getAttr("path"), getAttrAsUInt64("io_poll_period"),
                                           getAttrAsUInt64("quantum"), stod(getAttr("conversion_factor")));


            mModulePtr->set_default_read_callback(model_provider_read_cb);
            mModulePtr->set_default_write_callback(model_provider_write_cb);
            mModulePtr->set_sync_callback(model_provider_sync);
            mModulePtr->modelprovider_unlock(model_provider_unlock_cb, (void *) mModulePtr);
            mModulePtr->modelprovider_wait_unlock(model_provider_wait_unlock_cb, (void *) mModulePtr);
            mModulePtr->modelprovider_register_fill_bias_cb(&ModelProvider::get_cpu_biases,
                                                            mModulePtr->conversion_factor);

            if (getAttrAsUInt64("notify_main_memory_access")) {
                if (!getAttrAsUInt64("roi_only")) mModulePtr->modelprovider_register_main_mem_callback(
                    model_provider_main_mem_cb, mModulePtr->quantum);
                else MainMemCosim::addRegisterMainMemCb(mModulePtr->modelprovider_register_main_mem_callback,
                                                        model_provider_main_mem_cb, mModulePtr->quantum,
                                                        mModulePtr->modelprovider_unregister_main_mem_callback);
                mModulePtr->modelprovider_register_outer_stat_cb(model_provider_outer_stat_cb);
            }

            if (getAttrAsUInt64("simulate_icache")) {
                mModulePtr->modelprovider_register_icache_miss_cb(model_provider_fetch_miss_cb);
                mModulePtr->modelprovider_register_add_victim_cb(&StandaloneInstructionCache::AppendVictim);
            }

            if (getAttrAsUInt64("notify_ioaccess")) {
                mModulePtr->modelprovider_register_ioaccess_callback(model_provider_ioaccess_cb);
                mModulePtr->modelprovider_register_ioaccess_get_delay_cb(model_provider_ioaccess_get_delay_cb);
                mModulePtr->modelprovider_register_ioaccess_stat_cb(model_provider_ioaccess_stat_cb);
            }
        }

        void addDmiAddress(std::string targetIpName, uint64_t baseAddr, uint64_t size, unsigned char *pointer,
                                   bool cached, bool has_dmi) override {
            if (mModulePtr == nullptr) {
                throw runtime_error(getName() + " : calling addDmiAddress() before make() !!!");
            }
            if (!mModulePtr->configured) {
                VpsimIp::MapTypeIf("ModelProviderParam1",
                                   [this](VpsimIp *target) {
                                       return target->getAttr("provider") == this->getName();
                                   },
                                   [this](VpsimIp *target) {
                                       this->mModulePtr->addParam1(target->getAttr("option"));
                                   });
                VpsimIp::MapTypeIf("ModelProviderParam2",
                                   [this](VpsimIp *target) {
                                       return target->getAttr("provider") == this->getName();
                                   },
                                   [this](VpsimIp *target) {
                                       this->mModulePtr->addParam2(target->getAttr("option"), target->getAttr("value"));
                                   });

                mModulePtr->config();
            }
            if (has_dmi) {
                mModulePtr->declare_external_ram((char *) targetIpName.c_str(), baseAddr, size, pointer);
            } else {
                mModulePtr->declare_external_dev((char *) targetIpName.c_str(), baseAddr, size);
            }
        }

        void addMonitor(uint64_t base, uint64_t size) override {
            //mModulePtr->monitorRange(base,size);
        }

        void removeMonitor(uint64_t base, uint64_t size) override {
            //mModulePtr->removeMonitor(base,size);
        }

        void showMonitor() override {
            //mModulePtr->showMonitor();
        }

        void finalize() override {
            // gather all params !
            if (!mModulePtr->configured) {
                VpsimIp::MapTypeIf("ModelProviderParam1",
                                   [this](VpsimIp *target) {
                                       return target->getAttr("provider") == this->getName();
                                   },
                                   [this](VpsimIp *target) {
                                       this->mModulePtr->addParam1(target->getAttr("option"));
                                   });
                VpsimIp::MapTypeIf("ModelProviderParam2",
                                   [this](VpsimIp *target) {
                                       return target->getAttr("provider") == this->getName();
                                   },
                                   [this](VpsimIp *target) {
                                       this->mModulePtr->addParam2(target->getAttr("option"), target->getAttr("value"));
                                   });

                mModulePtr->config();
            }
            VpsimIp::MapTypeIf("ModelProviderDev",
                               [this](VpsimIp *target) {
                                   return target->getAttr("provider") == this->getName();
                               },
                               [this](VpsimIp *target) {
                                   dynamic_cast<DynamicModelProviderDev *>(target)->mModulePtr->setProvider(
                                       this->mModulePtr);
                               });


            VpsimIp::MapTypeIf("ModelProviderCpu",
                               [this](VpsimIp *target) {
                                   return target->getAttr("provider") == this->getName();
                               },
                               [this](VpsimIp *target) {
                                   dynamic_cast<DynamicModelProviderCpu *>(target)->mModulePtr->setProvider(
                                       this->mModulePtr);
                               });

            mModulePtr->finalize_config();
        }

        void terminate() override {
            if (mModulePtr) {
                delete mModulePtr;
            }
        }

        InterruptIf *getIrqIf() override {
            return mModulePtr;
        }

        ModelProvider *mModulePtr;
    };


}

#endif /* VPSIM_DYNAMIC_DYNAMICMODELPROVIDER_HPP */