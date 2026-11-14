# How OpenGate works

OpenGate adds a **built-in boot card** to the sd2psXtd 1.5.0 firmware. This
document describes the boot flow, the built-in card, and how a real console
reset is told apart from the console's own internal restarts, with pointers to
the source files involved.

## Boot flow

```
power on / reset
      │
      ▼
console boot probe (burst of 0x21 commands) ──► OpenGate mounts the built-in card
      │
      ▼
MagicGate auth and reads from the built-in card
      │
      ▼
payload runs, loads BOOT/BOOT.ELF from the SD card over MMCE
      │
      ▼
BOOT.ELF read to the end and closed ──► OpenGate mounts the selected card
      │                                  (BOOT card if PS2 autoboot is on,
      ▼                                   otherwise the default card)
launcher starts and uses the normal card
```

## The built-in card

Files: `src/ps2/opengate_builtin.c`, `src/ps2/opengate_builtin.h`,
`src/ps2/opengate_builtin_data.inc`

- The card is a 64 MB PS2 memory card image stored in the firmware flash. Only
  the pages that the boot needs are stored (`gb_pages[]`, sorted by page
  number); each is run-length compressed in `gb_blob[]`. Pages that are not
  stored read as `0xFF`.
- A few pages have a second version (marked with the high bit in
  `gb_pages[]`): the console sees the first version while the payload is being
  loaded and the second one afterwards.
- Writes go to a RAM scratch area of 32 pages and are never saved; it is
  cleared every time the built-in card is mounted.
- The built-in card is served in SD mode: every page read goes through core 0
  (`ps2_cardman_read_sector()`), not through PSRAM.
- While the built-in card is mounted, the history tracker is paused, so the
  boot does not show up in the game history.

## ECC policy

Files: `src/ps2/card_emu/opengate_ecc_policy.h`,
`src/ps2/card_emu/opengate_ecc_layout.h`, `src/ps2/card_emu/ps2_mc_commands.c`

While the payload is being loaded, the policy is **armed**: raw pages are
served and one sector reports adjusted ECC spare bytes, so the payload pages
read back as written. Once the payload range has been read (the page numbers
in `opengate_ecc_layout.h`), the policy disarms and the card behaves normally.
The policy is reset on firmware boot and when the card is mounted; it is not
re-armed by an IOP reset.

## Handing back to the normal card

Files: `src/ps2/opengate_elf_watch.h`, `src/ps2/mmceman/ps2_mmceman_fs.c`,
`src/ps2/ps2_cardman.c`

The payload loads the launcher by reading `BOOT/BOOT.ELF` from the SD card
through MMCE. `opengate_elf_watch` follows that file: when it has been read to
the end and closed, the built-in card is unmounted and the selected card is
mounted. The selected card is the BOOT card when PS2 autoboot is enabled,
otherwise the default card.

## Real-reset detection

File: `src/ps2/card_emu/ps2_memory_card.c` (`opengate_reset_track()`)

OpenGate must bring the built-in card back on a real reset, but **not** on the
console's own internal restarts (PS2BBL, a launcher or a game reloading the
IOP). All of these send the same `0xF3` auth reset, so the auth reset alone
cannot tell them apart. OpenGate uses the timing of the boot probe instead.

Memory card transactions are grouped into clusters separated by at least
100 ms. When the PS2 boots, it probes the bus with a burst of about 22
transactions that start with `0x21`, roughly 1.1 s before the `0xF3` auth
reset. The distinguishing signal is the **silence before that burst**:

| Event | Silence before the burst |
|---|---|
| Real reset / power cycle | ≈ 1.7 s or more (bus idle + hardware boot) |
| PS2BBL / launcher / game IOP reload | ≈ 1.0 s or less |

When the burst reaches `GATEB_BOOT_BURST_MIN` (12) `0x21` transactions after
at least `GATEB_RESET_PREGAP_US` (1.4 s) of silence, the re-arm is requested.
This happens about 1.1 s before the `0xF3`, so the card switch finishes before
MagicGate auth begins — switching at the `0xF3` itself breaks the handshake.

The re-arm switch is **fast**: it skips the stock 500 ms "card removed" pause
and the mcman ignore ramp. The console is rebooting and runs a full auth
anyway, so there is no mcman cache to invalidate; keeping the ramp would make
the BIOS's first auth commands be ignored and the boot would fail.

After a re-arm, OpenGate returns to the BOOT card (as on power-up), not to
whatever Game ID card the last game had selected.

## PS1 mode

Files: `src/ps1/ps1_memory_card.c`, `src/ps1.c`

A PS1 game switches the firmware into temporary PS1 mode. A reset from there
is a PS2 console boot, so the same probe burst appears on the PS1 bus. The PS1
side detects it the same way (`gateb_ps1_reset_track()`), asks for a re-arm
and switches the firmware back to PS2 mode, where the built-in card is mounted
from the first access. The silence clock is started on PS1-mode entry so that
the very first PS1 session after a cold start is also covered.

## Display

File: `src/gui.c`

The OLED header is redrawn on every PS1/PS2 mode entry, so the card name on
screen always matches the mounted card after a mode change.

## Build

Same as upstream sd2psXtd. The output `sd2psx.uf2` reports version
`1.5.0-opengate` when built from an untagged commit.
