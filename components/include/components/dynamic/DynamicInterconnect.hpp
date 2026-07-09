#ifndef VPSIM_DYNAMIC_DYNAMICINTERCONNECT_HPP
#define VPSIM_DYNAMIC_DYNAMICINTERCONNECT_HPP
#include <sstream>

#include <atomic>
#include <logger/logger.hpp>
#include "VpsimIp.hpp"
#include "connect/interconnect.hpp"



namespace vpsim {
    typedef tlm::tlm_target_socket<> InPortType;
    typedef tlm::tlm_initiator_socket<> OutPortType;


    struct DynamicInterconnect
            : public VpsimIp<InPortType, OutPortType> {
    public:
        DynamicInterconnect(std::string name) : VpsimIp(std::move(name)),
                                                mConnectionCounter(0),
                                                mModulePtr(nullptr) {
            registerRequiredAttribute("latency");
            registerRequiredAttribute("n_in_ports");
            registerRequiredAttribute("n_out_ports");

            registerRequiredAttribute("is_mesh");
            registerRequiredAttribute("mesh_x");
            registerRequiredAttribute("mesh_y");
            registerRequiredAttribute("router_latency");
        }

        

        void setStats() override {
            if (mModulePtr) {
                for (unsigned i = 0; i < getMaxOutPortCount(); i++) {
                    mStats[string("written_bytes[") + std::to_string(i) + "]"] = std::to_string(mModulePtr->getWriteCount(i));
                    mStats[string("read_bytes[") + std::to_string(i) + "]"] = std::to_string(mModulePtr->getReadCount(i));
                }
            }
        }
        void terminate() override {
            if (mModulePtr) {
                delete mModulePtr;
            }
        }

        N_IN_PORTS_OVERRIDE(getAttrAsUInt64("n_in_ports"));
        N_OUT_PORTS_OVERRIDE(getAttrAsUInt64("n_out_ports"));

        InPortType *getNextInPort() override {
            if (!mModulePtr) {
                throw runtime_error(getName() + "Please call make() before handling ports.");
            }
            return &mModulePtr->socket_in[mInPortCounter];
        }

        OutPortType *getNextOutPort() override {
            if (!mModulePtr) {
                throw runtime_error(getName() + " Please call make() before handling ports.");
            }

            return &mModulePtr->socket_out[mOutPortCounter];
        }

        void make() override {
            if (mModulePtr != nullptr) {
                throw runtime_error(getName() + " make() already called !");
            }
            checkAttributes();

            const auto nInPorts = getAttrAsUInt64("n_in_ports");
            const auto nOutPorts = getAttrAsUInt64("n_out_ports");
            mModulePtr = new interconnect(sc_module_name(getName().c_str()), nInPorts, nOutPorts);
            if (!getAttrAsUInt64("is_mesh")) {
                mModulePtr->set_is_mesh(false);
                //mModulePtr->set_latency(sc_time(getAttrAsUInt64("latency"), SC_PS));
                mModulePtr->set_latency(sc_time(getAttrAsUInt64("latency"), SC_NS));
                mModulePtr->set_enable_latency(true);
            } else {
                mModulePtr->set_is_mesh(true);
                mModulePtr->set_mesh_coord(getAttrAsUInt64("mesh_x"), getAttrAsUInt64("mesh_x"));
                mModulePtr->set_router_latency(getAttrAsUInt64("router_latency"));
                mModulePtr->set_enable_latency(false);
            }
        }

        void connect(std::string outPortAlias, VpsimIp<InPortType, OutPortType> *otherIp,
                             std::string inPortAlias) override {
            // set address before connecting (used for forwarding)
            if (otherIp->isMemoryMapped()) {
                LOG_GLOBAL_DEBUG(dbg2) << "Interconnect mapping : " << std::hex << otherIp->getBaseAddress() << " - "  << std::dec << otherIp->getSize() << endl;
                mModulePtr->set_socket_out_addr(mConnectionCounter++, otherIp->getBaseAddress(), otherIp->getSize());
            } else {
                mModulePtr->setDefaultRoute(mConnectionCounter++);
            }
            VpsimIp<InPortType, OutPortType>::connect(outPortAlias, otherIp, inPortAlias);
        }

    private:
        uint32_t mConnectionCounter;
        interconnect *mModulePtr;

        friend struct DynamicNoCMemoryController;
        friend struct DynamicNoCSource;
        friend struct DynamicNoCHomeNode;
    };
}


#endif  // VPSIM_DYNAMIC_DYNAMICINTERCONNECT_HPP
