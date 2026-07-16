#pragma once

#include <Arduino.h>
#include <IPAddress.h>
#include <atomic>

// Shared network view for every HomeTiles device.
//
// Application code must use this facade for connectivity, addressing and
// transport-sensitive workarounds. Backend-specific operations such as WiFi
// scans, SoftAP configuration or PHY setup remain inside their backend.
enum class NetworkTransportKind : uint8_t {
  None = 0,
  Wifi,
  UsbEthernet,
  NativeEthernet,
};

class NetworkTransportManager {
public:
  void begin();
  void update();

  bool isConnected() const;
  bool isWifiConnected() const;
  bool isWifiDriverActive() const { return wifi_driver_active_.load(); }
  bool isUsbEthernetLinkUp() const;
  bool isNativeEthernetLinkUp() const;
  bool isUsbEthernetConnected() const;
  bool isNativeEthernetConnected() const;
  bool isSdioWifiActive() const;
  void setWifiDriverActive(bool active);

  NetworkTransportKind activeKind() const { return active_kind_.load(); }
  const char* activeName() const;
  uint32_t generation() const { return generation_.load(); }

  IPAddress localIP() const;
  IPAddress gatewayIP() const;
  IPAddress dnsIP(uint8_t index = 0) const;

  // Backend hooks. A backend reports link/IP state here; the facade selects
  // the highest-priority usable transport and exposes it to the application.
  void setUsbEthernetState(bool link_up, bool has_ip, const IPAddress& local_ip,
                           const IPAddress& gateway, const IPAddress& dns);
  void setNativeEthernetState(bool link_up, bool has_ip,
                              const IPAddress& local_ip,
                              const IPAddress& gateway,
                              const IPAddress& dns);

private:
  struct EthernetState {
    std::atomic<bool> link_up{false};
    std::atomic<bool> has_ip{false};
    std::atomic<uint32_t> local_ip{0};
    std::atomic<uint32_t> gateway{0};
    std::atomic<uint32_t> dns{0};
  };

  void refreshActiveTransport();
  const EthernetState* activeEthernetState() const;

  bool begun_ = false;
  uint32_t wifi_poll_at_ = 0;
  uint32_t usb_poll_at_ = 0;
  uint32_t native_poll_at_ = 0;
  std::atomic<NetworkTransportKind> active_kind_{NetworkTransportKind::None};
  std::atomic<uint32_t> generation_{0};
  std::atomic<bool> wifi_driver_active_{false};
  std::atomic<bool> wifi_connected_{false};
  std::atomic<uint32_t> wifi_local_ip_{0};
  std::atomic<uint32_t> wifi_gateway_{0};
  std::atomic<uint32_t> wifi_dns_{0};
  EthernetState usb_;
  EthernetState native_;
};

extern NetworkTransportManager networkTransport;
