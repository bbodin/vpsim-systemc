#ifndef VPSIM_DYNAMIC_DYNAMICCOHERENCEINTERCONNECT_HPP
#define VPSIM_DYNAMIC_DYNAMICCOHERENCEINTERCONNECT_HPP
#include <sstream>

#include <atomic>
#include "VpsimIp.hpp"
#include "CoherenceInterconnect.hpp"



namespace vpsim {
    typedef tlm::tlm_target_socket<> InPortType;
    typedef tlm::tlm_initiator_socket<> OutPortType;

    struct DynamicCoherenceInterconnect : public VpsimIp<InPortType, OutPortType> {
    public:
        DynamicCoherenceInterconnect(std::string name) : VpsimIp(std::move(name)),
                                                         mConnectionCounter_cache(0),
                                                         mConnectionCounter_home(0),
                                                         mConnectionCounter_mmapped(0),
                                                         mModulePtr(nullptr) {
            registerRequiredAttribute("latency");
            registerRequiredAttribute("n_cache_in");
            registerRequiredAttribute("n_cache_out");
            registerRequiredAttribute("n_home_in");
            registerRequiredAttribute("n_home_out");
            registerRequiredAttribute("n_mmapped");
            registerRequiredAttribute("n_device");
            registerRequiredAttribute("flitSize");
            registerRequiredAttribute("memory_word_length"),
                    registerRequiredAttribute("is_coherent");
            registerOptionalAttribute("memory_interleave_length", "0");
            registerOptionalAttribute("slc_interleave_length", "0");
            registerOptionalAttribute("latency_enable", "1");
            registerRequiredAttribute("is_mesh");
            registerRequiredAttribute("mesh_x");
            registerRequiredAttribute("mesh_y");
            registerRequiredAttribute("with_contention");
            registerRequiredAttribute("contention_interval");
            registerRequiredAttribute("buffer_size");
            registerRequiredAttribute("virtual_channels");
            registerRequiredAttribute("router_latency");
            registerRequiredAttribute("link_latency");
            registerRequiredAttribute("noc_stats_per_initiator_on");
        }

