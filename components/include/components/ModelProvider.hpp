/*
 * Copyright (C) 2024 Commissariat à l'énergie atomique et aux énergies alternatives (CEA)

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 *    http://www.apache.org/licenses/LICENSE-2.0 

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#ifndef _MODELPROVIDER_HPP_
#define _MODELPROVIDER_HPP_

#include "global.hpp"
#include "InitiatorIf.hpp"
#include "quantum.hpp"
#include "InterruptIf.hpp"
#include "VpsimIp.hpp"
#include "MainMemCosim.hpp"
#include "IOAccessCosim.hpp"

#include <dlfcn.h>

#include <functional>
#include <utility>

#include "CacheBase.hpp"

using namespace tlm;

namespace vpsim {

    /* Instruction cache model based on CacheBase */
    class StandaloneInstructionCache : public CacheBase<uint64_t, uint64_t> {
    public:
        StandaloneInstructionCache(const sc_module_name& name,
                                   int cpu_id,
                                   uint64_t CacheSize,
                                   uint64_t CacheLineSize,
                                   uint64_t Associativity,
                                   CacheReplacementPolicy ReplPolicy = LRU)
            : CacheBase<uint64_t, uint64_t>(name, CacheSize, CacheLineSize, Associativity, ReplPolicy) {
            // anything ?
            SetEvictionNotifier(StandaloneInstructionCache::OnLineEvicted);
            mCpuId = cpu_id;
        }

        tlm::tlm_response_status ForwardRead(uint64_t Addr, size_t size, sc_time &delay) override {
            // register the fetch miss
            MainMemCosim::NotifyFetchMiss(mCpuId, (void *) Addr, size);
            return TLM_OK_RESPONSE;
        }

        static void OnLineEvicted(void *handle) {
            // Check if still valid
            int **flag = (int **) handle;

            if (*flag != &mZero && *flag != &mOne) {
                // throw runtime_error("Weird segfault was about to happen... :/");
            } else {
                *flag = &mZero; // stop assuming hits.
            }
        }

        int mCpuId;
        static int mZero;
        static int mOne;

        // victim list
        static void AppendVictim(void *victim) {
            Victims[victim] = true;
        }

        static map<void *, bool> Victims;
    };


    typedef void (*ThreadFunctionType)(void *cpu, uint64_t quantum);


    typedef uint64_t (*ReadCb)(void *opaque,
                               uint64_t addr,
                               unsigned size);

    typedef void (*WriteCb)(void *opaque,
                            uint64_t addr,
                            uint64_t data,
                            unsigned size);

    typedef void (*SyncCb)(void *opaque, uint64_t executed, int wfi);

    typedef uint64_t (*ICacheMissCb)(void *opaque, uint64_t addr, unsigned size, int *tb_hit);

    typedef void (*AddVictimCb)(void *victim);

    typedef void (*MainMemCb)(
        void *
        , uint64_t exec
        , uint8_t is_write
        , void *phys
        , uint64_t virt
        , unsigned int size
    );

    typedef uint64_t (*OuterStatGetter)(uint32_t index, enum OuterStat type);

    typedef void (*FillBiasCb)(uint64_t *ts, int n, double conversion_factor);

    typedef void (*IOAccessCb)(uint32_t device, uint64_t exec, uint8_t is_write
                               , void *phys
                               , uint64_t virt
                               , unsigned int size
                               , uint64_t tag
    );

    typedef uint64_t (*IOAccessStatGetter)(uint32_t device, enum IOAccessStat type);

    typedef uint8_t (*IOAccessGetDelayCb)(uint32_t device, uint64_t *time_stamp, uint64_t *delay, uint64_t *tag);

    typedef int (*modelprovider_configure_t)(int argc, char **argv, char **envp);

    typedef void (*modelprovider_set_default_read_callback_t)(ReadCb cb);

    typedef void (*modelprovider_set_default_write_callback_t)(WriteCb cb);

    typedef void (*modelprovider_set_sync_callback_t)(SyncCb cb);

    typedef void (*modelprovider_declare_external_dev_t)(char *name, uint64_t base, uint64_t size);

    typedef void (*modelprovider_declare_external_ram_t)(char *name, uint64_t base, uint64_t size, void *data);

    typedef void * (*modelprovider_create_internal_cpu_t)(void *proxy, char *type, int index, uint64_t start_pc,
                                                          int secure, int start_off);

    typedef void (*modelprovider_run_cpu_t)(void *cpu, uint64_t quantum);

    typedef void * (*modelprovider_create_internal_dev_default_t)(char *name,
                                                                  uint64_t base,
                                                                  int irq,
                                                                  ReadCb *rd,
                                                                  WriteCb *wr);

    typedef void (*modelprovider_poll_io_t)(void);

    typedef void (*modelprovider_finalize_config_t)(void);


    typedef void (*modelprovider_register_unlock_t)(void (*)(void *), void *);

    typedef void (*modelprovider_register_wait_unlock_t)(void (*)(void *), void *);

    typedef void (*modelprovider_interrupt_t)(int index, int value);

    typedef void (*modelprovider_cpu_get_stats_t)(int index, uint32_t *, void **);

    typedef void (*modelprovider_show_cpu_t)(void *handle);

    typedef void (*modelprovider_register_main_mem_callback_t)(MainMemCb, uint64_t);

    typedef void (*modelprovider_unregister_main_mem_callback_t)(void);

    typedef void (*modelprovider_register_outer_stat_cb_t)(OuterStatGetter);

    typedef void (*modelprovider_register_fill_bias_cb_t)(FillBiasCb, double);

    typedef void (*modelprovider_register_icache_miss_cb_t)(ICacheMissCb);

    typedef void (*modelprovider_register_add_victim_cb_t)(AddVictimCb);

    typedef void (*modelprovider_register_ioaccess_callback_t)(IOAccessCb);

    typedef void (*modelprovider_register_ioaccess_get_delay_cb_t)(IOAccessGetDelayCb);

    typedef void (*modelprovider_register_ioaccess_stat_cb_t)(IOAccessStatGetter);

