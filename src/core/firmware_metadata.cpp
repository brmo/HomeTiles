#include "src/core/firmware_metadata.h"

#include <string.h>

#if defined(DEVICE_M5STACKS_TAB5)
#define FW_META_DEVICE_KEY "m5stacks_tab5"
#define FW_META_DISPLAY_NAME "M5Stacks Tab5"
#elif defined(DEVICE_WAVESHARE_4B)
#define FW_META_DEVICE_KEY "waveshare_4b"
#define FW_META_DISPLAY_NAME "Waveshare B4"
#else
#define FW_META_DEVICE_KEY "unknown"
#define FW_META_DISPLAY_NAME "Unknown"
#endif

#define FW_META_PROJECT_KEY "esp32_p4_homeassistant_display"

namespace firmware_meta {
namespace {

constexpr uint8_t kEspImageHeaderMagic = 0xE9;
constexpr uint32_t kEspAppDescMagicWord = 0xABCD5432u;
constexpr size_t kEspImageHeaderSize = 24;
constexpr size_t kEspImageSegmentHeaderSize = 8;
constexpr size_t kEspAppDescOffset = kEspImageHeaderSize + kEspImageSegmentHeaderSize;
constexpr size_t kEspAppDescSize = 256;

inline const DeviceDescriptor kCurrentDeviceDescriptor
    __attribute__((used, section(".rodata_custom_desc"))) = {
        kDeviceDescriptorMagic,
        FW_META_PROJECT_KEY,
        FW_META_DEVICE_KEY,
        FW_META_DISPLAY_NAME,
};

uint32_t readU32LE(const uint8_t* data) {
  return static_cast<uint32_t>(data[0]) |
         (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) |
         (static_cast<uint32_t>(data[3]) << 24);
}

}  // namespace

const DeviceDescriptor& currentDeviceDescriptor() {
  return kCurrentDeviceDescriptor;
}

const char* currentProjectKey() {
  return kCurrentDeviceDescriptor.project_key;
}

const char* currentDeviceKey() {
  return kCurrentDeviceDescriptor.device_key;
}

const char* currentDisplayName() {
  return kCurrentDeviceDescriptor.display_name;
}

bool parseDeviceDescriptorFromImage(const uint8_t* image_data, size_t image_len, DeviceDescriptor& out) {
  if (!image_data || image_len < kDeviceDescriptorImageBytes) {
    return false;
  }
  if (image_data[0] != kEspImageHeaderMagic) {
    return false;
  }

  const uint8_t* app_desc = image_data + kEspAppDescOffset;
  if (readU32LE(app_desc) != kEspAppDescMagicWord) {
    return false;
  }

  memcpy(&out, image_data + kDeviceDescriptorImageOffset, sizeof(DeviceDescriptor));
  if (out.magic_word != kDeviceDescriptorMagic) {
    return false;
  }

  out.device_key[kDeviceKeyMaxLen - 1] = '\0';
  out.display_name[kDisplayNameMaxLen - 1] = '\0';
  return true;
}

}  // namespace firmware_meta
