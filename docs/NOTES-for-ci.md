# How the install page ships

The install page (`index.html` + two manifests) flashes the keyer over Web Serial using [ESP Web Tools](https://esphome.github.io/esp-web-tools/). Everything on the page is static except the firmware images and the version number, which CI produces. The pipeline is taken over from ok1cdj/M5-ESP32-BT-MIDI.

## Workflows

**`ci.yml`** runs on every push and pull request:

- runs `pio test -e native` (host tests of the keyer core and the protocol),
- builds `atom-lite`, `atoms3-lite` and `atoms3`,
- keeps the factory images as workflow artifacts.

**`release.yml`** runs only on a pushed tag `v*`:

1. checks that the tag matches `FW_VERSION` in `src/version.h`;
2. runs the host tests and builds all three boards;
3. attaches `keyer-atom-lite.factory.bin`, `keyer-atoms3-lite.factory.bin` and `keyer-atoms3.factory.bin` to the GitHub Release for that tag, creating the release if needed.

**`pages.yml`** deploys `docs/` to GitHub Pages. It triggers on:

- a push to `master` that touches `docs/**`,
- `workflow_dispatch`,
- `workflow_run` after `release.yml` succeeds.

It downloads the three images from the **latest** release, stamps the version (`v2.0.0` → `2.0.0`) into both manifests, and deploys.

## Two manifests, two buttons

- `manifest-lite.json` has two builds, `chipFamily` `ESP32` (Atom Lite) and `ESP32-S3` (AtomS3 Lite). ESP Web Tools detects the chip and picks the matching build.
- `manifest-atoms3.json` has only the display build. AtomS3 has the same chip as AtomS3 Lite, so chip detection can't tell them apart, and the page has a separate button for it.

## One merged file per board

`firmware.factory.bin` from pioarduino is a full-flash image starting at offset `0`: bootloader, partition table, `boot_app0` and the app. The ESP32 image has its bootloader at 0x1000 inside it, the S3 image at 0x0, so offset `0` is correct for both.

## Cutting a release

1. Bump `FW_VERSION` in `src/version.h`.
2. `git tag v2.0.1 && git push origin v2.0.1`

## Hosting

The page is served from **https://keyer.ok1cdj.com** via GitHub Pages. One-time setup:

- **DNS:** `keyer.ok1cdj.com` CNAME → `ok1cdj.github.io.`
- **Repo Settings → Pages:** Source = *GitHub Actions*, Custom domain = `keyer.ok1cdj.com`. Enable *Enforce HTTPS* once the certificate has been issued.
- `docs/CNAME` keeps the custom domain bound on every deploy.

`pages.yml` needs a release that carries the images, so run `release.yml` (push a tag) once before the first page deploy.
