#ifndef VPSIM_DYNAMIC_DYNAMICCACHE_HPP
#define VPSIM_DYNAMIC_DYNAMICCACHE_HPP
#include <sstream>

#include <atomic>
#include "VpsimIp.hpp"
#include "memory/Cache.hpp"
#include "MainMemCosim.hpp"


namespace vpsim {
    typedef tlm::tlm_target_socket<> InPortType;
    typedef tlm::tlm_initiator_socket<> OutPortType;

    struct DynamicCache : public VpsimIp<InPortType, OutPortType> {
        DynamicCache(std::string name) : VpsimIp(std::move(name)),
                                         mModulePtr(nullptr) {
            registerRequiredAttribute("latency");
            registerRequiredAttribute("size");
            registerRequiredAttribute("line_size");
            registerRequiredAttribute("associativity");
            registerOptionalAttribute("nb_interleaved_caches", "0");
            registerRequiredAttribute("repl_policy");
            registerRequiredAttribute("writing_policy");
            registerRequiredAttribute("allocation_policy");
            registerRequiredAttribute("cpu");
            registerRequiredAttribute("local");
            registerRequiredAttribute("id");
            registerRequiredAttribute("level");
            registerRequiredAttribute("levels_number");
            registerRequiredAttribute("is_home");
            registerOptionalAttribute("inclusion_higher", "NINE");
            registerOptionalAttribute("inclusion_lower", "NINE");
            registerOptionalAttribute("is_coherent", "0");
            registerOptionalAttribute("home_base_address", "0");
            registerOptionalAttribute("home_size", "0");
            registerOptionalAttribute("l1i_simulate", "0");
        }

        inline unsigned int getnIn() {
            unsigned int nin = 1; // port for data from Level-1
            if (getAttrAsUInt64("level") < getAttrAsUInt64("levels_number")) nin++;
            // add port for invalidation if not LLC
            if (getAttrAsUInt64("level") == 2 && getAttrAsUInt64("l1i_simulate")) nin++;
            // add port for instruction cache if L2
            return nin;
        }

        /*inline unsigned int getnOut () { //Assuming 3 levels, TODO enhance
           if (getAttrAsUInt64("level") == 1) return 1;
           else if (getAttrAsUInt64("level") == 3) return 1; // one output for the NoC
            else if (getAttrAsUInt64("level") == 2) return 2;
          else return 1;
       }*/
        inline unsigned int getnOut() {
            //Assuming 3 levels, TODO enhance
            unsigned int nout = 1; // port for data
            if (getAttrAsUInt64("level") > 1 && !getAttrAsUInt64("is_home")) nout++;
            // add port for invalidation if not connected to NoC
            // if home, unique output for the NoC
            return nout;
        }

        NEEDS_DMI_OVERRIDE;
        ID_MAPPED_OVERRIDE;
        //MEMORY_MAPPED_OVERRIDE;
        //ISHOME (getAttrAsUInt64("level")==3); //Assuming 3 levels, TODO enhance
        N_IN_PORTS_OVERRIDE(getnIn());
        N_OUT_PORTS_OVERRIDE(getnOut());

        InPortType *getNextInPort() override {
            if (!mModulePtr) throw runtime_error(getName() + "Please call make() before handling ports.");
            return &mModulePtr->socket_in[mInPortCounter];
        }

        OutPortType *getNextOutPort() override {
            if (!mModulePtr) throw runtime_error(getName() + " Please call make() before handling ports.");
            return &mModulePtr->socket_out[mOutPortCounter];
        }

