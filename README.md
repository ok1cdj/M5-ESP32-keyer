# M5-ESP32-keyer v2

CW keyer for the **M5Stack Atom** family by OK1CDJ. Send it text, and it keys the radio on its own through an optocoupler. The text comes from one of three modes: **BLE**, **HTTP** or **cwdaemon**.

**Install and configure from the browser:** https://keyer.ok1cdj.com

- Flashing needs Chrome or Edge on a desktop (Web Serial).
- Configuration over Bluetooth also works in Chrome on Android. On Linux, enable `chrome://flags/#enable-web-bluetooth` and restart Chrome.

Protocol for clients (kQSO, nRF Connect, scripts): [PROTOCOL.md](PROTOCOL.md)

## Boards

| board | chip | key output | paddles (reserved) | battery ADC | display |
|---|---|---|---|---|---|
| M5 **AtomS3 Lite** | ESP32-S3 | G2 (Grove) | G6 dot, G7 dash | G8 | – |
| M5 **Atom Lite** | ESP32-PICO-D4 | G26 (Grove) | G19 dot, G23 dash | G33 | – |
| M5 **AtomS3** | ESP32-S3 | G2 (Grove) | G6 dot, G7 dash | G8 | 128×128 |

- **Same wiring on every board.** The functions sit on the same positions on all boards:
  - **Grove** (to the radio): `GND · 5V · CIV · KEY`. KEY is the SDA pin (Atom Lite G26, AtomS3 G2). The SCL pin (G32 / G1) is reserved for a planned CI-V interface.
  - **Side header** (paddles, battery): `DOT · DASH · BAT` (Atom Lite `G19 G23 G33`, AtomS3 `G6 G7 G8`).
- **Safe pins only.** None of them is a strapping pin, so a keyed radio or a pressed paddle can't change the boot mode. None is input-only, so the paddles get internal pull-ups.
- **Battery.** All boards fit the **Atomic Battery Base** (200 mAh), which reads the battery voltage through a 1:2 divider on the BAT pin.

## Wiring

```
KEY pin ──[ 330 Ω ]──┬──► PC817 anode (1)          PC817 collector (4) ──► jack tip   (key)
                     │    PC817 cathode (2) ─ GND   PC817 emitter   (3) ──► jack sleeve (GND)
                  [10 kΩ]
                     │
                    GND
```

- **Keying.** GPIO → 330 Ω → optocoupler LED (about 6 mA). The transistor side goes to the radio's key input through a 3.5 mm jack: tip = key, sleeve = GND.
- **10 kΩ pull-down** on the KEY pin (recommended). The firmware opens the output before anything else starts, but during a reset the pin is briefly floating, and the resistor holds it low regardless of the firmware.
- **Paddles (next phase).** A separate 3.5 mm jack: tip = dot, ring = dash, sleeve = GND. The contacts switch to GND and are idle high. The firmware doesn't read them yet, but plan the second jack in your enclosure.

## Modes

| mode | LED | transport | Wi-Fi | intended for |
|---|---|---|---|---|
| **BLE** (default) | blue | Nordic UART Service | off | kQSO, phone, battery |
| **HTTP** | green | HTTP API, `/sendmorse` as in v1 | client | compatibility with v1 |
| **CWD** | yellow | cwdaemon UDP 6789 | client | logging programs (e.g. Tucnak) |

Exactly one transport runs at a time.

**Switching modes.** Hold the button while powering on. The LED cycles through the mode colours about once a second. Release the button when it shows the mode you want, and the keyer stores it and restarts. On the AtomS3 the display shows the mode name instead.

**Wi-Fi setup** for HTTP and CWD happens over BLE. There is no access point and no captive portal.

The easiest way is the *Configure over Bluetooth* section of https://keyer.ok1cdj.com: connect, fill in SSID, password and optionally an API key, then choose the mode.

Manually, for example from nRF Connect (on iPhone and iPad this is the only way, because they have no Web Bluetooth):

1. In BLE mode, connect to `keyer-XXXX`.
2. Write `WIFI <ssid><TAB><password>\n` to the NUS RX characteristic.
3. Optionally, write `APIKEY <key>\n` to protect the HTTP API.
4. Write `MODE HTTP\n` (or `MODE CWD\n`).

The keyer then joins the network using DHCP and announces itself as **`keyer-XXXX.local`** (XXXX = the last two bytes of the MAC address). The AtomS3 also shows its IP address.

To get back to BLE, use the button at power-on.

### HTTP API (v1 compatible)

```
http://keyer-XXXX.local/sendmorse?apikey=KEY&message=CQ+DE+OK1CDJ&speed=25
http://keyer-XXXX.local/cmd?apikey=KEY&c=STATUS
http://keyer-XXXX.local/?apikey=KEY         simple web page
```

`apikey` is required only after you have set one with `APIKEY`.

## Controls

The same on all boards. On the AtomS3, pressing the screen is the button.

