# OpenGate

[![Ko-fi](https://img.shields.io/badge/Ko--fi-amxcs-ff5e5b?logo=kofi&logoColor=white)](https://ko-fi.com/amxcs)

OpenGate is a firmware for the **sd2psx** memory card emulator, based on
[sd2psXtd](https://github.com/sd2psXtd/firmware) 1.5.0. It adds a **built-in
boot card**: the PS2 boots your homebrew launcher straight from the SD card,
with no exploit installed on a memory card and nothing else to set up on the
console.

The built-in card is presented again on **every** console start:

- cold boot (power on)
- reset button, from the menu
- reset button, from inside a PS1 or PS2 game

After the launcher has been loaded, OpenGate switches back to your normal
card, so saves, Game ID switching and the other sd2psXtd features keep
working as usual.

The exploit runs very early, **before the PS2 boot logo (splash screen)**, so
that screen is skipped and your launcher comes up directly.

OpenGate has been tested and confirmed working with **BIOS 2.00, 2.20 and 2.30**.
**BIOS 2.00, 2.20 support is not yet included in a published release.**

Support for **BIOS 1.10, 1.20, 1.50, 1.60, 1.70, 1.90 and 2.50**
is expected soon.

## What you need

- An sd2psx with a microSD card
- `sd2psx.uf2` from the [Releases](../../releases) page, or your own build
  (see [Building](#building))
- The ELF you want to boot, saved on the SD card as `BOOT/BOOT.ELF`:

  ```
  SD card
  └── BOOT/
      └── BOOT.ELF
  ```

## Installation

1. **Flash the firmware.** Hold the sd2psx button while you connect it to
   your computer over USB. A drive named `RPI-RP2` appears. Copy
   `sd2psx.uf2` to it. The device restarts with the new firmware.
2. **Copy your ELF** to the SD card as `BOOT/BOOT.ELF`.
3. Insert the sd2psx in memory card slot 1 and turn on the console.

## Recommended setup

I recommend **[PS2BBL Extended](https://github.com/saildot4k/PlayStation2-Basic-BootLoader-Extended)** from the **SD2PSX / PSxMemCard Gen 1/2 SD Card**
package: [MMCE-ALL.7z](https://downloads.ps2homebrewstore.com/MMCE-ALL.7z).
Use its PS2BBLE[^1] as `BOOT/BOOT.ELF`, and enable the PS2 boot card in the sd2psx
settings (PS2 autoboot). After PS2BBLE[^1] has started, OpenGate returns to the
BOOT card, and PS2BBLE[^1] reads its configuration from there.

If you want to keep the PS2 boot logo (splash screen), use
[OSDMenu](https://github.com/pcm720/OSDMenu) as `BOOT/BOOT.ELF`: it boots the
same way as the stock browser, with the splash screen shown.

For disc fans: if you want an authentic OSDMenu, you can also use my version:
`BOOT.ELF` from the [latest release](https://github.com/amxcs/OpenGate/releases/latest).
As a bonus, burned discs start instantly and a few screen flickers are gone
(it uses a custom disc loader).

Booting burned discs requires [Force Unlock V2](https://github.com/amxcs/MechaPwn-v2-unofficial/releases/tag/force-unlock-v2-r1) to be installed.

## How it works

On start-up the console reads the built-in card and loads `BOOT/BOOT.ELF`
from the SD card. When the file has been read completely, OpenGate switches
back to the selected card. A real console reset brings the built-in card
back; the console's own internal restarts (for example when a launcher or a
game reloads the IOP) do not.

The full description is in [OPENGATE.md](OPENGATE.md).

## Building

OpenGate builds the same way as sd2psXtd. See the
[sd2psXtd README](README.sd2psXtd.md) and the upstream documentation. Builds
from an untagged commit report version `1.5.0-opengate`.

## Credits

- [sd2psXtd](https://github.com/sd2psXtd/firmware): the firmware OpenGate is based on
- [sd2psx](https://github.com/sd2psx) by @xyzz: the original hardware and firmware
- [PS2BBL Extended](https://github.com/saildot4k/PlayStation2-Basic-BootLoader-Extended): the recommended launcher
- [OSDMenu](https://github.com/pcm720/OSDMenu) by pcm720: launcher that keeps the PS2 boot logo



## License

GPL-3.0, the same as sd2psXtd. See [LICENSE](LICENSE.txt).

## Support

If OpenGate was useful to you, you can
[buy me a coffee](https://ko-fi.com/amxcs).

[^1]: PlayStation 2 Basic BootLoader Extended
