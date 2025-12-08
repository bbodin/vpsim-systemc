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

#ifndef _SESAMCONTROLLER_HPP_
#define _SESAMCONTROLLER_HPP_

#include <core/TargetIf.hpp>
#include <core/TlmCallbackPrivate.hpp>


#define SESAMOP_QUIT            0x42
#define SESAMOP_START_BENCH     0x52
#define SESAMOP_END_BENCH       0x54
#define SESAMOP_LIST            0x20
#define SESAMOP_CLEAN_PARAMS    0x58
#define SESAMOP_START_PARAM     0x62
#define SESAMOP_END_PARAM       0x72
#define SESAMOP_EXECUTE_PARAMS  0x78


namespace vpsim {

    class SesamController : public sc_module, public TargetIf<uint8_t> {
    public:
        SesamController(const sc_module_name& name);

        ~SesamController() override;

        tlm::tlm_response_status read(payload_t &payload, sc_time &delay);

        tlm::tlm_response_status write(payload_t &payload, sc_time &delay);

        virtual void sesamCommand(vector<string> &args, size_t counter = 0) {
        };

    private:
        string *strBuf;
        vector<string> strParam;

    protected:
        string mCommandOutputBuffer;
        //It is not a static variable, so not adapted if there are multiple instances of sesamController
        bool captureModeActivated = false; // Precaution for sesam benchmark commands overlapping
    };
}

#endif /* _SESAMCONTROLLER_HPP_ */