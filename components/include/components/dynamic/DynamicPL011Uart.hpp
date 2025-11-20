#ifndef VPSIM_DYNAMIC_DYNAMICPL011UART_HPP
#define VPSIM_DYNAMIC_DYNAMICPL011UART_HPP
#include <sstream>

#include <atomic>
#include "VpsimIp.hpp"
#include "TargetIf.hpp"
#include "components/SmartUart.hpp"
#include "PL011Uart.hpp"



namespace vpsim {
    typedef tlm::tlm_target_socket<> InPortType;
    typedef tlm::tlm_initiator_socket<> OutPortType;

    struct DynamicPL011Uart :
            public PL011Uart,
            public VpsimIp<InPortType, OutPortType> {
    public:
        explicit DynamicPL011Uart(const std::string& name) : PL011Uart(name.c_str()),
                                                      VpsimIp(name) {
            registerRequiredAttribute("base_address");
            registerOptionalAttribute("size", "4095");
            registerOptionalAttribute("cycle_duration", "100e3");
            registerOptionalAttribute("write_cycles", "1");
            registerOptionalAttribute("read_cycles", "1");

            registerRequiredAttribute("interrupt_parent");
            registerRequiredAttribute("irq_n");
            registerRequiredAttribute("poll_period");

            registerRequiredAttribute("channel");
        }

        ~DynamicPL011Uart() override {
        }

        MEMORY_MAPPED_OVERRIDE;

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
            throw runtime_error(VpsimIp::getName() + " : PL011Uart has no out sockets.");
        }

        void make() override {
            checkAttributes();
            setBaseAddress(getAttrAsUInt64("base_address"));
            //setCycleDuration(sc_time(getAttrAsUInt64("cycle_duration"), SC_PS));
            setCycleDuration(sc_time(getAttrAsUInt64("cycle_duration"), SC_NS));
            setCyclesPerWrite(static_cast<int>(getAttrAsUInt64("write_cycles")));
            setInterruptLine(getAttrAsUInt64("irq_n"));
            //setPollPeriod(sc_time(getAttrAsUInt64("poll_period"), SC_PS));
            setPollPeriod(sc_time(getAttrAsUInt64("poll_period"), SC_NS));
            selectChannel(getAttr("channel"));
        }

        uint64_t getBaseAddress() override {
            return getAttrAsUInt64("base_address");
        }

        uint64_t getSize() override {
            return getAttrAsUInt64("size");
        }

        unsigned char *getActualAddress() override {
            return (unsigned char *) getLocalMem();
        }

        void finalize() override {
            //if (AllInstances.find("Arm") != AllInstances.end()) {

            VpsimIp<InPortType, OutPortType>::MapIf(
                [this](VpsimIp<InPortType, OutPortType> *ip) {
                    return ip->getName() == this->getAttr("interrupt_parent");
                },
                [this](VpsimIp<InPortType, OutPortType> *ip) {
                    this->setInterruptParent(ip->getIrqIf());
                    cout << "Set interrupt parent of " << this->VpsimIp::getName() << " to " << ip->getName() << endl;
                }
            );
            //}
        }


    };
}

#endif  // VPSIM_DYNAMIC_DYNAMICPL011UART_HPP
