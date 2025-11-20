#ifndef VPSIM_DYNAMIC_DYNAMICMODELPROVIDERCPU_HPP
#define VPSIM_DYNAMIC_DYNAMICMODELPROVIDERCPU_HPP

#include "VpsimIp.hpp"
#include "ModelProvider.hpp"

namespace vpsim {
    typedef tlm::tlm_target_socket<> InPortType;
    typedef tlm::tlm_initiator_socket<> OutPortType;


    struct DynamicModelProviderCpu
            : public VpsimIp<InPortType, OutPortType> {
    public:
        DynamicModelProviderCpu(std::string name) : VpsimIp(std::move(name)),
                                                    mModulePtr(nullptr) {
            registerRequiredAttribute("model");
            registerRequiredAttribute("reset_pc");
            registerRequiredAttribute("provider");
            registerRequiredAttribute("id");
            registerRequiredAttribute("quantum");
            registerRequiredAttribute("secure");
            registerRequiredAttribute("start_powered_off");

            registerRequiredAttribute("icache_size");
            registerRequiredAttribute("icache_associativity");
            registerRequiredAttribute("icache_line_size");
            // registerRequiredAttribute("intc");
        }


        NEEDS_DMI_OVERRIDE;
        PROCESSOR_OVERRIDE;

        N_IN_PORTS_OVERRIDE (
        0
        );
        N_OUT_PORTS_OVERRIDE (
        1
        );

        InPortType *getNextInPort() override {
            throw runtime_error("No input ports for CPU.");
        }

        OutPortType *getNextOutPort() override {
            if (!mModulePtr) {
                throw runtime_error("Please call make() before handling ports.");
            }
            return mModulePtr->mInitiatorSocket[mOutPortCounter];
        }

        void pushStats() override {
             LOG_GLOBAL_DEBUG(dbg0) << "Your component " << this->getName() << " is asked to push stats.\n";
            if (mModulePtr) {
                struct ent {
                    char name[512];
                    uint64_t val;
                };
                ent *statlist;
                uint32_t count;
                mModulePtr->get_stats(mModulePtr->index, &count, (void **) &statlist);


                //printf("Initial stat push in CPU\n");
                if (mSegmentedStats.empty()) {
                    mSegmentedStats.push_back({
                        {string(statlist[0].name), "0"},
                        {string(statlist[1].name), "0"},
                        {string(statlist[2].name), "0"},
                        {string(statlist[3].name), "0"},
                        {string(statlist[4].name), "0"},
                        {string(statlist[5].name), "0"},
                        {string(statlist[6].name), "0"},
                        {string(statlist[7].name), "0"}
                    });
                }

                const auto &back = mSegmentedStats.back();

                //                for (uint32_t i = 0; i < count; i++) {
                //                    instructions[i] = statlist[i].val - stoull(back.at("executed_instructions"));
                //                    mSegmentedStats.push_back({
                //                        {"executed_instructions", to_string(instructions[i])}
                //                    });
                //                }

                mSegmentedStats.push_back({
                    {string(statlist[0].name), to_string(statlist[0].val - stoull(back.at(string(statlist[0].name))))},
                    {string(statlist[1].name), to_string(statlist[1].val - stoull(back.at(string(statlist[1].name))))},
                    {string(statlist[2].name), to_string(statlist[2].val - stoull(back.at(string(statlist[2].name))))},
                    {string(statlist[3].name), to_string(statlist[3].val - stoull(back.at(string(statlist[3].name))))},
                    {string(statlist[4].name), to_string(statlist[4].val - stoull(back.at(string(statlist[4].name))))},
                    {string(statlist[5].name), to_string(statlist[5].val - stoull(back.at(string(statlist[5].name))))},
                    {string(statlist[6].name), to_string(statlist[6].val - stoull(back.at(string(statlist[6].name))))},
                    {string(statlist[7].name), to_string(statlist[7].val - stoull(back.at(string(statlist[7].name))))}
                });
            }
        }

        void make() override {
            if (mModulePtr != nullptr) {
                throw runtime_error("make() already called for DynamicArm");
            }
            checkAttributes();
            mModulePtr = new ModelProviderCpu(
                getName().c_str(),
                getAttr("model"),
                getAttrAsUInt64("id"),
                getAttrAsUInt64("reset_pc"),
                getAttrAsUInt64("quantum"),
                getAttrAsUInt64("secure"),
                getAttrAsUInt64("start_powered_off"),

                // icache data
                /*size*/ getAttrAsUInt64("icache_size"),
                /* line size */ getAttrAsUInt64("icache_line_size"),
                /*assoc*/ getAttrAsUInt64("icache_associativity"),
                /*repl*/ LRU
            );
        }

        void addDmiAddress(std::string targetIpName, uint64_t baseAddr, uint64_t size, unsigned char *pointer,
                                   bool cached, bool has_dmi) override {
            mModulePtr->mMaps.push_back(make_tuple(pointer, baseAddr, size));
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
            /*VpsimIp *par=VpsimIp::Find(this->getAttr("provider"));
            if (par==nullptr)
                    throw runtime_error(getName() + ": Unfound provider :" + getAttr("provider"));
            DynamicModelProvider* mp = dynamic_cast<DynamicModelProvider*>(par);
            mModulePtr->setProvider(mp->mModulePtr);*/
            /*VpsimIp *par=VpsimIp::Find(this->getAttr("intc"));
            if (par==nullptr)
                    throw runtime_error(getName() + ": Unfound interrupt controller :" + getAttr("intc"));
            dynamic_cast<DynamicGIC*>(par)->connectCpu(getIrqIf(),getAttrAsUInt64("id"));*/
        }

        /*virtual VpsimModule* asModule() {
                return mModulePtr;
        }*/

        void setStatsAndDie() override {
            if (mModulePtr) {
                struct ent {
                    char name[512];
                    uint64_t val;
                };

                ent *statlist;
                uint32_t count;

                mModulePtr->get_stats(mModulePtr->index, &count, (void **) &statlist);
                for (unsigned i = 0; i < count; i++) {
                    mStats[string(statlist[i].name)] = to_string(statlist[i].val);
                }

                delete mModulePtr;
            }
        }

        void show() override {
            mModulePtr->show_cpu();
        }

        InterruptIf *getIrqIf() override {
            return mModulePtr;
        }


        ModelProviderCpu *mModulePtr;
    };


  } // end of vpsim

#endif /* VPSIM_DYNAMIC_DYNAMICMODELPROVIDERCPU_HPP */