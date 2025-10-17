#ifndef VPSIM_DYNAMIC_DYNAMICEXTERNALCPU_HPP
#define VPSIM_DYNAMIC_DYNAMICEXTERNALCPU_HPP
#include <sstream>

#include <atomic>
#include "VpsimIp.hpp"
#include "components/SmartUart.hpp"
#include "gic.hpp"
#include "compute/arm.hpp"



namespace vpsim {
    typedef tlm::tlm_target_socket<> InPortType;
    typedef tlm::tlm_initiator_socket<> OutPortType;

    struct DynamicExternalCPU
            : public VpsimIp<InPortType, OutPortType>,
              public VpsimModule,
              public InterruptIf {
    public:
        DynamicExternalCPU(const std::string& name) : VpsimIp(name),
                                               VpsimModule(name, moduleType::intermediate, 1),
                                               lib(nullptr) {
            registerRequiredAttribute("lib_path");
            registerRequiredAttribute("quantum");
            registerRequiredAttribute("kernel");
            registerRequiredAttribute("model_name");
            registerRequiredAttribute("extra_arg");
            registerRequiredAttribute("gic");
            registerRequiredAttribute("id");
            registerRequiredAttribute("finalize");
            registerRequiredAttribute("n_smp_cpus");

            destry = nullptr;
        }

        ~DynamicExternalCPU() override {
            if (destry)
                destry();
        }

        //NEEDS_DMI_OVERRIDE;
        // PROCESSOR_OVERRIDE;  --> this only works for our ISSWrapper

        N_IN_PORTS_OVERRIDE(0);
        N_OUT_PORTS_OVERRIDE(1);

        using destroyFctType = void (*)();
        using extIrqFctType = void (*)(uint64_t, uint32_t);
        destroyFctType destry;

        InPortType *getNextInPort() override {
            throw runtime_error("No input ports for CPU.");
        }

        OutPortType *getNextOutPort() override {
            if (!lib) {
                throw runtime_error("Please call make() before handling ports.");
            }
            return nullptr;
        }

        void connect(std::string outPortAlias, VpsimIp<InPortType, OutPortType> *otherIp,
                             std::string inPortAlias) override {
            //std::cout<<"Connecting "<<getName()<<" to "<<otherIp->getName()<<std::endl;

            WrappedInSock thatSock = otherIp->getInPort(inPortAlias);

            if (thatSock.second != nullptr) {
                this->addSuccessor(*thatSock.second, 0);
            }
            using extFctType = void (*)(const char *, uint64_t, const char *, const char *, InPortType *);
            extFctType createAndConnectCPU = (extFctType) dlsym(
                lib, (getAttr("model_name") + "_createAndConnectCPU").c_str());
            if (!createAndConnectCPU) {
                throw runtime_error(string("Could not load initializer function !") + dlerror());
            }

            destry = (destroyFctType) dlsym(lib, (getAttr("model_name") + "_destroy").c_str());

            if (!destry) {
                cout << "warning: " << (string("Could not load destructor function !") + dlerror()) << endl;
            }


            createAndConnectCPU(getName().c_str(),
                                getAttrAsUInt64("quantum") / 1000,
                                getAttr("kernel").c_str(),
                                getAttr("extra_arg").c_str(),
                                thatSock.first);
        }

        void make() override {
            if (lib != nullptr) {
                throw runtime_error("make() already called for DynamicExternalCPU");
            }
            checkAttributes();
            // load shared library
            cout << "Opening library: " << getAttr("lib_path") << endl;
            lib = dlopen(getAttr("lib_path").c_str(), RTLD_LOCAL | RTLD_LAZY);
            if (!lib) {
                throw runtime_error(string("Could not load External CPU : ") + dlerror());
            }


            ext_update_irq = (extIrqFctType) dlsym(lib, (getAttr("model_name") + "_update_irq").c_str());
            if (!ext_update_irq) {
                throw runtime_error(string("Could not load update_irq function !") + dlerror());
            }
        }

        void finalize() override {
            if (!getAttrAsUInt64("finalize"))
                return;
            VpsimIp *par = VpsimIp::Find(this->getAttr("gic"));
            if (par == nullptr)
                throw runtime_error("External CPU: Please specify the gic attribute to point to an actual GIC.");
            for (uint32_t i = 0; i < getAttrAsUInt64("n_smp_cpus"); i++)
                dynamic_cast<DynamicGIC *>(par)->connectCpu(this, getAttrAsUInt64("id") + i);

            typedef void (*update_irq_cb_t)(void *instance, uint32_t id, int value, int line);
            typedef void (*register_irq_cb_t)(update_irq_cb_t cb, void *gic, uint32_t id);
            auto reg_cb = (register_irq_cb_t) dlsym(lib, "register_external_irq_callback");
            if (!reg_cb) {
                throw runtime_error(string("Could not load register_irq_cb function !") + dlerror());
            }
            reg_cb(update_irq_cb, (void *) dynamic_cast<DynamicGIC *>(par), getAttrAsUInt64("id"));

            typedef void (*set_id_t)(uint64_t id);
            auto set_id = (set_id_t) dlsym(lib, "vpsim_set_id");
            if (!set_id) {
                throw runtime_error("set_id function not found.\n");
            }
            set_id(getAttrAsUInt64("id"));
        }

        static void update_irq_cb(void *instance, uint32_t id, int value, int line) {
            ((DynamicGIC *) instance)->update_irq(value, line | ((1 << id) << 16));
        }

        void setStatsAndDie() override {
            if (lib) {
                dlclose(lib);
            }
        }

        void update_irq(uint64_t val, uint32_t irq_idx) override {
            ext_update_irq(val, irq_idx);
        }

        InterruptIf *getIrqIf() override { return this; }

    private:
        void *lib;
        extIrqFctType ext_update_irq;
    };
}

#endif  // VPSIM_DYNAMIC_DYNAMICEXTERNALCPU_HPP
