# Device Network Configuration

GemmaBuddy firmware does not store Wi-Fi credentials in source code. Credentials live in NVS through the upstream `esp-wifi-connect` component.

Default provisioning for this build:

- Method: Hotspot provisioning.
- AP prefix: `GemmaBuddy`.
- Trigger: first boot with no saved Wi-Fi, or a Wi-Fi connection timeout.
- Storage: NVS `wifi` namespace.

Flow:

1. Flash the firmware.
2. On first boot, wait for the device to enter Wi-Fi config mode.
3. Connect a phone or laptop to the `GemmaBuddy...` access point shown by the device.
4. Open the URL shown by the device and submit the SSID/password for the same Wi-Fi network as this laptop.
5. The device stores those credentials and then calls `CONFIG_OTA_URL`.

For the current laptop LAN setup:

```text
Laptop IP: 192.168.1.11
OTA URL:   http://192.168.1.11:8000/xiaozhi/ota/
WS URL:    ws://192.168.1.11:8000/xiaozhi/ws
```

If the laptop IP changes, update `sdkconfig.defaults.esp32s3`, rebuild, and flash again, or configure a stable DHCP reservation.