- **Short press = STOP.** The output opens immediately, even in the middle of a character, and the queue is dropped.
- **After start-up** the LED shows the mode colour for about 2 s, then turns off to save power.
- **Client connects or disconnects:** a short blink.
- **Button press:** a short blink, so you can see the press was registered.
- **AtomS3 display:**
  - shows the mode, IP address, WPM, battery and, while sending, the next 10 characters of the text, the one being sent in red (prosigns without brackets),
  - after 10 s of inactivity the backlight turns off and the panel goes to sleep; it stays on while sending,
  - the display wakes up when a client connects, when sending starts or ends, and on STOP.

## Safety

These rules are enforced in the keyer core, so they hold in every mode:

1. **The output is open after power-on and after any reset.** A static constructor drives the pin low before `setup()`, and M5Unified's board autodetection is disabled so it can't toggle the header pins.
2. **STOP is immediate**, from the button or the command.
3. **Keying watchdog.** If the output stays keyed for more than 5 s continuously, the keyer opens it and reports `ERR watchdog`. If the keyer task itself hangs, the task watchdog resets the chip, which opens the output (rule 1).
4. **A BLE disconnect means STOP.** The queue is dropped and the output opened.

## Power consumption

Goal: **2 hours** of activation on the 200 mAh Atomic Battery Base in BLE mode. The double conversion (3.7 V → 5 V boost → 3.3 V LDO) leaves only about 130 mAh usable.

BLE mode measures to save power:

- Wi-Fi is never initialised,
- the CPU runs at 80 MHz,
- BLE modem sleep is on. ESP32 has it in the stock libraries. The S3 builds enable it through `custom_sdkconfig`, which rebuilds the framework, so the first S3 build takes a couple of minutes longer,
- the advertising interval is about 1 s,
- the LED stays dark.

Measured with a USB meter at 5 V, without the battery base. 30 mA is about 4 h on the ~130 mAh the base delivers at 5 V:

| board | advertising | connected, idle | sending |
|---|---|---|---|
| Atom Lite | ~40 mA | ~40 mA | ~50 mA |
| AtomS3 Lite | ~30 mA | ~30 mA | ~30 mA |
| AtomS3, backlight off | ~40 mA | ~40 mA | ~40 mA |
| AtomS3, backlight on | ~70 mA | ~70 mA | ~70 mA |

Atom Lite draws about 10 mA more than AtomS3 Lite, which fits the FTDI USB-serial chip it has on board. AtomS3 Lite was measured with a slow USB meter: the average is about 30 mA in all three states, with short peaks up to about 50 mA from the radio at BLE events. "Sending" was measured without an optocoupler, which adds about 6 mA while keyed. The AtomS3 backlight is on only for 10 s after a change, so it runs at about 40 mA most of the time, which is about 3 h on the battery base.

## Building

Requires PlatformIO Core 6.2 or newer (`pipx install platformio`), because the pioarduino platform 55.03.312-1 depends on it.

```
pio test -e native          # host tests of the keyer core and protocol
pio run -e atoms3-lite      # or atom-lite, atoms3
pio run -e atoms3-lite -t upload
```

- The only differences between boards are the build flags in `platformio.ini` (pins, board) and the status module: `ui_led.cpp` or `ui_display.cpp`.
- The keyer core (`lib/keyer_core`) and the protocol parser (`lib/protocol`) are plain C++ without Arduino, so they are tested on the PC.

### Hardware checklist

Tested so far on AtomS3 Lite, AtomS3 and Atom Lite. The ticked items were checked through the protocol; the keying output on the jack hasn't been measured yet.

- [ ] Measure the jack after power-on and after a reset: the output must be open.
- [x] Over BLE, `SEND CQ CQ DE OK1CDJ` is sent correctly (character and word gaps).
- [x] A command longer than 20 bytes is assembled correctly.
- [x] The button (on the AtomS3, the screen) in the middle of a message stops keying immediately.
- [x] Disconnecting the phone in the middle of a message stops keying immediately.
- [x] Watchdog: a test build `PLATFORMIO_BUILD_FLAGS=-DWATCHDOG_TEST=1 pio run -e atoms3-lite -t upload` holds the first element down. After 5 s the output must open and `ERR watchdog` must arrive.
- [x] Switching modes with the button at power-on works (LED colours on the Lite boards, mode name on the AtomS3).
- [x] `WIFI` over BLE stores the credentials, and HTTP mode connects after the restart.
- [x] `/sendmorse` works as in v1: `speed` applies immediately and `message` replaces what is being sent.
- [ ] Battery Service percentage matches the voltage measured with a multimeter (the keyer logs `[bat] <mV> <%>` on the serial port).
- [x] cwdaemon over UDP 6789: text, `ESC 2` (speed) and `ESC 4` (abort). Timing matches PARIS.
- [ ] cwdaemon from Tucnak.

## License

GPL-3.0-or-later. © Ondrej Kolonicny, OK1CDJ.

[![https://www.buymeacoffee.com/ok1cdj](https://img.shields.io/badge/Donate-Buy%20me%20a%20coffee-orange?style=for-the-badge)](https://www.buymeacoffee.com/ok1cdj)
