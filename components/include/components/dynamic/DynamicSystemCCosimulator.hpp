#ifndef VPSIM_DYNAMIC_SYSTEMC_COSIMULATOR_HPP
#define VPSIM_DYNAMIC_SYSTEMC_COSIMULATOR_HPP

#include <sstream>

#include <atomic>
#include "VpsimIp.hpp"
#include "MainMemCosim.hpp"


namespace vpsim {
    typedef tlm::tlm_target_socket<> InPortType;
    typedef tlm::tlm_initiator_socket<> OutPortType;

    struct DynamicSystemCCosimulator : public VpsimIp<InPortType, OutPortType> {
    public:
        DynamicSystemCCosimulator(std::string name) : VpsimIp(std::move(name)), mModulePtr(nullptr) {
            registerRequiredAttribute("n_out_ports");
            registerOptionalAttribute("roi_only", "1");
        }

        NEEDS_DMI_OVERRIDE;

        N_IN_PORTS_OVERRIDE(0);
        N_OUT_PORTS_OVERRIDE(getAttrAsUInt64("n_out_ports")*2);

        InPortType *getNextInPort() override { throw runtime_error("No input ports for CPU."); }

        OutPortType *getNextOutPort() override {
            if (!mModulePtr) { throw runtime_error("Please call make() before handling ports."); }
            int cpu = mOutPortCounter / 2;
            int pt = mOutPortCounter % 2;
            return (pt == 0 ? mModulePtr->mOutPorts[cpu].first : mModulePtr->mOutPorts[cpu].second);
        }

        void make() override {
            if (mModulePtr != nullptr) { throw runtime_error("make() already called for DynamicSystemCCosimulator"); }
            checkAttributes();
            mModulePtr = new SystemCCosimulator(sc_module_name(getName().c_str()), getAttrAsUInt64("n_out_ports"));
            mModulePtr->setFocusOnROI(getAttrAsUInt64("roi_only"));
            unsigned cpu = 0;
            for (cpu = 0; cpu < getAttrAsUInt64("n_out_ports"); cpu++) {
                char ptName[512];
                sprintf(ptName, "fetch_port_%u", cpu);
                addOutPort(std::string(ptName));
                sprintf(ptName, "data_port_%u", cpu);
                addOutPort(std::string(ptName));
            }
        }

        void addDmiAddress(std::string targetIpName, uint64_t baseAddr, uint64_t size, unsigned char *pointer,
                           bool cached, bool has_dmi) override {
            if (has_dmi) { mModulePtr->mMaps.push_back(std::make_tuple((void *) pointer, baseAddr, size)); }
        }

        void finalize() override {}

        void setStatsAndDie() override { if (mModulePtr) { delete mModulePtr; } }

    private:
        SystemCCosimulator *mModulePtr;
        friend struct DynamicIOAccessCosimulator;
        friend struct DynamicSesamController;
    };
}

#endif // VPSIM_DYNAMIC_SYSTEMC_COSIMULATOR_HPP
