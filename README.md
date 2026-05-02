# ⚡ Stromleser ESPHome

A modular, high-precision ESPHome firmware for reading modern German smart meters (mME) via SML, featuring intelligent data scaling and an automated IR "Safecracker" PIN entry system.

Built specifically for the ESP32-C3, this project brings your smart meter natively into Home Assistant without the need for custom Tasmota builds.

## ✨ Key Features

- **🧩 Modular Architecture:** Hardware interactions, meter-specific parsers, and automation scripts are isolated into separate packages. Easily adapt it to your specific meter model!
- **🧠 Intelligent Data Scaling:** Prevents the dreaded "Energy Dashboard Spike." The firmware detects if your meter's `INFO` mode is locked or unlocked, automatically applying the correct `10^-4` multiplier to your `Total Consumed` and `Total Delivered` values.
- **🤖 The "Safecracker" Auto-Unlock:** Forget standing in a dark basement flashing a phone light at your meter. Enter your 4-digit PIN in Home Assistant, hit run, and the ESP32 will automatically pulse the IR diode with the exact timings required by the Landis+Gyr flowchart. _(Experimental)_
- **🔦 Virtual Flashlight:** Provides manual Home Assistant buttons for short (0.5s) and long (5s) IR pulses to navigate meter menus from your couch.
- **🆔 Smart Server ID Parser:** A custom C++ lambda that decodes the raw compressed hexadecimal `96.1.0` OBIS code into a perfectly formatted, human-readable string (e.g., `1 LGZ 00 1234 5678`).

## 🛠 Hardware Requirements

- **Board:** ESP32-C3 (e.g., DevKitM-1)
- **Sensor:** Standard TTL IR Optical Reading Head (Stromleser)
- **Meter:** Tested on the **Landis+Gyr E320** (but easily adaptable via packages)

## 🚀 Installation & Usage

You do not need to download or copy any files manually! You can pull the logic directly from this repository into your ESPHome configuration over the web.

1. Create a new device in ESPHome and open its configuration yaml.
2. Edit your file to include your specific WiFi credentials, OTA passwords, and substitution names.
3. Add the `packages` block to pull the modules directly from GitHub:

```yaml
substitutions:
  devicename: "stromleser"
  friendly_name: "Stromleser"

packages:
  # 1. The Meter Profile
  meter: github://arunoruto/stromleser-esphome/meters/landis_gyr_e320.yml@main

  # 2. Hardware Interaction (Required for IR pulses)
  interaction: github://arunoruto/stromleser-esphome/utils/interaction.yml@main

  # 3. Auto-Unlock Script (Requires interaction.yml)
  pin_script: github://arunoruto/stromleser-esphome/utils/pin.yml@main
```

4. Flash the device! ESPHome will automatically download the packages during the build process.

## 🔓 How to use the Auto-Unlock Script

_Note: This feature is currently experimental and timed specifically for the Landis+Gyr E320._

1. In Home Assistant, locate the **Meter PIN** number box.
2. Enter your 4-digit PIN (provided by your grid operator).
3. Press the **Run Auto-Unlock** button.
4. The ESP32 will wake up the meter, wait for the display tests to finish, enter the PIN digit by digit, and automatically wait the required 3.5 seconds for the meter to advance to the next screen.

## 🤝 Contributing

Want to add support for an Iskra, Apator, or EMH meter? Submit a PR adding a new `.yml` file to the `meters/` directory!
