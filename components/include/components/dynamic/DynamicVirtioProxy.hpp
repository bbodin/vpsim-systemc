#ifndef VPSIM_DYNAMIC_DYNAMICVIRTIOPROXY_HPP
#define VPSIM_DYNAMIC_DYNAMICVIRTIOPROXY_HPP
#include <sstream>

#include <atomic>
#include "VpsimIp.hpp"
#include "TargetIf.hpp"
#include "PL011Uart.hpp"
#include "VirtioTlm.hpp"
#include "compute/arm64.hpp"


namespace vpsim {
    typedef tlm::tlm_target_socket<> InPortType;
    typedef tlm::tlm_initiator_socket<> OutPortType;

    struct DynamicVirtioProxy : public VpsimIp<InPortType, OutPortType>, public VirtioTlm {
        DynamicVirtioProxy(const string& name) : VpsimIp(name), VirtioTlm(name.c_str()) {
            registerRequiredAttribute("provider_instance");
            registerRequiredAttribute("base_address");
            registerRequiredAttribute("irq");
            registerRequiredAttribute("device_type");
            registerRequiredAttribute("backend_config");
            registerOptionalAttribute("mac", "52:55:00:d1:55:01");
        }

        N_IN_PORTS_OVERRIDE(1);
        N_OUT_PORTS_OVERRIDE(0);
        MEMORY_MAPPED_OVERRIDE;

        InPortType *getNextInPort() override {
            return &mTargetSocket;
        }

        OutPortType *getNextOutPort() override {
            throw runtime_error(VpsimIp::getName() + " : Memory has no out sockets.");
        }

        void make() override {
            checkAttributes();
            setBaseAddress(getBaseAddress());
        }


        uint64_t getBaseAddress() override {
            return getAttrAsUInt64("base_address");
        }

        uint64_t getSize() override {
            return 0x1000;
        }

        unsigned char *getActualAddress() override {
            return (unsigned char *) -1;
        }

        void finalize() override {
            cout << "VIRTIO: Initializing callbacks..." << endl;
            pair<string, VpsimIp *> issProvider = VpsimIp::FindWithType(getAttr("provide_instance"));
            IssWrapper *wrapper = nullptr;
            if (issProvider.first == "Arm64") {
                wrapper = dynamic_cast<DynamicArm64 *>(issProvider.second)->getIssHandle();
            } else if (issProvider.first == "Arm") {
                wrapper = dynamic_cast<DynamicArm *>(issProvider.second)->getIssHandle();
            } else {
                throw runtime_error(
                    getAttr("provider_instance") +
                    " : Does not provide targets. Please provide valid ISS instance name.");
            }

            if (!wrapper) {
                throw runtime_error(getAttr("provider_instance") + " : Unable to get ISS instance for VIRTIO proxy.");
            }


            typedef void * (*sysbus_create_simple_t)(const char *, uint64_t, void *);
            auto create_f = (sysbus_create_simple_t) wrapper->get_symbol("vpsim_bus_create");
            create_f("virtio-mmio", getAttrAsUInt64("base_address"), (void *) getAttrAsUInt64("irq"));

            typedef void (*virtio_mmio_get_read_cb_t)(virtio_mmio_read_type *cb);
            typedef void (*virtio_mmio_get_write_cb_t)(virtio_mmio_write_type *cb);
            typedef void (*virtio_mmio_get_proxy_t)(void **proxy);

            auto virtio_mmio_get_proxy = (virtio_mmio_get_proxy_t) wrapper->get_symbol("virtio_mmio_get_proxy");
            auto virtio_mmio_get_read_cb = (virtio_mmio_get_read_cb_t) wrapper->get_symbol("virtio_mmio_get_read_cb");
            auto virtio_mmio_get_write_cb = (virtio_mmio_get_write_cb_t) wrapper->
                    get_symbol("virtio_mmio_get_write_cb");

            virtio_mmio_get_read_cb(&mRdFct);
            virtio_mmio_get_write_cb(&mWrFct);
            virtio_mmio_get_proxy(&mProxyPtr);

            typedef void (*io_step_t)();
            mIoStep = (io_step_t) wrapper->get_symbol("io_step_tlm");

            typedef void (*create_dev_t)(const char *name, const char *args, const char *extra);
            create_dev_t create_dev;
            string extra;
            // Now create device and backend
            if (getAttr("device_type") == "blk") {
                create_dev = (create_dev_t) wrapper->get_symbol("vpsim_create_blk");
            } else if (getAttr("device_type") == "net") {
                create_dev = (create_dev_t) wrapper->get_symbol("vpsim_create_net");
                extra = getAttr("mac");
            } else {
                throw runtime_error(
                    getAttr("device_type") + " is not a known virtio device type, known types are: blk, net.");
            }

            create_dev(VpsimIp::getName().c_str(), getAttr("backend_config").c_str(), extra.c_str());
        }
    };
}

#endif  // VPSIM_DYNAMIC_DYNAMICVIRTIOPROXY_HPP