        void pushStats() override {
             LOG_GLOBAL_DEBUG(dbg0) << "Your component " << this->getName() << " is asked to push stats.\n";
            static uint64_t ns_per_sec = 1000000000;

            string distanceKey = string("Total_Distance");
            string latencyKey = string("Total_Latency");
            string packetsKey = string("Packets");
            string totallatencyKey = string("Total_Latency");
            string averagelatencyKey = string("Average_Latency");
            //string readsKey, writesKey;
            /*for (size_t i = 0; i<mModulePtr->getMMappedCount(); ++i) {
             readsKey   = string("memory_reads[")+tostr(i)+string("]");
             writesKey  = string("memory_writes[")+tostr(i)+string("]");
          }*/
            // Initialize mSegmentedStats
            if (mSegmentedStats.empty()) {
                mSegmentedStats.push_back({});
                auto &back = mSegmentedStats.back();
                /*for (size_t i = 0; i<mModulePtr->getMMappedCount(); ++i) {
                         //printf("MMappedCount=%d\n", mModulePtr->getMMappedCount());
                          readsKey   = string("memory_reads[")+tostr(i)+string("]");
                     writesKey  = string("memory_writes[")+tostr(i)+string("]");

                   back[readsKey]  = "0";
                 back[writesKey] = "0";
                 }*/

                back[distanceKey] = "0";
                back[latencyKey] = "0";
                back[packetsKey] = "0";
                if (getAttrAsUInt64("with_contention")) {
                    back[totallatencyKey] = "0";
                    for (size_t j = 0; j < getAttrAsUInt64("mesh_y"); j++) {
                        for (size_t i = 0; i < getAttrAsUInt64("mesh_x"); i++) {
                            back[string("Router(") + std::to_string(i) + string(",") + std::to_string(j) + string(")_") + string(
                                     "Packets")] = "0";
                            back[string("Router(") + std::to_string(i) + string(",") + std::to_string(j) + string(")_") + string(
                                     "Contention")] = "0";
                        }
                    }
                }


                //NoC stats per initiator
                if (getAttrAsUInt64("noc_stats_per_initiator_on")) {
                    for (const auto &itr: mModulePtr->initTotalStats) {
                        back[string("Initiator_") + std::to_string(get<0>(itr)) + string("(Packets_Sent)")] = "0";
                        back[string("Initiator_") + std::to_string(get<0>(itr)) + string("(Total_Distance)")] = "0";
                        back[string("Initiator_") + std::to_string(get<0>(itr)) + string("(Total_Network_Latency)")] = "0";
                    }
                }
                for (size_t i = 0; i < mModulePtr->getMMappedSize(); ++i) {
                    std::pair<size_t, size_t> pos = mModulePtr->getMMappedPos(i);
                    back[string("Memory(") + std::to_string(pos.first) + string(",") + std::to_string(pos.second) + string(")_") +
                         string("Reads")] = "0";
                    back[string("Memory(") + std::to_string(pos.first) + string(",") + std::to_string(pos.second) + string(")_") +
                         string("Writes")] = "0";
                }
            }

            // Update mSegmentedStats
            //const auto& back = mSegmentedStats.back();
            auto &back = mSegmentedStats.back();
            decltype(mSegmentedStats)::value_type newMap;
            /*for (size_t i = 0; i<mModulePtr->getMMappedCount(); ++i) {
             auto reads     = to_string(mModulePtr->getReadMemoryCount(i) - stoull(back.at(readsKey)));
             auto writes    = to_string(mModulePtr->getWriteMemoryCount(i) - stoull(back.at(writesKey)));
           auto reads     = to_string(mModulePtr->getReadMemoryCount(i) - stoull(back.at(string("memory_reads[")+tostr(i)+string("]"))));
             auto writes    = to_string(mModulePtr->getWriteMemoryCount(i) - stoull(back.at(string("memory_writes[")+tostr(i)+string("]"))));
           //newMap[readsKey]= reads;
             //newMap[writesKey]= writes;
           newMap[string("memory_reads[")+tostr(i)+string("]")]= reads;
           newMap[string("memory_writes[")+tostr(i)+string("]")]= writes;
             }*/


            auto totalDistance = to_string(mModulePtr->getTotalDistance() - stoull(back.at(distanceKey)));
            auto packetsCount = to_string(mModulePtr->getPacketsCount() - stoull(back.at(packetsKey)));
            newMap[distanceKey] = totalDistance;
            newMap[packetsKey] = packetsCount;
            if (!getAttrAsUInt64("with_contention")) {
                auto totalLatency = to_string(
                    (mModulePtr->getTotalLatency()).to_seconds() * ns_per_sec - stoull(back.at(latencyKey)));
                newMap[latencyKey] = totalLatency;
            }
            if (getAttrAsUInt64("with_contention")) {
                double totlat = (mModulePtr->getTotalLatencyWithContention()).to_seconds() * ns_per_sec - stoull(
                                    back.at(totallatencyKey));
                double avgLat = 0.0;
                if (stoull(packetsCount)) avgLat = totlat / stoull(packetsCount);
                auto AverageLatency = to_string(avgLat);
                auto TotalLatencyContention = to_string(totlat);
                newMap[totallatencyKey] = TotalLatencyContention;
                newMap[averagelatencyKey] = AverageLatency;
                //NoC stats per Router
                for (size_t j = 0; j < getAttrAsUInt64("mesh_y"); j++) {
                    for (size_t i = 0; i < getAttrAsUInt64("mesh_x"); i++) {
                        newMap[string("Router(") + std::to_string(i) + string(",") + std::to_string(j) + string(")_") + string("Packets")]
                                = std::to_string(
                                    mModulePtr->getRouterPacketsCount(i,j) - stoull(back.at(string("Router(")+std::to_string(i)+
                                        string(",")+std::to_string(j)+string(")_")+string("Packets"))));
                        newMap[string("Router(") + std::to_string(i) + string(",") + std::to_string(j) + string(")_") +
                               string("Contention")] = std::to_string(
                            (mModulePtr->getRouterTotalLatency(i,j)).to_seconds()*ns_per_sec - stod(back.at(string(
                                "Router(")+std::to_string(i)+string(",")+std::to_string(j)+string(")_")+string("Contention"))));
                    }
                }
            }
            for (size_t i = 0; i < mModulePtr->getMMappedSize(); ++i) {
                std::pair<size_t, size_t> pos = mModulePtr->getMMappedPos(i);
                newMap[string("Memory(") + std::to_string(pos.first) + string(",") + std::to_string(pos.second) + string(")_") +
                       string("Reads")] = std::to_string(
                    mModulePtr->getReadCount(i) - stoull(back.at(string("Memory(")+std::to_string(pos.first)+string(",")+std::to_string(
                        pos.second)+string(")_")+string("Reads"))));
                newMap[string("Memory(") + std::to_string(pos.first) + string(",") + std::to_string(pos.second) + string(")_") +
                       string("Writes")] = std::to_string(
                    mModulePtr->getWriteCount(i) - stoull(back.at(string("Memory(")+std::to_string(pos.first)+string(",")+std::to_string(
                        pos.second)+string(")_")+string("Writes"))));
            }
            //NoC stats per initiator
            if (getAttrAsUInt64("noc_stats_per_initiator_on")) {
                double avg_latency = 0.0;
                for (const auto &itr: mModulePtr->initTotalStats) {
                    //printf("initiator_id %d\n", get<0>(itr));
                    newMap[string("Initiator_") + std::to_string(get<0>(itr)) + string("(Mesh_Position)")] = get<0>(get<1>(itr));
                    //newMap[string("Initiator_")+ std::to_string(get<0>(itr))+string("(Mesh_Position)")] = std::to_string(get<0>(get<1>(itr)));
                    //newMap[string("Initiator_")+ std::to_string(get<0>(itr))+string("(Packets_Sent)")] = to_string(get<1>(get<1>(itr))-stoull(back.at(string("Initiator_")+ std::to_string(get<0>(itr))+string("(Packets_Sent)"))));
                    if (back[(string("Initiator_") + std::to_string(get<0>(itr)) + string("(Packets_Sent)"))] == "")
                        newMap[string("Initiator_") + std::to_string(get<0>(itr)) + string("(Packets_Sent)")] = to_string(
                            get<1>(get<1>(itr)));
                    else
                        newMap[string("Initiator_") + std::to_string(get<0>(itr)) + string("(Packets_Sent)")] = to_string(
                            get<1>(get<1>(itr)) - stoull(
                                back[(string("Initiator_") + std::to_string(get<0>(itr)) + string("(Packets_Sent)"))]));
                    //printf("nbr pckts sent is %d\n", stoull(newMap[string("Initiator_")+ std::to_string(get<0>(itr))+string("(Packets_Sent)")]));
                    //newMap[string("Initiator_")+ std::to_string(get<0>(itr))+string("(Total_Distance)")] = to_string(get<2>(get<1>(itr))-stoull(back.at(string("Initiator_")+ std::to_string(get<0>(itr))+string("(Total_Distance)"))));
                    if (back[(string("Initiator_") + std::to_string(get<0>(itr)) + string("(Total_Distance)"))] == "")
                        newMap[string("Initiator_") + std::to_string(get<0>(itr)) + string("(Total_Distance)")] = to_string(
                            get<2>(get<1>(itr)));
                    else
                        newMap[string("Initiator_") + std::to_string(get<0>(itr)) + string("(Total_Distance)")] = to_string(
                            get<2>(get<1>(itr)) - stoull(
                                back[(string("Initiator_") + std::to_string(get<0>(itr)) + string("(Total_Distance)"))]));
                    //newMap[string("Initiator_")+ std::to_string(get<0>(itr))+string("(Total_Network_Latency)")] = to_string((get<3>(get<1>(itr)).to_seconds())*ns_per_sec -stoull(back.at(string("Initiator_")+ std::to_string(get<0>(itr))+string("(Total_Network_Latency)"))));
                    if (back[(string("Initiator_") + std::to_string(get<0>(itr)) + string("(Total_Network_Latency)"))] == "")
                        newMap[string("Initiator_") + std::to_string(get<0>(itr)) + string("(Total_Network_Latency)")] =
                                to_string((get<3>(get<1>(itr)).to_seconds()) * ns_per_sec);
                    else
                        newMap[string("Initiator_") + std::to_string(get<0>(itr)) + string("(Total_Network_Latency)")] =
                                to_string((get<3>(get<1>(itr)).to_seconds()) * ns_per_sec - stoull(
                                              back[(string("Initiator_") + std::to_string(get<0>(itr)) + string(
                                                        "(Total_Network_Latency)"))]));
                    //printf("total latency is %d\n", stoull(newMap[string("Initiator_")+ std::to_string(get<0>(itr))+string("(Total_Network_Latency)")]));
                    if (stoull(newMap[string("Initiator_") + std::to_string(get<0>(itr)) + string("(Packets_Sent)")]) != 0) {
                        avg_latency = (double) stoull(
                                          newMap[string("Initiator_") + std::to_string(get<0>(itr)) +
                                                 string("(Total_Network_Latency)")]) / (double) stoull(
                                          newMap[string("Initiator_") + std::to_string(get<0>(itr)) + string("(Packets_Sent)")]);
                        newMap[string("Initiator_") + std::to_string(get<0>(itr)) + string("(Avg_Packet_Latency)")] = to_string(
                            avg_latency);
                    } else
                        newMap[string("Initiator_") + std::to_string(get<0>(itr)) + string("(Avg_Packet_Latency)")] = "0";

                    //printf("average latency is %f\n", stoull(newMap[string("Initiator_")+ std::to_string(get<0>(itr))+string("(Avg_Packet_Latency)")]));
                    //printf("average latency is %f\n", avg_latency);
                }
            }

            mSegmentedStats.push_back(move(newMap));
        }

