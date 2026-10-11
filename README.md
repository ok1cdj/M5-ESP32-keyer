# M5-ESP32-keyer v2

CW keyer for the **M5Stack Atom** family by OK1CDJ. Send it text, and it keys the radio on its own through an optocoupler or a MOSFET. The text comes from one of three modes: **BLE**, **HTTP** or **cwdaemon**. With an Icom radio it also reads the frequency and mode over **CI-V**.

**Install and configure from the browser:** https://keyer.ok1cdj.com

**Build it** (parts with shop links, schematics, in English and Czech): https://keyer.ok1cdj.com/build.html

- Flashing needs Chrome or Edge on a desktop (Web Serial).
- Configuration over Bluetooth also works in Chrome on Android. On Linux, enable `chrome://flags/#enable-web-bluetooth` and restart Chrome.

Protocol for clients (kQSO, nRF Connect, scripts): [PROTOCOL.md](PROTOCOL.md)

## Boards

| board | chip | key output | CI-V | paddles (reserved) | battery ADC | display |
|---|---|---|---|---|---|---|
| M5 **AtomS3 Lite** | ESP32-S3 | G2 (Grove) | G1 (Grove) | G6 dot, G7 dash | G8 | – |
| M5 **Atom Lite** | ESP32-PICO-D4 | G26 (Grove) | G32 (Grove) | G19 dot, G23 dash | G33 | – |
| M5 **AtomS3** | ESP32-S3 | G2 (Grove) | G1 (Grove) | G6 dot, G7 dash | G8 | 128×128 |

- **Same wiring on every board.** The functions sit on the same positions on all boards:
  - **Grove** (to the radio): `GND · 5V · CIV · KEY`. KEY is the SDA pin (Atom Lite G26, AtomS3 G2), CIV the SCL pin (Atom Lite G32, AtomS3 G1).
  - **Side header** (paddles, battery): `DOT · DASH · BAT` (Atom Lite `G19 G23 G33`, AtomS3 `G6 G7 G8`).
- **Safe pins only.** None of them is a strapping pin, so a keyed radio or a pressed paddle can't change the boot mode. None is input-only, so the paddles get internal pull-ups.
- **Battery.** All boards fit the **Atomic Battery Base** (200 mAh), which reads the battery voltage through a 1:2 divider on the BAT pin.

## Wiring

The KEY pin drives the radio's key input through a 3.5 mm jack: tip = key, sleeve = GND. Use one of two variants.

**A: optocoupler (isolated).** The keyer and the radio share no ground. Use it when the radio's ground could be at a different potential, for example a radio on a separate supply or with a computer on the same cable.

```
KEY pin ──[ 330 Ω ]──┬──► PC817 anode (1)          PC817 collector (4) ──► jack tip   (key)
                     │    PC817 cathode (2) ─ GND   PC817 emitter   (3) ──► jack sleeve (GND)
                  [10 kΩ]
                     │
                    GND
```

- GPIO → 330 Ω → optocoupler LED (about 6 mA). The transistor side switches the key input.
- **10 kΩ pull-down** on the KEY pin (recommended). The firmware opens the output before anything else starts, but during a reset the pin is briefly floating, and the resistor holds it low regardless of the firmware.

**B: N-MOSFET (shared ground).** Fewer parts, and almost no current from the GPIO. The Grove GND is connected to the radio's ground through the jack sleeve.

```
KEY pin ──[ 100 Ω ]──┬──► gate                 drain  ──► jack tip    (key)
                     │                         source ──► jack sleeve (GND) ── Grove GND
                  [10 kΩ]
                     │
                    GND (source)
```

- A logic-level N-MOSFET: **AO3400** (fully on at 3.3 V) or **BSS138**. A through-hole **2N7000** also keyed the radio reliably in a test, although its threshold of up to 3 V leaves little margin at 3.3 V. 100 Ω in the gate, 10 kΩ gate to source.
- The 10 kΩ gate-source resistor is also the pull-down: the MOSFET stays off while the pin is floating during a reset.
- Mind the polarity: the drain goes to the tip. Swapped, the body diode keys the radio permanently.

Both variants have been tested on a modern transceiver whose key input has a few volts and milliamps. For an old radio with a high voltage on the key line (tube rigs, negative grid-block keying) use neither variant directly; add a relay or a suitable high-voltage keying circuit.

