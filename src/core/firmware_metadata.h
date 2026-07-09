#pragma once

#include <Arduino.h>

namespace firmware_meta {

constexpr uint32_t kDeviceDescriptorMagic = 0x44565034u;  // "DVP4"
constexpr size_t kProjectKeyMaxLen = 32;
constexpr size_t kDeviceKeyMaxLen = 32;
constexpr size_t kDisplayNameMaxLen = 32;
constexpr size_t kDeviceDescriptorImageOffset = 24 + 8 + 256;

struct DeviceDescriptor {
  uint32_t magic_word;
  char project_key[kProjectKeyMaxLen];
  char device_key[kDeviceKeyMaxLen];
  char display_name[kDisplayNameMaxLen];
} __attribute__((packed));

constexpr size_t kDeviceDescriptorImageBytes =
    kDeviceDescriptorImageOffset + sizeof(DeviceDescriptor);

const DeviceDescriptor& currentDeviceDescriptor();
const char* currentProjectKey();
const char* currentDeviceKey();
const char* currentDisplayName();
bool matchesCurrentDeviceKey(const char* incoming_device_key);
const char* expectedDeviceDisplayName();
bool parseDeviceDescriptorFromImage(const uint8_t* image_data, size_t image_len, DeviceDescriptor& out);

}  // namespace firmware_meta
