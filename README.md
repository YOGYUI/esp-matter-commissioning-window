# Open/Close Matter Commissioning Window (ESP32)
Example: Open & Close '**Matter(ConnectedHomeIP) Commissioning Window**' manually in code level.<br>
In this application, you can open/close commissioning window by clicking tact button wired to `GPIO0` of ESP32.
- single click: open window
- double click: close window

Clone source code
---
```shell
$ git clone https://github.com/YOGYUI/esp-matter-commissioning-window.git
```

Install SDK
---
You can install three sdk source (esp-idf, esp-matter, connectedhomeip) by calling automation script.
```shell
$ cd esp-matter-commissioning-window
$ source ./scripts/install_sdk.sh
```

Prepare SDK
---
```shell
$ cd esp-matter-commissioning-window
$ source ./scripts/prepare_sdk.sh
```

Build
---
Prerequisite: 'Prepare SDK'
```shell
$ idf.py build
```

Flash and monitor
---
Port name (`/dev/ttyUSB*` or `/devb/ttyACM*`) may be needed to modified following your environment.
```shell
$ idf.py -p /dev/ttyUSB0 flash monitor
```

Result (console log)
---
`GPIO0` wired tact button 
- single click (open window manually)
  ```
  I (85666) logger: [MatterCommWndWorkHandler] Try to open basic commissioning window (timeout: 300) [system.cpp:228]
  I (85666) chip[DIS]: Updating services using commissioning mode 1
  I (85676) chip[DIS]: CHIP minimal mDNS started advertising.
  I (85676) chip[DIS]: Advertise commission parameter vendorID=65521 productID=32768 discriminator=3840/15 cm=1
  I (85696) chip[DIS]: CHIP minimal mDNS configured as 'Commissionable node device'; instance name: FE2021427B97C1B6.
  I (85706) chip[DIS]: mDNS service published: _matterc._udp
  I (85706) logger: [CSystem::matter_event_callback] Commissioning window opened [system.cpp:197]
  ```
- double click (close window manually)
  ```
  I (49126) chip[SVR]: Closing pairing window
  I (49126) chip[DIS]: Updating services using commissioning mode 0
  I (49126) chip[DIS]: CHIP minimal mDNS started advertising.
  I (49136) logger: [CSystem::matter_event_callback] Commissioning window closed [system.cpp:200]
  ```

Reference
---
[Matter - Open/Close Commissioning Window in Code (SDK)](https://yogyui.tistory.com/entry/Matter-OpenClose-Commissioning-Window-in-Code-SDK)