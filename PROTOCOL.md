# M5-ESP32-keyer protocol

**Protocol version 1** (reported by `VER`). Firmware 2.0.0 and later.

A plain-text line protocol for sending CW through the keyer. BLE uses it directly, and HTTP carries it on the `/cmd` endpoint. The cwdaemon mode keeps its own UDP protocol (see the end of this document).

## Framing

- One line is one command, terminated by `\n`. A trailing `\r` is ignored.
- Commands are case-insensitive. The text of `SEND` keeps its case until the keyer upper-cases it for sending.
- A line may be at most **256 characters**. A longer line is dropped whole and answered with `ERR length`.
- Every command gets exactly one response line. Asynchronous messages (see below) can arrive between responses.
- Responses and asynchronous messages are also lines terminated by `\n`.

## Commands

| command | meaning | response |
|---|---|---|
| `SEND <text>` | append text to the send queue | `OK` |
| `WPM <n>` | set speed, 5–50 WPM, stored in NVS | `OK` / `ERR range` |
| `STOP` | stop immediately, even mid-character, and drop the queue | `OK` |
| `STATUS` | current state | `IDLE WPM 22` / `SENDING 14 WPM 22` |
| `VER` | firmware and protocol version | `VER keyer 2.0.0 proto 1` |
| `WIFI <ssid>\t<password>` | store Wi-Fi credentials (BLE only) | `OK` |
| `MODE BLE\|HTTP\|CWD` | store the mode and restart (BLE only) | `OK`, then restart |
| `APIKEY <key>` | store the HTTP API key; empty = no authentication (BLE only) | `OK` |
| `FREQ` | radio frequency in Hz and mode, read over CI-V | `FREQ 14025000 CW` / `FREQ NONE` |
| `FREQ ON\|OFF` | start / stop the asynchronous `FREQ` messages for this connection (BLE only) | `OK` |
| `CIVTX <hex>` | send one CI-V frame to the radio (BLE only) | `OK` / `ERR norig` / `ERR busy` |
| `CIVRX ON\|OFF` | start / stop forwarding every frame from the radio (BLE only) | `OK` |
| `CIV` | CI-V setting and the detected rate and rig address | `CIV AUTO 19200 58` |
| `CIV OFF\|AUTO` | CI-V off, or automatic detection (BLE only) | `OK` |
| `CIV <baud> <addr>` | fixed rate and rig address in hex (BLE only) | `OK` / `ERR range` |

- `SENDING 14`: the number of characters not yet fully sent. It includes the one on the air, and spaces count as characters.
- `WIFI`: the separator is a **tab**, because an SSID may contain spaces. An empty password (`WIFI cafe\t`) means an open network.
- `WIFI`, `MODE` and `APIKEY` are refused with `ERR mode` outside BLE mode.
- `MODE` stops any transmission, answers `OK` and restarts about 300 ms later.
- `FREQ` and `CIV` need firmware 2.1.0 or later. `FREQ` modes: `LSB USB AM CW RTTY FM WFM CW-R RTTY-R`, `?` for any other. `FREQ NONE` means no rig has answered on the CI-V bus, or CI-V is off.
- `FREQ ON` is off again for every new BLE connection, so a client that doesn't know it never gets an unexpected line. With `FREQ ON`, take the frequency from the asynchronous messages only: the response to a bare `FREQ` looks the same and can't be told apart.
- `CIV` works in every mode; with arguments it is BLE only and takes effect at once, without a restart. The setting is stored in NVS.
  - `AUTO` (default) tries 19200, 9600, 4800 and 1200 Bd and takes the rig address from the first frame it hears. It also asks the rig found last time (stored), then the common default addresses (IC-706 family 58/4E/48, IC-910 60, IC-7300 94), so it finds a rig with Transceive off or quiet. A rig with a changed address is found the first time once it sends something (turn the VFO knob); after that it is remembered.
  - `<baud>` is 1200, 4800, 9600 or 19200, `<addr>` is 01–DF. Use it for a rig with a changed address and Transceive off.
  - The query answers `CIV AUTO - -` while no rig has been found yet, `CIV FIXED <baud> <addr>` with a fixed setting, and `CIV OFF` when CI-V is off.

### Errors

| response | reason |
|---|---|
| `ERR cmd` | unknown command |
| `ERR arg` | missing or malformed argument |
| `ERR range` | `WPM` outside 5–50, `CIV` rate or address not allowed |
| `ERR mode` | command not allowed in the current mode |
| `ERR length` | line longer than 256 characters |
| `ERR busy` | the keyer event queue (or the CI-V send queue) is full; retry |
| `ERR norig` | `CIVTX` while no radio has been found on the CI-V bus |