**CI-V (Icom, frequency and mode).** One more 3.5 mm jack to the radio's REMOTE (CI-V) socket: tip = CI-V, sleeve = GND. The bus is one open-collector wire pulled up to 5 V inside the radio, so a BSS138 shifts it to 3.3 V in both directions. Everything comes from the Grove cable, which has 5 V but no 3.3 V. It needs the shared ground of variant B.

```
Grove 5V ──[ 10 kΩ ]──┬──[ 27 kΩ ]── GND
                      │
                    gate (about 3.6 V)
                      │
CIV pin ──────── source  BSS138 / 2N7000  drain ──► REMOTE tip    (CI-V)
                                           REMOTE sleeve (GND) ── Grove GND
```

- **Gate at about 3.6 V from a divider.** The gate takes no current, so two resistors replace a 3.3 V supply. 10 k / 27 k gives 3.65 V. The gate voltage must stay below about 4 V: the MOSFET can then lift the CIV pin to at most Vg − Vth(min) = 4 − 0.8 V, under 3.3 V.
- **BSS138 or 2N7000.** The 2N7000 has a threshold of up to 3 V, so it needs the 3.6 V; the BSS138 would also work with a gate as low as 2.7 V.
- **No pull-up resistor on the keyer side.** The firmware turns on the ESP32's internal pull-up on the CIV pin, which is plenty at 19200 Bd. An external pull-up to 3.3 V would work too, but the Grove has no 3.3 V and the divider is too weak to feed one.
- When the radio pulls the line low, the BSS138's body diode and then the channel pull the CIV pin low. When the keyer pulls the pin low, the channel conducts and pulls the radio's line low. Idle, the CIV pin sits at 3.3 V and the radio's line at 5 V. The ESP32 never sees 5 V.
- With the keyer switched off the divider has no voltage, the BSS138 stays off and the radio's bus is left alone.
- **A plain diode does not work here:** it passes a low level in one direction only, so the keyer could either send or receive, not both.
- Before connecting, measure the open-circuit voltage between the REMOTE tip and the sleeve: about 5 V is expected. If the radio's pull-up turns out to be missing (0 V), add 10 kΩ from the tip to the Grove 5V.
- In the radio: **CI-V Transceive ON** (the keyer then hears every change at once) and any rate from 1200 to 19200 Bd, or Auto. The keyer finds the rate and the address by itself and remembers the rig. A rig with a changed address (an IC-706MkII here was at 4D) is found the first time when you turn the VFO knob. With Transceive off it polls once a second; that works for the default addresses (IC-706 family, IC-910, IC-7300) and the remembered rig, otherwise set them with `CIV <baud> <addr>` over BLE.
- The frequency appears on the AtomS3 display, as `FREQ <Hz> <mode>` messages over BLE after `FREQ ON`, on the HTTP page and through the `FREQ` command ([PROTOCOL.md](PROTOCOL.md)).
- Another controller on the same bus (a logging program through a CI-V interface) is fine; the keyer only asks when the radio is silent.

**Paddles (next phase).** A separate 3.5 mm jack: tip = dot, ring = dash, sleeve = GND. The contacts switch to GND and are idle high. The firmware doesn't read them yet, but plan the second jack in your enclosure.

## Modes

| mode | LED | transport | Wi-Fi | intended for |
|---|---|---|---|---|
| **BLE** (default) | blue | Nordic UART Service | off | kQSO, phone, battery |
| **HTTP** | green | HTTP API, `/sendmorse` as in v1 | client | compatibility with v1 |
| **CWD** | yellow | cwdaemon UDP 6789 | client | logging programs (e.g. Tucnak) |

Exactly one transport runs at a time.

**Switching modes.** Hold the button while powering on. The LED cycles through the mode colours about once a second. Release the button when it shows the mode you want, and the keyer stores it and restarts. On the AtomS3 the display shows the mode name instead.

**Wi-Fi setup** for HTTP and CWD happens over BLE. There is no access point and no captive portal.

The easiest way is the *Configure over Bluetooth* section of https://keyer.ok1cdj.com: connect, fill in SSID, password and optionally an API key, then choose the mode. The same section shows the radio's frequency, sets up CI-V and rotates the AtomS3 display.

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

The web page sends text, sets the speed, shows the radio's frequency and, on the AtomS3, has the display rotation under Settings.

`apikey` is required only after you have set one with `APIKEY`.

## Controls