#define LDFCT(typ,nm) nm=(typ##_t)loadSymbol(#typ)

    struct ModelProvider : public sc_module, public InterruptIf {
        ModelProvider(const sc_module_name& name, const string& path, uint64_t poll_period, uint64_t quantum = 1000,
                      double conversion_factor = 1.0) : sc_module(name), configured(false), poll_period(poll_period),
                                                        quantum(quantum), conversion_factor(conversion_factor) {
            lib = dlopen(path.c_str(), RTLD_LOCAL | RTLD_LAZY);

            if (!lib) {
                throw runtime_error(path + ": unable to load library " + dlerror());
            }
            LDFCT(modelprovider_configure, configure);
            LDFCT(modelprovider_set_default_read_callback, set_default_read_callback);
            LDFCT(modelprovider_set_default_write_callback, set_default_write_callback);
            LDFCT(modelprovider_set_sync_callback, set_sync_callback);
            LDFCT(modelprovider_run_cpu, run_cpu);
            LDFCT(modelprovider_poll_io, poll_io);
            LDFCT(modelprovider_declare_external_dev, declare_external_dev);
            LDFCT(modelprovider_declare_external_ram, declare_external_ram);
            LDFCT(modelprovider_create_internal_cpu, create_internal_cpu);
            LDFCT(modelprovider_create_internal_dev_default, create_internal_dev_default);
            LDFCT(modelprovider_finalize_config, finalize_config);
            LDFCT(modelprovider_register_unlock, modelprovider_unlock);
            LDFCT(modelprovider_register_wait_unlock, modelprovider_wait_unlock);
            LDFCT(modelprovider_interrupt, interrupt);
            LDFCT(modelprovider_cpu_get_stats, get_stats);
            LDFCT(modelprovider_show_cpu, show_cpu);
            LDFCT(modelprovider_register_main_mem_callback, modelprovider_register_main_mem_callback);
            LDFCT(modelprovider_unregister_main_mem_callback, modelprovider_unregister_main_mem_callback);
            LDFCT(modelprovider_register_outer_stat_cb, modelprovider_register_outer_stat_cb);
            LDFCT(modelprovider_register_fill_bias_cb, modelprovider_register_fill_bias_cb);
            LDFCT(modelprovider_register_icache_miss_cb, modelprovider_register_icache_miss_cb);
            LDFCT(modelprovider_register_add_victim_cb, modelprovider_register_add_victim_cb);
            LDFCT(modelprovider_register_ioaccess_callback, modelprovider_register_ioaccess_callback);
            LDFCT(modelprovider_register_ioaccess_get_delay_cb, modelprovider_register_ioaccess_get_delay_cb);
            LDFCT(modelprovider_register_ioaccess_stat_cb, modelprovider_register_ioaccess_stat_cb);

            addParam1("ModelProvider");
            internal_executed = 0;
            wait_to_consume = SC_ZERO_TIME;
            max_time = SC_ZERO_TIME;

            SC_THREAD(io_thread);
            SC_THREAD(cpu_thread);
        }

        SC_HAS_PROCESS(ModelProvider);

        void *loadSymbol(const string& sym) {
            if (!lib)
                throw runtime_error("getting symbol from null library !");

            dlerror();
            void *ptr = dlsym(lib, sym.c_str());
            if (!ptr) {
                dlerror();
                throw runtime_error(string("ISS Wrapper: unable to load symbol: ") + sym);
            }
            return ptr;
        }

        void addParam1(const string& arg) {
            argv.push_back(arg);
        }

        void addParam2(const string& param, const string& value) {
            argv.push_back(param);
            argv.push_back(value);
        }

        void config() {
            if (configured)
                return;
            char **argv_c = (char **) malloc(sizeof(char *) * argv.size());
            int i = 0;
            for (string &arg: argv) {
                argv_c[i++] = strdup(arg.c_str());
            }
            configure(i, argv_c, nullptr);
            configured = true;
        }

        void io_thread() {
                try {
                    while (true) {
                    poll_io();
                   }
                } catch (const std::exception &e) {
                    LOG_GLOBAL_ERROR << "IO_THREAD Failure:" << e.what() << std::endl;
                }
        }

        void cpu_thread() {
            try {
            run_cpu(NULL,
                    tlm::tlm_global_quantum::instance().get().to_seconds()
                    * 1000000000);
            LOG_GLOBAL_INFO << "End of cpu_thread" << std::endl;
                } catch (const std::exception &e) {
                    LOG_GLOBAL_ERROR << "CPU_THREAD Failure:" << e.what() << std::endl;
                }
        }

        static void get_cpu_biases(uint64_t *times, int n, double conversion_factor) {
            MainMemCosim::FillBiases(times, n, conversion_factor);
        }

        void wait_unlock() {
            wait(big_mutex);
        }

        void unlock() {
            big_mutex.notify(SC_ZERO_TIME);
            wait(1, SC_NS);
        }

        void update_irq(uint64_t val, uint32_t irq_idx) override {
            if (val) {
                sysc_event.notify(SC_ZERO_TIME);
            }
            interrupt(irq_idx, val);
        }

        void sync(uint64_t executed, bool wait_for_event) {
            internal_executed += executed;

            if (internal_executed >= poll_period || (internal_executed && wait_for_event)) {
                wait(internal_executed, SC_NS);
                internal_executed = 0;
            } else if (wait_for_event) {
                wait(sc_time(poll_period, SC_NS), sysc_event);
            }
        }

        void *lib;
        vector<string> argv;

        bool configured;
        sc_event check_io_event;
        uint64_t poll_period;
        uint64_t quantum;
        double conversion_factor;

        sc_event big_mutex;
        sc_event sysc_event;

        sc_time wait_to_consume;
        sc_time max_time;

        uint64_t internal_executed;

        // functions

        modelprovider_configure_t configure;
        modelprovider_set_default_read_callback_t set_default_read_callback;
        modelprovider_set_default_write_callback_t set_default_write_callback;
        modelprovider_set_sync_callback_t set_sync_callback;
        modelprovider_run_cpu_t run_cpu;
        modelprovider_poll_io_t poll_io;

        modelprovider_declare_external_dev_t declare_external_dev;
        modelprovider_declare_external_ram_t declare_external_ram;
        modelprovider_create_internal_cpu_t create_internal_cpu;

        modelprovider_create_internal_dev_default_t create_internal_dev_default;

        modelprovider_finalize_config_t finalize_config;
        modelprovider_register_unlock_t modelprovider_unlock;
        modelprovider_register_wait_unlock_t modelprovider_wait_unlock;

        modelprovider_interrupt_t interrupt;
        modelprovider_cpu_get_stats_t get_stats;
        modelprovider_show_cpu_t show_cpu;

        modelprovider_register_main_mem_callback_t modelprovider_register_main_mem_callback;
        modelprovider_unregister_main_mem_callback_t modelprovider_unregister_main_mem_callback;
        modelprovider_register_outer_stat_cb_t modelprovider_register_outer_stat_cb;
        modelprovider_register_fill_bias_cb_t modelprovider_register_fill_bias_cb;
        modelprovider_register_icache_miss_cb_t modelprovider_register_icache_miss_cb;
        modelprovider_register_add_victim_cb_t modelprovider_register_add_victim_cb;

        modelprovider_register_ioaccess_callback_t modelprovider_register_ioaccess_callback;
        modelprovider_register_ioaccess_get_delay_cb_t modelprovider_register_ioaccess_get_delay_cb;
        modelprovider_register_ioaccess_stat_cb_t modelprovider_register_ioaccess_stat_cb;
    };

    struct ModelProviderDev : public sc_module {
        ModelProviderDev(const sc_module_name& name, string model, uint64_t addr, uint32_t size, int irq) : sc_module(
                name),
            //TargetIf(string(name), size),
            model(std::move(model)),
            read_callback(nullptr),
            write_callback(nullptr),
            internal_dev(nullptr),
            irq(irq), get_stats(nullptr) {
            //TargetIf <REG_T>::RegisterReadAccess(REGISTER(ModelProviderDev,read));
            //TargetIf <REG_T>::RegisterWriteAccess(REGISTER(ModelProviderDev,write));

            //setBaseAddress(addr);

            base_address = addr;
        }

        tlm::tlm_response_status read(const payload_t &payload, sc_time &delay) const {
            if (!read_callback || !write_callback || !internal_dev)
                throw runtime_error("ModelProviderDev: not properly initialized !");

            uint64_t res = read_callback(internal_dev, payload.addr, payload.len);
            memcpy(payload.ptr, &res, payload.len);

            return TLM_OK_RESPONSE;
        }

        tlm::tlm_response_status write(payload_t &payload, sc_time &delay) {
            if (!read_callback || !write_callback || !internal_dev)
                throw runtime_error("ModelProviderDev: not properly initialized !");

            uint64_t wr = 0;
            memcpy(&wr, payload.ptr, payload.len);
            write_callback(internal_dev, payload.addr, wr, payload.len);

            return TLM_OK_RESPONSE;
        }

        uint64_t getBaseAddress() {
            return base_address;
        }

        void setProvider(ModelProvider *prov) {
            //prov->config();
            get_stats = prov->get_stats;
            internal_dev = prov->create_internal_dev_default((char *) model.c_str(), getBaseAddress(), irq,
                                                             &read_callback, &write_callback);
        }

        string model;

        ReadCb read_callback;
        WriteCb write_callback;
        void *internal_dev;
        uint64_t base_address;
        int irq;

        modelprovider_cpu_get_stats_t get_stats;
    };

    struct ModelProviderCpu : public sc_module, public InitiatorIf, public InterruptIf {
        ModelProviderCpu(const sc_module_name& name, string model, uint32_t index, uint64_t start_pc, uint64_t quantum,
                         int secure, int start_off,
                         uint64_t iCacheSize,
                         uint64_t iCacheLineSize,
                         uint64_t iCacheAssociativity,
                         CacheReplacementPolicy iCacheReplPolicy) : sc_module(name),
                                                                    InitiatorIf(string(name), quantum, true, 1),
                                                                    model(std::move(model)),
                                                                    index(index),
                                                                    start_pc(start_pc),
                                                                    quantum(quantum),
                                                                    thread_function(nullptr),
                                                                    internal_cpu(nullptr),
                                                                    quantum_keeper(quantum),
                                                                    secure(secure),
                                                                    start_off(start_off),

                                                                    icache((string(name) + "_icache").c_str(), index,
                                                                           iCacheSize, iCacheLineSize,
                                                                           iCacheAssociativity, iCacheReplPolicy) {
            //SC_THREAD(exec_thread_function);
        }

        SC_HAS_PROCESS(ModelProviderCpu);

        void exec_thread_function() {
            if (!thread_function || !internal_cpu || !provider) {
                throw runtime_error("ModelProviderCpu: not properly intialized.");
            }
            provider->config();
            thread_function(internal_cpu, quantum);
        }

        uint64_t do_read(uint64_t addr,
                         unsigned size) {
            uint64_t res = 0;
            InitiatorIf::tlm_error_checking(
                InitiatorIf::target_mem_access(0, addr, size, (uint8_t *) &res,
                                               READ, local_bias, index)
            );
            return res;
        }

        void do_write(uint64_t addr,
                      uint64_t data,
                      unsigned size) {
            InitiatorIf::tlm_error_checking(
                InitiatorIf::target_mem_access(0, addr, size, (uint8_t *) &data,
                                               WRITE, local_bias, index)
            );
        }

        void sync(uint64_t executed) {
            quantum_keeper += sc_time(executed, SC_NS);
            quantum_keeper.sync();
            throw (0);
        }

        void update_irq(uint64_t val, uint32_t irq_idx) override {
            //cerr<<"warning: ModelProviderCpu: interrupt not yet implemented."<<endl;
        }

        void setProvider(ModelProvider *prov) {
            //prov->config();
            thread_function = prov->run_cpu;
            internal_cpu = prov->create_internal_cpu((void *) this, (char *) model.c_str(), index, start_pc, secure,
                                                     start_off);
            provider = prov;
            get_stats = prov->get_stats;
        }

        void show_cpu() {
            provider->show_cpu(internal_cpu);
        }

        string model;
        uint32_t index;
        uint64_t start_pc;
        uint64_t quantum;

        ThreadFunctionType thread_function;
        void *internal_cpu;

        ParallelQuantumKeeper quantum_keeper;

        int secure, start_off;

        ModelProvider *provider;
        modelprovider_cpu_get_stats_t get_stats;

        sc_time local_bias;

        // instruction cache here ;)
        StandaloneInstructionCache icache;


        // should be a base class (AddressConverter)
        vector<tuple<void *, uint64_t, uint64_t> > mMaps;

        bool convertAddr(void *host, uint64_t *p) {
            uint64_t H = (uint64_t) host;
            for (auto &t: mMaps) {
                uint64_t Hp = (uint64_t) get < 0 > (t);
                //cout<<"Checking range: "<<hex<<Hp<<endl;
                if (H >= Hp && H < Hp + get < 2 > (t)) {
                    *p = get < 1 > (t) + (H - Hp);
                    return true;
                }
            }
            return false;
        }
    };

    
}

#endif /* _MODELPROVIDER_HPP_ */