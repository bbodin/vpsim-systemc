#ifndef VPSIM_DYNAMIC_DYNAMICPYTHONDEVICE_HPP
#define VPSIM_DYNAMIC_DYNAMICPYTHONDEVICE_HPP
#include <sstream>

#include <atomic>
#include "VpsimIp.hpp"
#include "TargetIf.hpp"
#include "PythonDevice.hpp"



namespace vpsim {
    typedef tlm::tlm_target_socket<> InPortType;
    typedef tlm::tlm_initiator_socket<> OutPortType;

    struct DynamicPythonDevice :
            public VpsimIp<InPortType, OutPortType> {
        explicit DynamicPythonDevice(std::string name) : VpsimIp(std::move(name)),
                                                         mModulePtr(nullptr) {
            registerRequiredAttribute("base_address");
            registerRequiredAttribute("size");
            registerRequiredAttribute("interrupt_parent");
            registerRequiredAttribute("py_module_name");
            registerRequiredAttribute("param_string");
        }

        MEMORY_MAPPED_OVERRIDE;

        unsigned getMaxInPortCount() override {
            return 1;
        }

        unsigned getMaxOutPortCount() override {
            return 0;
        }

        InPortType *getNextInPort() override {
            return &mModulePtr->mTargetSocket;;
        }

        OutPortType *getNextOutPort() override {
            throw runtime_error("Python Device currently only has one input socket.");
        }

        void make() override {
            checkAttributes();
            string params = getAttr("param_string");
            char *x = strdup(params.c_str());
            char *token = strtok(x, ",");
            vector<string> tmp;
            while (token) {
                tmp.push_back(token);
                token = strtok(NULL, ",");
            }
            vector<string> tmpParam;
            map<string, string> args;
            for (auto t: tmp) {
                char *y = strdup(t.c_str());
                token = strtok(y, "=");
                while (token) {
                    tmpParam.push_back(token);
                    token = strtok(NULL, "=");
                }
                args[tmpParam.at(0)] = tmpParam.at(1);
                tmpParam.clear();
            }
            mModulePtr = new PyDevice(getName().c_str(), getAttr("py_module_name"), args, getAttrAsUInt64("size"));
            mModulePtr->setBaseAddress(getAttrAsUInt64("base_address"));
        }

        uint64_t getBaseAddress() override {
            return mModulePtr->getBaseAddress();
        }

        uint64_t getSize() override {
            return mModulePtr->getSize();
        }

        unsigned char *getActualAddress() override {
            return (unsigned char *) mModulePtr->getLocalMem();
        }

        void terminate() override {
            if (mModulePtr) {
                delete mModulePtr;
            }
        }

        void finalize() override {
            VpsimIp<InPortType, OutPortType>::MapIf(
                [this](VpsimIp<InPortType, OutPortType> *ip) {
                    return ip->getName() == this->getAttr("interrupt_parent");
                },
                [this](VpsimIp<InPortType, OutPortType> *ip) {
                    this->mModulePtr->setInterruptParent(ip->getIrqIf());
                    cout << "Set interrupt parent of " << this->VpsimIp::getName() << " to " << ip->getName() << endl;
                }
            );
        }

        sc_module *getScModule() override { return mModulePtr; }

    private:
        PyDevice *mModulePtr;
    };
}

#endif  // VPSIM_DYNAMIC_DYNAMICPYTHONDEVICE_HPP