        void setStatsAndDie() override {
            uint64_t ns_per_sec = 1000000000;
            if (mModulePtr) {
                if (getAttrAsUInt64("is_mesh")) {
                    /*for (unsigned i = 0; i<mModulePtr->getMMappedCount(); i++) {
                   mStats[string("read_bytes[") + std::to_string(i) + "]"]    = std::to_string(mModulePtr->getReadMemoryCount(i));
                      mStats[string("written_bytes[") + std::to_string(i) + "]"] = std::to_string(mModulePtr->getWriteMemoryCount(i));
                     }*/
                    mStats[string("Total_Distance")] = std::to_string(mModulePtr->getTotalDistance());
                    mStats[string("Packets")] = std::to_string(mModulePtr->getPacketsCount());
                    if (getAttrAsUInt64("with_contention")) {
                        double totlat = (mModulePtr->getTotalLatencyWithContention()).to_seconds() * ns_per_sec;
                        mStats[string("Total_Latency")] = std::to_string(totlat) + " ns";
                        double avgLat = 0.0;
                        if (mModulePtr->getPacketsCount()) avgLat = totlat / (mModulePtr->getPacketsCount());
                        //mStats[string("Total_Latency")]  = std::to_string((mModulePtr->getTotalLatencyWithContention()).to_seconds()*ns_per_sec) + " ns";
                        //double avgLat= ((mModulePtr->getTotalLatencyWithContention().to_seconds())*ns_per_sec)/(mModulePtr->getPacketsCount());
                        mStats[string("Average_Latency")] = std::to_string(avgLat) + " ns";
                        //NoC stats per Router
                        for (size_t j = 0; j < getAttrAsUInt64("mesh_y"); j++) {
                            for (size_t i = 0; i < getAttrAsUInt64("mesh_x"); i++) {
                                mStats[string("Router(") + std::to_string(i) + string(",") + std::to_string(j) + string(")_") +
                                       string("Packets")] = std::to_string(mModulePtr->getRouterPacketsCount(i,j));
                                mStats[string("Router(") + std::to_string(i) + string(",") + std::to_string(j) + string(")_") +
                                       string("Contention")] = std::to_string(
                                    (mModulePtr->getRouterTotalLatency(i,j)).to_seconds()*ns_per_sec) + " ns";
                            }
                        }
                    } else
                        mStats[string("Total_Latency")] =
                                std::to_string((mModulePtr->getTotalLatency()).to_seconds()*ns_per_sec) + " ns";
                    for (size_t i = 0; i < mModulePtr->getMMappedSize(); ++i) {
                        std::pair<size_t, size_t> pos = mModulePtr->getMMappedPos(i);
                        mStats[string("Memory(") + std::to_string(pos.first) + string(",") + std::to_string(pos.second) + string(")_") +
                               string("Reads")] = std::to_string(mModulePtr->getReadCount(i));
                        mStats[string("Memory(") + std::to_string(pos.first) + string(",") + std::to_string(pos.second) + string(")_") +
                               string("Writes")] = std::to_string(mModulePtr->getWriteCount(i));
                    }
                    //NoC stats per initiator
                    if (getAttrAsUInt64("noc_stats_per_initiator_on")) {
                        for (const auto &itr: mModulePtr->initTotalStats) {
                            //mStats[string("Initiator_")+ std::to_string(get<0>(itr))+string("(Mesh_Position)")] = get<0>(get<1>(itr));
                            //mStats[string("Initiator_")+ std::to_string(get<0>(itr))+string("(Mesh_Position)")] = std::to_string(get<0>(get<1>(itr)));
                            mStats[string("Initiator_") + std::to_string(get<0>(itr)) + string("(Packets_Sent)")] = std::to_string(
                                get<1>(get<1>(itr)));
                            mStats[string("Initiator_") + std::to_string(get<0>(itr)) + string("(Total_Distance)")] = std::to_string(
                                get<2>(get<1>(itr))) + " hops";
                            mStats[string("Initiator_") + std::to_string(get<0>(itr)) + string("(Total_Network_Latency)")] =
                                    std::to_string((get<3>(get<1>(itr)).to_seconds())*ns_per_sec) + " ns";
                            sc_time avg_lat = SC_ZERO_TIME;
                            if ((get<1>(get<1>(itr))) != 0)
                                avg_lat = ((get<3>(get<1>(itr)))) / ((get<1>(get<1>(itr))));
                            mStats[string("Initiator_") + std::to_string(get<0>(itr)) + string("(Avg_Packet_Latency)")] = std::to_string(
                                avg_lat.to_seconds()*ns_per_sec) + " ns";
                        }

                        (mModulePtr->initTotalStats).clear();
                    }
                }
                delete mModulePtr;
            }
        }

