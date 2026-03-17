# Commands

- Flash OTA: `cd firmware && pio run -e ota -t upload`
- Flash USB: `cd firmware && pio run -t upload`
- PlatformIO project root is `firmware/`, not repo root
- Start inkless-server: `cd inkless-server && /usr/bin/python3 -m uvicorn server:app --host 0.0.0.0 --port 8100`
- inkless-server config: `inkless-server/.env` (API key, printer URL)
- ESP32 points to inkless-server via `INKLESS_SERVER_URL` in `firmware/include/secrets.h`

## Prerequisites

- Install PlatformIO CLI or PlatformIO in VS Code so `pio` is available in your shell.
- Install Python if your PlatformIO setup does not already provide it.
- Ensure the USB serial driver for your ESP32 board is installed on your machine.
- Use the `firmware/` directory as the PlatformIO project root for all build and upload commands.
- For USB flashing, connect the board over USB and confirm the serial device is visible before uploading.
- For OTA flashing, make sure the board is already running firmware with OTA support and is reachable on the same network as your computer.

## Explanation

- `cd firmware && pio run -e ota -t upload`
	- Builds the firmware using the `ota` environment from `platformio.ini`.
	- Uploads over the network instead of over a USB cable.
	- Use this when the device is already deployed, powered on, and reachable via Wi‑Fi.

- `cd firmware && pio run -t upload`
	- Builds the default PlatformIO environment and uploads over the connected serial/USB port.
	- Use this for first-time flashing, recovery, or whenever OTA is unavailable.

In practice: USB is the safest path for initial setup and debugging. OTA is the convenient path once the device is already configured and online.

## Hardware/Setup

- Required hardware:
	- ESP32-based controller used by this receipt printer firmware.
	- Thermal receipt printer hardware wired to the configured UART pins.
	- USB cable for wired flashing and serial debugging.
	- Stable power for the board and printer.

- OTA setup requirements:
	- The device must already have a working firmware build that includes OTA upload support.
	- Wi‑Fi credentials in `firmware/include/secrets.h` must be valid.
	- Your computer and the device must be on the same local network unless your OTA setup explicitly supports routing across networks.
	- The device should be powered on, connected to Wi‑Fi, and discoverable/reachable by its configured network address.

## Troubleshooting

- Serial port problems:
	- Reconnect the USB cable and verify the board appears as a serial device.
	- Close any serial monitor or other tool that may already be using the port.
	- Reinstall or update the USB-to-serial driver if the port never appears.
	- If upload fails repeatedly, try a different USB cable or port.

- OTA upload problems:
	- Verify the board is on the same network and still reachable.
	- Check firewall settings if network uploads time out or cannot connect.
	- Confirm the OTA target configuration in `platformio.ini` matches the board's current IP/hostname.
	- If OTA is broken, fall back to USB flashing to restore a known-good build.

- PlatformIO build problems:
	- Run the command from `firmware/`, not the repository root.
	- Confirm PlatformIO is installed and `pio` is available on your PATH.
	- Rebuild after checking `platformio.ini`, board configuration, and secrets/config header changes.
	- If dependency or cache issues appear, clean the build artifacts and retry the upload.
