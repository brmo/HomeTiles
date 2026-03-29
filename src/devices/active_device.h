#pragma once

#include "src/devices/device_select.h"

#if defined(DEVICE_M5STACK_TAB5)
#include "src/devices/m5stack_tab5/device_m5stack_tab5.h"
namespace DeviceImpl = DeviceM5StackTab5;
#elif defined(DEVICE_WAVESHARE_4B)
#include "src/devices/waveshare_4b/device_waveshare_4b.h"
namespace DeviceImpl = DeviceWaveshare4B;
#else
#error "No supported device target selected."
#endif