        unsigned getMaxInPortCount() override {
            return getAttrAsUInt64("n_cache_in") + getAttrAsUInt64("n_home_in") + getAttrAsUInt64("n_device");
        }

        unsigned getMaxOutPortCount() override {
            return getAttrAsUInt64("n_cache_out") + getAttrAsUInt64("n_home_out") + getAttrAsUInt64("n_mmapped");
        }

        InPortType *getNextInPort() override {
            throw runtime_error(VpsimIp::getName() + " : Does not support dynamic port allocation.");
        }

        OutPortType *getNextOutPort() override {
            throw runtime_error(VpsimIp::getName() + " : Does not support dynamic port allocation.");
        }

        sc_module *getScModule() override { return mModulePtr; }

        void make() override {
            if (mModulePtr != nullptr) throw runtime_error(getName() + " make() already called !");
            checkAttributes();
            mModulePtr = new CoherenceInterconnect(sc_module_name(getName().c_str()),
                                                   getAttrAsUInt64("n_cache_in"),
                                                   getAttrAsUInt64("n_cache_out"),
                                                   getAttrAsUInt64("n_home_in"),
                                                   getAttrAsUInt64("n_home_out"),
                                                   getAttrAsUInt64("n_mmapped"),
                                                   getAttrAsUInt64("n_device"),
                                                   getAttrAsUInt64("flitSize"),
                                                   getAttrAsUInt64("memory_word_length"),
                                                   getAttrAsUInt64("is_coherent"),
                                                   getAttrAsUInt64("memory_interleave_length"),
                                                   getAttrAsUInt64("slc_interleave_length"));
            setDelayStatCapture(true);
            if (!getAttrAsUInt64("is_mesh")) {
                mModulePtr->set_is_mesh(false);
                //mModulePtr->set_latency(sc_time(getAttrAsUInt64("latency"), SC_PS));
                mModulePtr->set_latency(sc_time(getAttrAsUInt64("latency"), SC_NS));
                mModulePtr->set_enable_latency(true);
            } else {
                mModulePtr->set_is_mesh(true);
                if (!getAttrAsUInt64("noc_stats_per_initiator_on"))
                    mModulePtr->set_noc_stats_per_initiator(false);
                else
                    mModulePtr->set_noc_stats_per_initiator(true);
                mModulePtr->set_mesh_coord(getAttrAsUInt64("mesh_x"), getAttrAsUInt64("mesh_y"));
                //mModulePtr->set_router_latency(getAttrAsUInt64("router_latency"));
                mModulePtr->set_router_latency(stod(getAttr("router_latency")));
                mModulePtr->set_enable_latency(false);
                if (getAttrAsUInt64("with_contention")) {
                    mModulePtr->set_contention(true);
                    //mModulePtr->set_contention_interval(getAttrAsUInt64("contention_interval"));
                    mModulePtr->set_contention_interval(stod(getAttr("contention_interval")));
                    mModulePtr->set_virtual_channels(getAttrAsUInt64("virtual_channels"));
                    mModulePtr->set_buffer_size(getAttrAsUInt64("buffer_size"));
                    //mModulePtr->set_link_latency(getAttrAsUInt64("link_latency"));
                    mModulePtr->set_link_latency(stod(getAttr("link_latency")));
                } else {
                    mModulePtr->set_contention(false);
                }
            }
            /* mModulePtr-> set_latency(sc_time(getAttrAsUInt64("latency"), SC_PS));
            if (getAttrAsUInt64("latency_enable")) mModulePtr->set_enable_latency(true);
           else mModulePtr-> set_enable_latency(false); */
            for (uint32_t i = 0; i < getAttrAsUInt64("n_cache_in"); i++) {
                addInPort(string("cache_in_") + to_string(i), mModulePtr->mCacheSocketsIn[i]);
            }
            for (uint32_t i = 0; i < getAttrAsUInt64("n_cache_out"); i++) {
                addOutPort(string("cache_out_") + to_string(i), mModulePtr->mCacheSocketsOut[i]);
                mModulePtr->set_cache_pos(string("cache_out_") + to_string(i), i);
            }
            for (uint32_t i = 0; i < getAttrAsUInt64("n_home_in"); i++) {
                addInPort(string("home_in_") + to_string(i), mModulePtr->mHomeSocketsIn[i]);
            }
            for (uint32_t i = 0; i < getAttrAsUInt64("n_home_out"); i++) {
                addOutPort(string("home_out_") + to_string(i), mModulePtr->mHomeSocketsOut[i]);
            }
            for (uint32_t i = 0; i < getAttrAsUInt64("n_mmapped"); i++) {
                addOutPort(string("mmapped_out_") + to_string(i), mModulePtr->mMMappedSocketsOut[i]);
            }
            for (uint32_t i = 0; i < getAttrAsUInt64("n_device"); i++) {
                addInPort(string("device_") + to_string(i), mModulePtr->mDeviceSocketsIn[i]);
            }
        }

