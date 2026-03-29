#pragma once

#if defined(DEVICE_M5STACK_TAB5) || defined(DEVICE_TAB5)
#include "src/devices/m5stack_tab5/device_m5stack_tab5.h"
namespace DeviceImpl = DeviceM5StackTab5;
#else
#include "src/devices/waveshare_4b/device_waveshare_4b.h"
namespace DeviceImpl = DeviceWaveshare4B;
#endif
