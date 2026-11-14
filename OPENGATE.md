# OpenGate on sd2psXtd 1.5.0

This branch adds a **built-in boot card** to the sd2psXtd firmware (based on
the `1.5.0` tag). On power-up, and after every real console reset, the
firmware presents a built-in memory card image that boots `BOOT/BOOT.ELF`
from the SD card. When the ELF has been read completely, the firmware
switches back to the normal card selection (the BOOT card if PS2 autoboot is
enabled, otherwise the default card).

## How it works

- **Built-in card** (`src/ps2/opengate_builtin.c`, `opengate_builtin_data.inc`): a
  sparse, compressed 64 MB card image served from flash. Writes go to a small
  RAM scratch area and are never persisted.
- **ECC policy** (`src/ps2/card_emu/opengate_ecc_policy.h`, `opengate_ecc_layout.h`):
  while the payload is being loaded, raw pages are served and sector 201
  returns patched ECC bytes. The patch is disabled after the payload pages have
  been read.
- **Hand-back** (`src/ps2/opengate_elf_watch.h`, `ps2_mmceman_fs.c`): when
  `BOOT/BOOT.ELF` has been read to the end and closed over MMCE, the built-in
  card is unmounted and the selected card is mounted.
- **Real-reset detection** (`opengate_reset_track()` in
  `src/ps2/card_emu/ps2_memory_card.c`): a console boot probes the bus with a
  burst of ~22 transactions that start with `0x21`, about 1.1 s before the
  `0xF3` auth reset. A user reset or power cycle has at least ~1.7 s of bus
  silence before that burst. The console's own IOP reboots (PS2BBL, OSD, game
  launches) have about 1.0 s or less. When the burst reaches 12× `0x21` after
  at least 1.4 s of silence, the built-in card is mounted again. The card
  switch for this skips the stock 500 ms pause and the mcman ignore ramp, so it
  finishes before MagicGate auth starts.
- **PS1 mode**: if a PS1 game switched the firmware to temporary PS1 mode, the
  same burst is detected on the PS1 side and the firmware goes back to PS2 mode
  with the built-in card mounted.
- The OLED header is redrawn on every PS1/PS2 mode entry.

## Build

Same as upstream sd2psXtd. The output `sd2psx.uf2` reports version
`1.5.0-opengate` when built from an untagged commit.
