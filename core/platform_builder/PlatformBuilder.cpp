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

#include "platform_builder/PlatformBuilder.hpp"


#include <utility>

namespace vpsim {
    int PlatformBuilder::Container = 0;

    PlatformBuilder::PlatformBuilder(std::string platformName) : mCurrentIp(nullptr) {
        LOG_GLOBAL_DEBUG(dbg0) << "PlatformBuilder cosntructor called." << std::endl;
        if (platformName == "")
            platformName = to_string(Container++);
        mCurrentIp = &beginBuild("Container", platformName);
    }

    PlatformBuilder::~PlatformBuilder() {

        LOG_GLOBAL_DEBUG(dbg0) << "PlatformBuilder destructor called." << std::endl;

        VpsimIp<InPortType, OutPortType>::GatherStats();

        for (auto inStack: mBuildStack) {
            if (inStack != nullptr) {
                delete inStack;
                return;
            }
        }
        if (mCurrentIp) delete mCurrentIp;
    }

    VpsimIp<InPortType, OutPortType> &PlatformBuilder::beginBuild(
        const std::string& ipType, const std::string& ipName) {
        
       LOG_GLOBAL_DEBUG(dbg2) << "Now building " << ipType << " " << ipName << std::endl;

        if (mCurrentIp && !mCurrentIp->isContainer()) {
            throw runtime_error(ipType + "Building outside container.");
        }
        mBuildStack.push_back(mCurrentIp);
        if (VpsimIp<InPortType, OutPortType>::IsKnown(ipType)) {
            mCurrentIp
                    = VpsimIp<InPortType, OutPortType>::NewByName(ipType, ipName);
        } else {
            throw runtime_error(ipType + " Undefined reference to type.");
        }
        return *mCurrentIp;
    }

    VpsimIp<InPortType, OutPortType> &PlatformBuilder::endBuild(
        VpsimIp<InPortType, OutPortType> **newIp) {
        if (mBuildStack.size() <= 1) {
            throw std::runtime_error("Too  many calls to endBuild()");
        }
        VpsimIp<InPortType, OutPortType> *newlyBuilt = mCurrentIp;
        mCurrentIp = mBuildStack.back();
        mBuildStack.pop_back();
        mCurrentIp->addChild(newlyBuilt);

        if (newIp != nullptr)
            *newIp = newlyBuilt;

        LOG_GLOBAL_DEBUG(dbg2) << "Done building " << newlyBuilt->getName()
                << ", now calling make()" << std::endl;

        newlyBuilt->make();
        mLocalIps.push_back(newlyBuilt->getName());

        LOG_GLOBAL_DEBUG(dbg2) << "Make ok !" << std::endl;
        return *mCurrentIp;
    }

    void PlatformBuilder::finalize() {
        VpsimIp<InPortType, OutPortType>::NotifyDmiAddresses(mLocalIps);
        VpsimIp<InPortType, OutPortType>::Finalize(mLocalIps);
    }

    void PlatformBuilder::setAttribute(std::string attr, std::string value) {
        mCurrentIp->setAttribute(std::move(attr), std::move(value));
    }

    void PlatformBuilder::connect(std::string srcIpName,
                                  std::string srcOutPortName, std::string dstIpName,
                                  std::string dstInPortName) {
        mCurrentIp->getChild(std::move(srcIpName))->connect(std::move(srcOutPortName),
                                                 mCurrentIp->getChild(std::move(dstIpName)), std::move(dstInPortName));
    }

    void PlatformBuilder::forwardInPort(std::string childName,
                                        std::string childInPortName, std::string portAlias) {
        mCurrentIp->forwardChildInPort(std::move(childName), std::move(childInPortName), std::move(portAlias));
    }

    void PlatformBuilder::forwardOutPort(std::string childName,
                                         std::string childOutPortName, std::string portAlias) {
        mCurrentIp->forwardChildOutPort(std::move(childName), std::move(childOutPortName), std::move(portAlias));
    }

    void PlatformBuilder::dumpComponents(ostream &stream) {
        for (auto cls = VpsimIp<InPortType, OutPortType>::RegisteredClasses.begin();
             cls != VpsimIp<InPortType, OutPortType>::RegisteredClasses.end(); cls++) {
            stream << "begin_component " << cls->first << endl;
            VpsimIp<InPortType, OutPortType> *dummy = VpsimIp<InPortType, OutPortType>::NewByName(
                cls->first, string(cls->first) + "_dummy");
            for (auto attr = dummy->mOptionalAttrs.begin(); attr != dummy->mOptionalAttrs.end(); attr++) {
                stream << "\toptional_attr " << attr->first << " " << attr->second << endl;
            }
            for (auto attr = dummy->mRequiredAttrs.begin(); attr != dummy->mRequiredAttrs.end(); attr++) {
                stream << "\trequired_attr " << *attr << endl;
            }

            int inprts = -1;
            int outprts = -1;

            try {
                inprts = dummy->getMaxInPortCount();
                outprts = dummy->getMaxOutPortCount();
            } catch (std::runtime_error &r) {
            }
            stream << "\tin_ports " << inprts << endl;
            stream << "\tout_prts " << outprts << endl;
            delete dummy;
            stream << "end_component" << endl;
        }
    }
}