        void make() override {
            if (mModulePtr != nullptr) throw runtime_error("make() already called for DynamicCache");
            checkAttributes();
            CacheReplacementPolicy repl;
            if (getAttr("repl_policy") == "LRU") repl = LRU;
            else if (getAttr("repl_policy") == "FIFO") repl = FIFO;
            else if (getAttr("repl_policy") == "MRU") repl = MRU;
            else throw runtime_error(getAttr("repl_policy") + " Unknown replacement policy");
            CacheWritePolicy writePol;
            if (getAttr("writing_policy") == "WBack") writePol = WBack;
            else if (getAttr("writing_policy") == "WThrough") writePol = WThrough;
            else throw runtime_error(getAttr("write_policy") + " Unknown writing policy");
            CacheAllocPolicy allocPol;
            if (getAttr("allocation_policy") == "WAllocate") allocPol = WAllocate;
            else if (getAttr("allocation_policy") == "WAround") allocPol = WAround;
            else throw runtime_error(getAttr("alloc_policy") + " Unknown allocation policy");
            CacheInclusionPolicy incl_higher, incl_lower;
            if (getAttrAsUInt64("level") == 1) incl_higher = NINE;
            else {
                if (getAttr("inclusion_higher") == "Inclusive") incl_higher = Inclusive;
                if (getAttr("inclusion_higher") == "Exclusive") incl_higher = Exclusive;
                if (getAttr("inclusion_higher") == "NINE") incl_higher = NINE;
            }
            if (getAttrAsUInt64("level") == 3) incl_lower = NINE;
            else {
                if (getAttr("inclusion_lower") == "Inclusive") incl_lower = Inclusive;
                if (getAttr("inclusion_lower") == "Exclusive") incl_lower = Exclusive;
                if (getAttr("inclusion_lower") == "NINE") incl_lower = NINE;
            }
            mModulePtr = new Cache<uint64_t, uint64_t>(sc_module_name(getName().c_str()),
                                                       //sc_time(getAttrAsUInt64("latency"), SC_PS),
                                                       sc_time(getAttrAsUInt64("latency"), SC_NS),
                                                       getAttrAsUInt64("size"),
                                                       getAttrAsUInt64("line_size"),
                                                       getAttrAsUInt64("associativity"),
                                                       getAttrAsUInt64("nb_interleaved_caches"),
                                                       repl,
                                                       writePol,
                                                       allocPol,
                                                       false,
                                                       getAttrAsUInt64("id"),
                                                       getAttrAsUInt64("level"),
                                                       getnIn(),
                                                       getnOut(),
                                                       incl_higher,
                                                       incl_lower,
                                                       getAttrAsUInt64("is_home"),
                                                       getAttrAsUInt64("is_coherent"));
            setId(getAttrAsUInt64("id"));
            setDelayStatCapture(true);
            if (getAttrAsUInt64("local")) {
                mModulePtr->setIsPriv(true);
                //IOAccessCosim::RegStat(getAttrAsUInt64("cpu"), IOACCESS_READ,   &mModulePtr->WriteBacks);
                //IOAccessCosim::RegStat(getAttrAsUInt64("cpu"), IOACCESS_WRITE,  &mModulePtr->WriteBacks);
                if (getAttrAsUInt64("level") == 1) {
                    MainMemCosim::RegStat(getAttrAsUInt64("cpu"), L1_WB, &mModulePtr->WriteBacks);
                    MainMemCosim::RegStat(getAttrAsUInt64("cpu"), L1_MISS, &mModulePtr->MissCount);
                    MainMemCosim::RegStat(getAttrAsUInt64("cpu"), L1_LD, &mModulePtr->NReads);
                    MainMemCosim::RegStat(getAttrAsUInt64("cpu"), L1_ST, &mModulePtr->NWrites);
                } else if (getAttrAsUInt64("level") == 2) {
                    MainMemCosim::RegStat(getAttrAsUInt64("cpu"), L2_WB, &mModulePtr->WriteBacks);
                    MainMemCosim::RegStat(getAttrAsUInt64("cpu"), L2_MISS, &mModulePtr->MissCount);
                    MainMemCosim::RegStat(getAttrAsUInt64("cpu"), L2_LD, &mModulePtr->NReads);
                    MainMemCosim::RegStat(getAttrAsUInt64("cpu"), L2_ST, &mModulePtr->NWrites);
                }
            } else mModulePtr->setIsPriv(false);

            addInPort("in_data");
            if (getAttrAsUInt64("level") == 2 && getAttrAsUInt64("l1i_simulate")) addInPort("in_instruction");
            if (getAttrAsUInt64("level") < getAttrAsUInt64("levels_number")) addInPort("in_invalidate");
            addOutPort("out_data");
            if (getAttrAsUInt64("level") > 1 && !getAttrAsUInt64("is_home")) addOutPort("out_invalidate");
        }

        uint64_t getBaseAddress() override {
            return getAttrAsUInt64("home_base_address");
        }

        uint64_t getSize() override {
            return getAttrAsUInt64("home_size");
        }

        bool isMemoryMapped() override {
            return getAttrAsUInt64("is_home");
        }

        void addDmiAddress(std::string targetIpName, uint64_t baseAddr, uint64_t size, unsigned char *pointer,
                                   bool cached, bool has_dmi) override {
            if (mModulePtr == nullptr) throw runtime_error(getName() + " calling addDmiAddress() before make() !!!");
            if (!cached) mModulePtr->add_uncached_region(baseAddr, size);
            if (has_dmi) mModulePtr->setDmiRange(0, baseAddr, size, pointer);
        }

