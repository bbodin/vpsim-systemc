/*
 * Umbrella header aggregating dynamic component definitions split into
 * individual headers under components/dynamic/.
 */
#ifndef DYNAMICCOMPONENTS_HPP_
#define DYNAMICCOMPONENTS_HPP_

#include "dynamic/DynamicExternalSimulator.hpp"
#include "dynamic/DynamicSystemCTarget.hpp"
#include "dynamic/DynamicGIC.hpp"
#include "dynamic/DynamicRemoteInitiator.hpp"
#include "dynamic/DynamicRemoteTarget.hpp"
#include "dynamic/DynamicSystemCCosimulator.hpp"
#include "dynamic/DynamicIOAccessCosimulator.hpp"
#include "dynamic/DynamicArm.hpp"
#include "dynamic/DynamicArm64.hpp"
#include "dynamic/DynamicVirtioProxy.hpp"
#include "dynamic/DynamicExternalCPU.hpp"
#include "dynamic/DynamicCache.hpp"
#include "dynamic/DynamicCoherenceInterconnect.hpp"
#include "dynamic/DynamicNoCDeviceController.hpp"
#include "dynamic/DynamicNoCMemoryController.hpp"
#include "dynamic/DynamicCacheController.hpp"
#include "dynamic/DynamicCacheIdController.hpp"
#include "dynamic/DynamicCpuController.hpp"
#include "dynamic/DynamicInterconnect.hpp"
#include "dynamic/DynamicNoCHomeNode.hpp"
#include "dynamic/DynamicNoCSource.hpp"
#include "dynamic/DynamicMemory.hpp"
#include "dynamic/DynamicBlobLoader.hpp"
#include "dynamic/DynamicElfLoader.hpp"
#include "dynamic/DynamicMonitor.hpp"
#include "dynamic/DynamicUart.hpp"
#include "dynamic/DynamicItCtrl.hpp"
#include "dynamic/DynamicPL011Uart.hpp"
#include "dynamic/DynamicXuartPs.hpp"
#include "dynamic/DynamicAddressTranslator.hpp"
#include "dynamic/DynamicTLMCallbackRegister.hpp"
#include "dynamic/DynamicSesamController.hpp"
#include "dynamic/DynamicPythonDevice.hpp"
#include "dynamic/DynamicModelProvider.hpp"
#include "dynamic/DynamicModelProviderParam.hpp"
#include "dynamic/DynamicModelProviderDev.hpp"
#include "dynamic/DynamicModelProviderCpu.hpp"

#endif // DYNAMICCOMPONENTS_HPP_