The same on all boards. On the AtomS3, pressing the screen is the button.

- **Short press = STOP.** The output opens immediately, even in the middle of a character, and the queue is dropped.
- **After start-up** the LED shows the mode colour for about 2 s, then turns off to save power.
- **Client connects or disconnects:** a short blink.
- **Button press:** a short blink, so you can see the press was registered.
- **AtomS3 display:**
  - shows the mode, IP address, WPM, battery, the radio's frequency and mode (CI-V) and, while sending, the next 10 characters of the text, the one being sent in red (prosigns without brackets),
  - can be rotated in 90° steps: on the HTTP page under Settings, or with `ROTATE <deg>` over BLE or HTTP; the setting is stored,
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
- The keyer core (`lib/keyer_core`), the protocol parser (`lib/protocol`) and the CI-V parser with its link logic (`lib/civ`) are plain C++ without Arduino, so they are tested on the PC.

### Hardware checklist

Tested so far on AtomS3 Lite, AtomS3 and Atom Lite. Real keying of a radio through the Grove port, with both the PC817 and the N-MOSFET, was checked on the AtomS3 (over BLE) and the Atom Lite (over cwdaemon); the AtomS3 Lite has the same pin as the AtomS3 but hasn't keyed a radio yet. CI-V has been tested on the AtomS3 only (with an IC-706MkII); the other boards build the same code with different pins.

- [x] The radio keys correctly at 20 WPM (PARIS).
- [x] The output stays open after power-on and after a reset, including a reset in the middle of a message.
- [x] `STOP` over BLE, the button and `ESC 4` in the middle of a message open the output immediately.
- [x] Over BLE, `SEND CQ CQ DE OK1CDJ` is sent correctly (character and word gaps).
- [x] A command longer than 20 bytes is assembled correctly.
- [x] The button (on the AtomS3, the screen) in the middle of a message stops keying immediately.
- [x] Disconnecting the phone in the middle of a message stops keying immediately.
- [x] Watchdog: a test build `PLATFORMIO_BUILD_FLAGS=-DWATCHDOG_TEST=1 pio run -e atoms3-lite -t upload` holds the first element down. After 5 s the output must open and `ERR watchdog` must arrive.
- [x] Switching modes with the button at power-on works (LED colours on the Lite boards, mode name on the AtomS3).
- [x] `WIFI` over BLE stores the credentials, and HTTP mode connects after the restart.
- [x] `/sendmorse` works as in v1: `speed` applies immediately and `message` replaces what is being sent.
- [x] Battery reading on the AtomS3: calibrated against the charger's 4.20 V end of charge (it read 4045 mV before, 4200 mV after, factor 1.038). The keyer logs `[bat] <mV> <%>` on the serial port.
- [x] Battery reading on the Atom Lite: reads 4222–4228 mV at the 4.20 V end of charge, within the charger's ±1 %, so it needs no correction.
- [x] cwdaemon over UDP 6789: text, `ESC 2` (speed) and `ESC 4` (abort). Timing matches PARIS.
- [x] cwdaemon from Tucnak.
- [x] CI-V: open-circuit voltage on the REMOTE tip measured before connecting (IC-706MkII: 3.7 V).
- [x] CI-V on the IC-706MkII (address 4D, direct through 1 kΩ): rate and address found, also with the radio's rate on Auto; the remembered rig is found after a restart without Transceive.
- [x] CI-V tunnel: `CIVTX` retunes the radio, five frequency sets in a row all acknowledged.
- [x] CI-V: the frequency follows the VFO knob on the AtomS3 display and over BLE (`FREQ ON`); about 10 s after the cable is pulled the display line goes away and `FREQ NONE` arrives, and the rig is found again when it is plugged back.
- [x] Keying at 20 WPM with CI-V running on the IC-706MkII (2N7000 on KEY): PARIS ×3 took 8.78 s (8.58 s plus BLE latency) and keyed the radio cleanly; `STOP` mid-message answered `STOPPED` within 63 ms and the radio stopped at once.
- [x] Display rotation on the AtomS3: set from the HTTP page, applied at once and kept after a restart.

## License

GPL-3.0-or-later. © Ondrej Kolonicny, OK1CDJ.

[![https://www.buymeacoffee.com/ok1cdj](https://img.shields.io/badge/Donate-Buy%20me%20a%20coffee-orange?style=for-the-badge)](https://www.buymeacoffee.com/ok1cdj)