        void pushStats() override {
             LOG_GLOBAL_DEBUG(dbg2) << "Your component " << this->getName() << " is asked to push stats.\n";
            if (mSegmentedStats.empty())
                mSegmentedStats.push_back({
                    {"misses", "0"},
                    {"hits", "0"},
                    //{"uncached_forwards"  , "0"},
                    {"reads", "0"},
                    {"writes", "0"},
                    {"write_backs", "0"},
                    {"real_invalidations", "0"},
                    {"total_invalidations", "0"},
                    {"back_invalidations", "0"},
                    {"evictions", "0"},
                    {"evict_backs", "0"},
                    {"PutS", "0"},
                    {"PutM", "0"},
                    {"PutI", "0"},
                    {"GetS", "0"},
                    {"GetM", "0"},
                    {"FwdGetS", "0"},
                    {"FwdGetM", "0"}
                });
            const auto &back = mSegmentedStats.back();
            auto misses = to_string(mModulePtr->getMisses() - stoull(back.at("misses")));
            auto hits = to_string(mModulePtr->getHits() - stoull(back.at("hits")));
            //auto uncachedForwards = to_string(mModulePtr->getForwards() - stoull(back.at("uncached_forwards")));
            auto reads = to_string(mModulePtr->getReads() - stoull(back.at("reads")));
            auto writes = to_string(mModulePtr->getWrites() - stoull(back.at("writes")));
            auto writeBacks = to_string(mModulePtr->getWriteBacks() - stoull(back.at("write_backs")));
            auto invals = to_string(mModulePtr->getInvals() - stoull(back.at("real_invalidations")));
            auto totalInvals = to_string(mModulePtr->getTotalInvals() - stoull(back.at("total_invalidations")));
            auto backInvals = to_string(mModulePtr->getBackInvals() - stoull(back.at("back_invalidations")));
            auto evictions = to_string(mModulePtr->getEvictions() - stoull(back.at("evictions")));
            auto evictBacks = to_string(mModulePtr->getEvictBacks() - stoull(back.at("evict_backs")));
            auto PutS = to_string(mModulePtr->getPutS() - stoull(back.at("PutS")));
            auto PutM = to_string(mModulePtr->getPutM() - stoull(back.at("PutM")));
            auto PutI = to_string(mModulePtr->getPutI() - stoull(back.at("PutI")));
            auto GetS = to_string(mModulePtr->getGetS() - stoull(back.at("GetS")));
            auto GetM = to_string(mModulePtr->getGetM() - stoull(back.at("GetM")));
            auto FwdGetS = to_string(mModulePtr->getFwdGetS() - stoull(back.at("FwdGetS")));
            auto FwdGetM = to_string(mModulePtr->getFwdGetM() - stoull(back.at("FwdGetM")));

            mSegmentedStats.push_back({
                {"misses", misses},
                {"hits", hits},
                //{"uncached_forwards"  , uncachedForwards},
                {"reads", reads},
                {"writes", writes},
                {"write_backs", writeBacks},
                {"real_invalidations", invals},
                {"total_invalidations", totalInvals},
                {"back_invalidations", backInvals},
                {"evictions", evictions},
                {"evict_backs", evictBacks},
                {"PutS", PutS},
                {"PutM", PutM},
                {"PutI", PutI},
                {"GetS", GetS},
                {"GetM", GetM},
                {"FwdGetS", FwdGetS},
                {"FwdGetM", FwdGetM}
            });
        }

        void setStats() override {
            if (mModulePtr) {
                mStats["misses"] = std::to_string(mModulePtr->getMisses());
                mStats["hits"] = std::to_string(mModulePtr->getHits());
                //mStats["uncached_forwards"]   = std::to_string(mModulePtr->getForwards());
                mStats["reads"] = std::to_string(mModulePtr->getReads());
                mStats["writes"] = std::to_string(mModulePtr->getWrites());
                mStats["write_backs"] = std::to_string(mModulePtr->getWriteBacks());
                //if (mModulePtr->InclusionOfLower==Inclusive) {
                mStats["real_invalidations"] = std::to_string(mModulePtr->getInvals());
                mStats["total_invalidations"] = std::to_string(mModulePtr->getTotalInvals());
                //}
                //if (mModulePtr->InclusionOfHigher==Inclusive)
                mStats["back_invalidations"] = std::to_string(mModulePtr->getBackInvals());
                //if (mModulePtr->InclusionOfLower==Exclusive)
                mStats["evictions"] = std::to_string(mModulePtr->getEvictions());
                mStats["evict_backs"] = std::to_string(mModulePtr->getEvictBacks());
                mStats["PutS"] = std::to_string(mModulePtr->getPutS());
                mStats["PutM"] = std::to_string(mModulePtr->getPutM());
                mStats["PutI"] = std::to_string(mModulePtr->getPutI());
                mStats["GetS"] = std::to_string(mModulePtr->getGetS());
                mStats["GetM"] = std::to_string(mModulePtr->getGetM());
                mStats["FwdGetS"] = std::to_string(mModulePtr->getFwdGetS());
                mStats["FwdGetM"] = std::to_string(mModulePtr->getFwdGetM());
            }
        }
        
        void terminate() override {
            if (mModulePtr) {
                delete mModulePtr;
            }
        }




        void configure() override {
            mModulePtr->configure();
        }

    private:
        friend struct DynamicCacheIdController;
        Cache<uint64_t, uint64_t> *mModulePtr;
    };
}


#endif  // VPSIM_DYNAMIC_DYNAMICCACHE_HPP
