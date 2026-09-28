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

- `SENDING 14`: the number of characters not yet fully sent. It includes the one on the air, and spaces count as characters.
- `WIFI`: the separator is a **tab**, because an SSID may contain spaces. An empty password (`WIFI cafe\t`) means an open network.
- `WIFI`, `MODE` and `APIKEY` are refused with `ERR mode` outside BLE mode.
- `MODE` stops any transmission, answers `OK` and restarts about 300 ms later.

### Errors

| response | reason |
|---|---|
| `ERR cmd` | unknown command |
| `ERR arg` | missing or malformed argument |
| `ERR range` | `WPM` outside 5–50 |
| `ERR mode` | command not allowed in the current mode |
| `ERR length` | line longer than 256 characters |
| `ERR busy` | the keyer event queue is full; retry |

## Asynchronous messages

| message | meaning |
|---|---|
| `DONE` | the queue has been sent completely |
| `STOPPED` | a transmission was interrupted by `STOP` or the button |
| `ERR char` | unsupported characters were skipped (sent once per `SEND`) |
| `ERR full` | the send queue (1024 characters) overflowed and the rest of the text was dropped |
| `ERR watchdog` | the output was keyed for more than 5 s continuously, so the keyer opened it and dropped the queue |
| `WPM <n>` | speed changed by other means than the `WPM` command (reserved for the paddle phase) |

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
- Asynchronous messages are not delivered over HTTP. Poll `STATUS` instead.

## cwdaemon (CWD mode)

- UDP port 6789, mDNS `keyer-XXXX.local`.
- A plain datagram is text to send. A trailing `\r\n` and the echo request `^` are ignored.
- Escape requests:
  - `ESC 0` (reset) and `ESC 4` (abort) stop the transmission,
  - `ESC 2<n>` sets the speed,
  - other escape requests are ignored.

## Versioning

The protocol version rises when a command changes incompatibly. New commands or messages don't raise it, so clients should ignore unknown asynchronous messages.