## Asynchronous messages

| message | meaning |
|---|---|
| `DONE` | the queue has been sent completely |
| `STOPPED` | a transmission was interrupted by `STOP` or the button |
| `ERR char` | unsupported characters were skipped (sent once per `SEND`) |
| `ERR full` | the send queue (1024 characters) overflowed and the rest of the text was dropped |
| `ERR watchdog` | the output was keyed for more than 5 s continuously, so the keyer opened it and dropped the queue |
| `WPM <n>` | speed changed by other means than the `WPM` command (reserved for the paddle phase) |
| `CIVRX <hex>` | only after `CIVRX ON`: a CI-V frame from the radio, whole (`FE FE … FD`) |
| `FREQ <Hz> <mode>` / `FREQ NONE` | only after `FREQ ON`: the current value at once, then every change of frequency or mode, and the rig found or lost. At most 5 per second; the last value always arrives. |

## CI-V tunnel

For a client that drives the radio itself (a logger tuning the radio to the band typed in it), the keyer passes raw CI-V frames through. It still finds the rate and the rig address on its own, so the client needs no settings.

- `CIVTX <hex>`: one whole frame `FE FE <to> <from> <cmd> [data] FD` as hex without spaces, at most 38 bytes. The keyer replaces `to` with the address of the rig it found and `from` with `E0`, so a client may keep the addresses it uses for another radio. The frame goes out when the bus is quiet; `OK` means queued, not answered. The answer comes as `CIVRX`.
- `CIVRX ON`: from now on every frame the radio sends goes to the client as `CIVRX <hex>`: Transceive broadcasts (to `00`), answers to the client's and the keyer's own queries (to `E0`), `FB` / `FA`. The keyer's own echo is not forwarded. Up to 24 frames are buffered, more are dropped.
- The rig address is the `from` of the received frames, or the last field of `CIV`.
- Both are off again for every new BLE connection.

Example, IC-706MkIIG at 58h, tune to 7.025 MHz:

```
→ CIVRX ON
← OK
→ CIVTX FEFEA4E0050050020700FD       (built for an IC-705 at A4)
← OK
← CIVRX FEFEE058FBFD                (the radio accepted it)
```

## Character set

`A–Z 0–9 / ? . , = + -` and the space. Lower case is converted to upper case. Tabs and line breaks count as spaces.

Prosigns are written in angle brackets, for example `<AR>`, `<SK>`, `<BT>` or `<KN>`. Their letters are sent without a character gap between them. Any other character is skipped and reported once with `ERR char`.

Timing is PARIS: dit = 1200 / WPM ms, dah = 3 dits, element gap 1, character gap 3, word gap 7 dits.

## BLE

- **Nordic UART Service**
  - service `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
  - RX `6E400002-…` (write, write without response): client → keyer
  - TX `6E400003-…` (notify): keyer → client
- Without a negotiated MTU a write carries only 20 bytes. The keyer assembles lines across writes until `\n`. Notifications are split to the negotiated MTU too, so the client must also assemble lines until `\n`.
- **Battery Service** `0x180F`, characteristic Battery Level `0x2A19`: percent, read and notify. It notifies when the value changes by 5 % or more, and it is measured every 30 s.
- The device name is `keyer-XXXX`, where XXXX are the last two bytes of the BT MAC address. It is in the scan response.
- **Disconnecting stops the transmission** and drops the queue.

## HTTP (HTTP mode)

The keyer announces itself over mDNS as `keyer-XXXX.local`.

- `GET /cmd?c=<line>` (or `POST /cmd` with form field `c`): runs one protocol command and returns the response line as `text/plain`. `WIFI`, `MODE` and `APIKEY` return `ERR mode`.
- `GET /sendmorse?message=<text>&speed=<wpm>` works as in v1:
  - `speed` applies immediately,
  - `message` replaces whatever is being sent,
  - the response is `OK`.
- `GET /?message=…&speed=…` behaves the same way and returns a simple web page.
- When an API key is set (`APIKEY` over BLE), every request needs `apikey=<key>`, otherwise the keyer returns `401 Unauthorized`.
- Asynchronous messages are not delivered over HTTP. Poll `STATUS` and `FREQ` instead.

## cwdaemon (CWD mode)

- UDP port 6789, mDNS `keyer-XXXX.local`.
- A plain datagram is text to send. A trailing `\r\n` and the echo request `^` are ignored.
- Escape requests:
  - `ESC 0` (reset) and `ESC 4` (abort) stop the transmission,
  - `ESC 2<n>` sets the speed,
  - other escape requests are ignored.

## Versioning

The protocol version rises when a command changes incompatibly. New commands or messages don't raise it, so clients should ignore unknown asynchronous messages.
