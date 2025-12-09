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

#include <SesamController.hpp>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>

const unsigned int MAX_BUF_LENGTH = 4096;

namespace vpsim {
    SesamController::SesamController(const sc_module_name& name) : sc_module(name), TargetIf(string(name), 0x4) {
        TargetIf<REG_T>::RegisterReadAccess(REGISTER(SesamController, read));
        TargetIf<REG_T>::RegisterWriteAccess(REGISTER(SesamController, write));
    }

    SesamController::~SesamController() {
    }

    tlm::tlm_response_status SesamController::read(payload_t &payload, sc_time &delay) {
        uint8_t data = 0;
        if (!payload.ptr) {
            throw runtime_error("Monitor does not support null payloads !");
        }
        if (payload.addr == getBaseAddress() + 1) {
            if (mCommandOutputBuffer.length() != 0) {
                data = mCommandOutputBuffer.c_str()[0];
                string buf = string(&mCommandOutputBuffer.c_str()[1]);
                mCommandOutputBuffer = buf;
            }
        } else {
            data = 42;
        }

        memcpy(payload.ptr, &data, payload.len);
        return tlm::TLM_OK_RESPONSE;
    }

    tlm::tlm_response_status SesamController::write(payload_t &payload, sc_time &delay) {

        if (!payload.ptr) {
            throw runtime_error("Monitor does not support null payloads !");
        }

        uint8_t *data = new uint8_t;
        
        memcpy(data, payload.ptr, payload.len);
        if (payload.addr == getBaseAddress()) {
            // command
            switch (*data) {
                case SESAMOP_LIST: {
                    strParam.clear();
                    strParam.push_back("list");
                    sesamCommand(strParam);
                }
                break;
                case SESAMOP_QUIT: {
                    // quit w/o question
                    strParam.clear();
                    strParam.push_back("quit");
                    sesamCommand(strParam);
                }
                break;
                case SESAMOP_START_BENCH: {
                    // start benchmark mode
                    string benchmark_name = strParam.back();
                    strParam.clear();
                    strParam.push_back("benchmark");
                    strParam.push_back(benchmark_name);
                    sesamCommand(strParam);
                }
                break;
                case SESAMOP_END_BENCH: {
                    // end benchmark mode
                    string benchmark_name = strParam.back();
                    strParam.clear();
                    strParam.push_back("endBenchmark");
                    strParam.push_back(benchmark_name);
                    sesamCommand(strParam);
                }
                break;
                case SESAMOP_CLEAN_PARAMS: {
                    // start receiving parameter
                    strParam.clear();
                }
                break;
                case SESAMOP_START_PARAM: {
                    // start receiving string
                    strBuf = new string;
                }
                break;
                case SESAMOP_END_PARAM: {
                    // end receiving string and add parameter
                    strParam.push_back(*strBuf);
                    delete strBuf;
                }
                break;
                case SESAMOP_EXECUTE_PARAMS: {
                    // end receiving parameter and execute command
                    sesamCommand(strParam);
                }
                break;
                default:
                    delete data;
                    throw runtime_error("SesamController in unknown command.");
            }
        } else if (payload.addr == (getBaseAddress() + 1)) {
            // data
            *strBuf += *data;
        }

        delete data;
        return tlm::TLM_OK_RESPONSE;
    }
}