        void connect(std::string outPortAlias, VpsimIp<InPortType, OutPortType> *otherIp,
                             std::string inPortAlias) override {
            if (otherIp->isMemoryMapped() && otherIp->isIdMapped())
                mModulePtr->
                        set_home_output(mConnectionCounter_home++, otherIp->getId(), otherIp->getBaseAddress(),
                                        otherIp->getSize());
            else if (otherIp->isIdMapped())
                mModulePtr->set_cache_id(mConnectionCounter_cache++, otherIp->getId(), outPortAlias);
            else if (otherIp->isMemoryMapped())
                mModulePtr->set_mmapped_output(mConnectionCounter_mmapped++, otherIp->getBaseAddress(),
                                               otherIp->getSize());
            else
                throw runtime_error("Component is not id-mapped nor home nor memory-mmaped\n");
            VpsimIp<InPortType, OutPortType>::connect(outPortAlias, otherIp, inPortAlias);
            //cout << "connect " << outPortAlias << " to " << inPortAlias << endl;
        }

    private:
        uint32_t mConnectionCounter_cache;
        uint32_t mConnectionCounter_home;
        uint32_t mConnectionCounter_mmapped;

        friend struct DynamicNoCMemoryController;
        friend struct DynamicCacheController;
        friend struct DynamicCacheIdController;
        friend struct DynamicCpuController;
        friend struct DynamicNoCSource;
        friend struct DynamicNoCHomeNode;
        //friend struct DynamicMemory;
        friend struct DynamicNoCDeviceController;
        CoherenceInterconnect *mModulePtr;
    };

}


#endif  // VPSIM_DYNAMIC_DYNAMICCOHERENCEINTERCONNECT_HPP
