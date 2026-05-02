# 🤖 Agent Instructions: Stromleser ESPHome

Welcome! You are an expert AI developer assisting with the **Stromleser ESPHome** project. This is a highly modular, high-precision ESPHome firmware for reading modern German smart meters (mME) via SML (Smart Message Language) using an ESP32-C3 and TTL optical reading heads.

Please read these instructions carefully to understand the workflow, architecture, and coding standards of this repository.

## ⚠️ THE GOLDEN RULE: The Two-Phase Workflow

Do **NOT** directly edit the production modular files (`stromleser.yml`, `meters/*`, `utils/*`) when developing a new feature or debugging. You must follow this strict two-phase workflow:

### Phase 1: Experimentation (`dev.yml`)

When the user asks you to write new code, test a theory, or build a feature:

1. Write **all** the code in a single, monolithic file named `dev.yml` located in the root directory.
2. This file acts as a sandbox. It allows the user to easily copy/paste the entire configuration directly into their Home Assistant ESPHome dashboard to flash and test immediately.
3. Assume `dev.yml` contains all necessary substitutions and Wi-Fi boilerplate.

### Phase 2: Refactoring (Explicit Trigger Required)

**ONLY** when the user explicitly states "The feature is complete" or "Refactor this into the project":

1. Take the working code from `dev.yml`.
2. Deconstruct it and place the relevant blocks into the correct modular files (see Architecture below).
3. Ensure the base `stromleser.yml` includes the new packages if necessary.
4. Clean up `dev.yml` to be a blank slate for the next feature.

---

## 🏗️ Project Architecture

When in Phase 2 (Refactoring), you must respect this strict modular structure:

- `stromleser.yml` -> The base file. Contains board config (ESP32-C3), Wi-Fi fallback, and the `packages:` block. No actual meter logic goes here.
- `meters/` -> Contains meter-specific SML parsing profiles (e.g., `landis_gyr_e320.yml`, `iskra_mt681.yml`). These handle UART, SML components, and specific data scaling (nan-checks and 10^-4 multiplication logic).
- `utils/` -> Contains hardware tools and scripts.
  - `interaction.yml` -> Physical GPIO interactions (e.g., Virtual Flashlight for the IR TX diode).
  - `pin.yml` -> The automated "Safecracker" script that pulses the IR diode to enter meter PINs.
  - _(Future)_ `shelly_emulator.yml` -> Web server interception to fake a Shelly 3EM Pro JSON payload.
  - _(Future)_ `proxy.yml` -> UART repeater logic for MITM smart meter data via the TTL expansion ports.

---

## 💻 Tech Stack & Coding Standards

- **Framework:** ESPHome (YAML).
- **Hardware:** ESP32-C3.
- **Custom Logic:** C++ (Lambda functions).

### C++ Lambda Guidelines within ESPHome:

1.  **State Persistence:** Remember that ESPHome sensors retain their last valid state until updated or rebooted. Use timestamp checking (`millis()`) combined with global variables to determine data "freshness" (e.g., `id(last_power_update) > 0`).
2.  **Defensive Programming:** Always provide a safe fallback `return` at the end of a lambda to satisfy the C++ compiler.
3.  **Non-Blocking:** Never use `delay()` in a lambda unless it is explicitly inside a script sequence. Use asynchronous programming if necessary to prevent watchdog resets.
4.  **Logging:** Use `ESP_LOGI`, `ESP_LOGD`, etc., for custom C++ components so the user can debug via the ESPHome logs.

### YAML Guidelines:

1.  **Substitutions:** Use substitutions for names (e.g., `${devicename}`).
2.  **No Hardcoded Secrets:** Never output real Wi-Fi passwords or OTA passwords. Use dummy values or substitutions so the user's Captive Portal can handle the real setup.
3.  **Clean IDs:** Always assign an `id:` to sensors or outputs if they need to be referenced by a C++ lambda or a script.

---

## 🎯 Current Context / Upcoming Goals

- We are preparing to build a **Shelly 3EM Emulator** directly into the ESPHome web server to feed data to local solar inverters.
- We are preparing to build a **UART Proxy** that reads from the internal SML reader and echoes the bytes out to a TTL port to feed a Tibber Pulse in a "Man-in-the-Middle" setup.

When you are ready, say: _"I have read the Stromleser architecture guidelines. I am ready to work in `dev.yml`."_
