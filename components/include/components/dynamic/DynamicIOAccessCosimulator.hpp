#ifndef VPSIM_DYNAMIC_IOACCESS_COSIMULATOR_HPP
#define VPSIM_DYNAMIC_IOACCESS_COSIMULATOR_HPP

#include <sstream>

#include <atomic>
#include "VpsimIp.hpp"
#include "MainMemCosim.hpp"
#include "IOAccessCosim.hpp"
#include "DynamicSystemCCosimulator.hpp"



namespace vpsim {
    typedef tlm::tlm_target_socket<> InPortType;
    typedef tlm::tlm_initiator_socket<> OutPortType;


    struct DynamicIOAccessCosimulator
            : public VpsimIp<InPortType, OutPortType> {
    public:
        DynamicIOAccessCosimulator(std::string name) : VpsimIp(std::move(name)),
                                                       mModulePtr(nullptr) {
            registerRequiredAttribute("n_out_ports");
        }


        NEEDS_DMI_OVERRIDE;

        N_IN_PORTS_OVERRIDE(0);
        N_OUT_PORTS_OVERRIDE(getAttrAsUInt64("n_out_ports"));

        InPortType *getNextInPort() override {
            throw runtime_error("No input ports for DynamicIOAccessCosimulator.");
        }

        OutPortType *getNextOutPort() override {
            if (!mModulePtr) {
                throw runtime_error("Please call make() before handling ports.");
            }
            return mModulePtr->mOutPorts[mOutPortCounter];
        }

        void make() override {
            if (mModulePtr != nullptr) {
                throw runtime_error("make() already called for DynamicIOAccessCosimulator");
            }
            checkAttributes();
            mModulePtr = new IOAccessCosimulator(sc_module_name(getName().c_str()),
                                                 getAttrAsUInt64("n_out_ports"));

            unsigned iodevice = 0;
            for (iodevice = 0; iodevice < getAttrAsUInt64("n_out_ports"); iodevice++) {
                char ptName[512];
                sprintf(ptName, "dma_port_%d", iodevice);
                addOutPort(string(ptName));
            }
        }

        void addDmiAddress(std::string targetIpName, uint64_t baseAddr, uint64_t size, unsigned char *pointer,
                                   bool cached, bool has_dmi) override {
            if (has_dmi) {
                mModulePtr->mMaps.push_back(make_tuple((void *) pointer, baseAddr, size));
                //Cosim address space. necessary for tests but should be removed
                mModulePtr->mMaps.push_back(make_tuple((void *) 0, baseAddr, size));
                //Memory address space for IO notification comes from guest linux. Thus, it is different from that of Cosim
            }
        }

        void finalize() override {
            VpsimIp *ip = VpsimIp::Find("SystemCCosim0");
            DynamicSystemCCosimulator *cosim = dynamic_cast<DynamicSystemCCosimulator *>(ip);
            cosim->mModulePtr->setIOAccessPtr(mModulePtr);
        }

        void terminate() override {
            if (mModulePtr) {
                delete mModulePtr;
            }
        }

    private:
        IOAccessCosimulator *mModulePtr;
    };

}

#endif // VPSIM_DYNAMIC_IOACCESS_COSIMULATOR_HPP

