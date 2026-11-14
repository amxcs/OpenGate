# How OpenGate works

OpenGate is based on sd2psXtd 1.5.0 and adds a built-in PS2 boot card to the
sd2psx firmware. The console reads this temporary card at startup; its payload
then loads a launcher from the microSD card. Once the launcher file has been
read and closed, OpenGate switches to the normal card used for saves.

This document describes the complete path from installation and power-on to a
subsequent reset, followed by the implementation details. For installation and
build instructions, see [README.md](README.md).

## What is needed

- An sd2psx running the OpenGate firmware and a microSD card.
- A compatible launcher ELF at `BOOT/BOOT.ELF` on the microSD card. The README
  recommends PS2BBL from the SD2PSX / PSxMemCard Gen 1/2 SD Card package.
- The sd2psx in PS2 memory card slot 1.

The built-in card is part of the firmware; it is not a card image that the user
must copy to the SD card. The launcher and its own configuration are separate
from the built-in card. The project README reports testing with BIOS 2.30;
other BIOS versions are not established by that statement.

## Startup, step by step

```text
Power on or real console reset
        │
        ▼
Firmware presents the built-in PS2 card
        │
        ▼
PS2 probes the card, authenticates it and reads the boot payload
        │
        ▼
Payload opens and reads BOOT/BOOT.ELF from the SD card through MMCE
        │
        ▼
The watched file reaches its end and is successfully closed
        │
        ▼
Firmware switches to the selected normal PS2 card
        │
        ▼
Launcher continues; normal card and save features are available
```

1. **Firmware starts.** On the first PS2 card-manager initialization, OpenGate
   marks the built-in card for presentation. It also selects the normal card to
   return to later: the BOOT card if PS2 autoboot is enabled, otherwise the
   default card.
2. **The built-in card is mounted.** The firmware presents a 64 MB virtual PS2
   card backed by data embedded in flash. It resets the card's temporary writes
   and enables the boot-specific read/ECC policy. The PS2 accesses it through
   the normal memory-card interface.
3. **The console boots from the card.** The PS2 probes and authenticates the
   card using its normal protocol, then reads the embedded payload. During this
   phase OpenGate serves the boot version of the relevant card pages and applies
   the special ECC handling described below.
4. **The payload loads the launcher.** It accesses `BOOT/BOOT.ELF` on the SD
   card through MMCE, the file-access interface used by the firmware and
   payload. The launcher is a separate ELF supplied by the user.
5. **OpenGate observes the file access.** While the built-in card is active,
   the firmware watches a successful open of that path, followed by a read
   reaching the file's end and a successful close. It waits until the MMCE
   operation has finished before requesting the card switch.
6. **The normal card takes over.** The built-in card is closed, its temporary
   changes are discarded, and the previously selected BOOT or default card is
   mounted. The launcher can then use the normal card. Game ID selection and
   other inherited sd2psXtd behavior operate on normal cards.

For PS2BBL, enabling PS2 autoboot selects the BOOT card for this handover; the
README recommends this setup so PS2BBL can read its configuration there.

## The built-in card and temporary writes

The implementation is in `src/ps2/opengate_builtin.c`,
`src/ps2/opengate_builtin.h`, and `src/ps2/opengate_builtin_data.inc`.

The virtual card has 131,072 pages of 512 bytes (64 MB). The firmware stores
only the required pages in flash: `gb_pages[]` maps page numbers to offsets in
`gb_blob[]`, and the data is run-length compressed. A page absent from the
image reads as `0xFF`. Out-of-range reads and writes fail.

Pages 254, 57926 and 57927 have alternative versions. Raw boot reads use the
original versions; after the boot read policy is disarmed, reads can use the
alternative versions. Writes are held in a 32-page RAM scratch area. Once raw
reads have ended, a scratch copy takes precedence over the embedded page. A
write to an existing scratch page replaces it; a write to a new page fails if
all 32 slots are occupied. The scratch area is cleared on mount and close, so
these writes do not persist.

The built-in card uses the SD-mode data path, with page reads handled by core 0
through `ps2_cardman_read_sector()` rather than served from PSRAM. The history
tracker is paused while the built-in card is active, so boot activity does not
become game history.

## Boot read and ECC policy

The relevant code is `src/ps2/card_emu/opengate_ecc_policy.h`,
`src/ps2/card_emu/opengate_ecc_layout.h`,
`src/ps2/card_emu/ps2_mc_commands.c`, and `src/ps2/opengate_builtin.c`.
The policy changes what the **built-in card** returns while its payload is
being loaded. It has two independent effects: selecting raw page data and
substituting three spare-area bytes on page 201. Neither is a persistent write.

### State and exact transition

The policy has two Boolean fields, `armed` and `payload_seen`. Mounting the
built-in card calls `ps2_mc_opengate_mount(true)`, setting them to `(true,
false)` and setting `opengate_raw_reads` to `true`. Mounting a normal card or
closing the built-in card calls the same function with `false`, clearing them.
An IOP reset alone does not re-arm the policy.

At the **start of each read-data handler** (`0x43` after the PS2 card
identifier `0x81`), if the built-in card is active, the code observes the
current `read_sector` and then copies `armed` to `opengate_raw_reads`:

| Observed page | Condition | Result |
|---|---|---|
| `59490` / `0x0000E862` | Any policy state | Set `payload_seen = true` |
| `59876` / `0x0000E9E4` | `payload_seen == true` | Set `armed = false` |
| `59876` / `0x0000E9E4` | `payload_seen == false` | Do not disarm |
| Any other page | — | Leave both fields unchanged |

For example, after mount the sequence `0xE9E4, 0xE862, 0xE9E4` leaves the
policy armed after the first two observations and disarms it at the third.
The check observes page numbers at handler entry; it does not verify that every
page between the two markers was read. If one read-data operation crosses a
page boundary, the policy is not observed again inside that operation. The
policy update happens before that handler obtains its page pointer, but the
data interface can have prefetched a page; the policy code alone does not
establish that every byte at a boundary is fetched anew.

The two constants are generated from the loader ELF. Their corresponding
logical, data-only card offsets are `0x01D0C400` and `0x01D3C800`; these are
not PS2 RAM addresses or offsets in the compressed firmware blob.

### Raw versus normal reads

With `opengate_raw_reads == true`, `opengate_builtin_read(page, out, true)`
ignores RAM scratch writes and looks up the original key for the page. An
absent page yields 512 copies of `0xFF`. Once raw mode is off, the read first
checks scratch, then tries alternative keys for pages `254` (`0xFE`), `57926`
(`0xE246`) and `57927` (`0xE247`), and finally falls back to the original
key. The alternative key has bit 31 set: `0x800000FE`, `0x8000E246` or
`0x8000E247`. Writes still go to scratch while raw mode is on, but raw reads
do not return those scratch copies.

### Exact page 201 spare-byte substitution

The PS2 read stream represents a page as `0x200` (512) data bytes followed by
`0x10` (16) spare bytes. For page `201` (`0x000000C9`, logical data offset
`0x00019200`), the handler normally obtains a spare byte from
`readecc[readptr - 0x200]`. **Only when the built-in card is active and the
policy is armed**, it replaces these three returned bytes:

| `readptr` | Spare index | Byte returned |
|---|---|---|
| `0x209` (521) | `9` | `0x70` |
| `0x20A` (522) | `10` | `0x84` |
| `0x20B` (523) | `11` | `0xFB` |

This overrides the response variable, not `readecc[]` or the embedded image.
Other spare positions retain their normal computed values. The read command's
XOR checksum includes the bytes actually returned, including substitutions.
The substitution is conditional on the page and policy **at the time each byte
is emitted**; it is not an edit of the card's 512 data bytes.

For a reproducible example, decompressing the current image's ordinary page
201 gives these first 32 data bytes:

```text
page offset  00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
000          81 53 00 80 82 53 00 80 83 53 00 80 84 53 00 80
010          FF FF FF FF FF FF FF FF A9 53 00 80 88 53 00 80
```

For the standard (non-SC2) ECC algorithm and a fresh, sequential read of all
512 data bytes of this unchanged page, applying the firmware's `EccTable` to
the four 128-byte blocks produces these first 12 spare bytes:

```text
spare index   00 01 02 03 04 05 06 07 08 09 0A 0B
computed      61 3C 43 00 38 38 25 3C 43 00 3E 3E
armed output  61 3C 43 00 38 38 25 3C 43 70 84 FB
```

The computed row is derived from the checked-in compressed page and current
ECC table; it is not a fixed constant hard-coded into the handler. It assumes
the stated page contents and read sequence. The last three values in the armed
row **are** the hard-coded substitutions. This example is not an SC2 trace.

### How the normal ECC bytes are accumulated

On setting a read address, the handler resets `readptr` to zero, points
`eccptr` at the 16-byte `readecc` buffer and initializes that buffer. For the
standard variant it starts at `00` for all 16 bytes. Each data byte `b` indexes
`EccTable[b]`; the result is XORed into the first accumulator byte. If its high
bit is set, the low seven bits of the current position and their complement
are XORed into the next two accumulator bytes. At each `0x80`-byte boundary,
the three accumulators are complemented and masked with `0x77`, `0x7F`, and
`0x7F`, respectively; `eccptr` then advances by three bytes. Four 128-byte
blocks therefore produce 12 ECC bytes. The code's read loop also processes the
first spare-byte position under its `readptr <= 0x200` condition; the example
above reports the first 12 returned spare bytes, which are already accumulated
from the data.

For the SC2 variant, the code instead initializes the 16-byte buffer to `FF`
and uses an MSB-first CRC with polynomial `0x04C11DB7`, initially zero. At the
first spare position it places the CRC in `readecc[0..3]`, most significant
byte first; the special CRC `0xE76D8BF0` is mapped to `0xFFFFFFFF`. The same
page-201 overrides still apply when their conditions are met. These are
implementation details of the two variants, not a general specification for
every PS2 card.

After `readptr` reaches `0x210`, the handler resets it to zero, increments the
page number and resets ECC state for the next page. The command emits an XOR
checksum and terminator after the requested bytes. Exact results depend on the
bytes actually read and the configured card variant.

## When the handover happens

The watcher is in `src/ps2/opengate_elf_watch.h`; MMCE file operations call it
from `src/ps2/mmceman/ps2_mmceman_fs.c`, and the card manager handles the
switch in `src/ps2/ps2_cardman.c`.

The path comparison ignores leading slashes and is case-insensitive. The open
must succeed while the built-in card is active. A positive read on the watched
file must leave its position at or beyond its size, and the size must be at
least 52 bytes. The watched descriptor must then close successfully. This is
an end-position check, not proof that every byte was read sequentially or that
the ELF is valid. The close notification also requires the built-in card to be active and the raw
boot-read policy to be disarmed at that moment. The switch is scheduled only
after these conditions and completion of the MMCE operation. A close that
occurs while raw reads are still active does not by itself schedule a later
switch.

If the file cannot be opened, never reaches the required end position, or is
not successfully closed, this handover condition is not met. Check the SD card,
the exact `BOOT/BOOT.ELF` location and the launcher if startup stops here.

## Recognizing a real reset

A console reset should present the built-in card again. An internal IOP reload
by PS2BBL, a launcher or a game should not: the normal card must remain
available. Both kinds of event can send the same `0xF3` authentication reset,
so OpenGate does not use that command alone to distinguish them.

In `src/ps2/card_emu/ps2_memory_card.c`, the firmware groups memory-card
transactions into clusters separated by at least 100 ms. A PS2 boot probe
contains a burst of `0x21` transactions. The code requests a re-arm when a
cluster reaches 12 such transactions and the preceding silence was at least
1.4 seconds (`OPENGATE_BOOT_BURST_MIN` and `OPENGATE_RESET_PREGAP_US` in
`src/ps2/ps2_cardman.h`). The timing described by the implementation is:

| Event | Observed silence before the probe burst |
|---|---|
| Real reset or power cycle | Approximately 1.7 seconds or more |
| PS2BBL, launcher or game IOP reload | Approximately 1.0 second or less |

These are timing observations used by a heuristic, not a protocol guarantee.
The burst reaches the threshold roughly 1.1 seconds before `0xF3`, giving the
firmware time to switch cards before authentication. Switching at `0xF3` would
interrupt the handshake. If the built-in card is already active or pending,
the reset request does not trigger another switch.

For this re-arm, `src/ps2/mmceman/ps2_mmceman.c` skips the normal 500 ms
card-removal delay and the mcman request-ignore ramp. The console is booting and
will authenticate again; delaying or ignoring its early authentication
commands would interfere with that boot. After the next launcher handover, the
firmware returns to the BOOT card when autoboot is enabled, otherwise the
default card, rather than retaining the last game's Game ID card.

## Reset after a PS1 game

A PS1 game puts the firmware into temporary PS1 mode. A real reset from there
starts a PS2 boot, whose probe is visible on the PS1 bus. The PS1-side detector
in `src/ps1/ps1_memory_card.c` uses the same 1.4-second silence and 12-command
thresholds. It starts its silence clock on PS1-mode entry, including the first
PS1 session after a cold start.

When the probe is recognized, `src/ps1.c` requests a re-arm and returns the
firmware to PS2 mode. PS2 card-manager initialization then presents the
built-in card, and the normal boot and launcher handover proceed as above.

## Display and build

`src/gui.c` forces the OLED header to refresh on PS1/PS2 mode entry so that the
card name reflects the current mode and mounted card.

OpenGate builds like upstream sd2psXtd. The output is `sd2psx.uf2`; an untagged
build reports version `1.5.0-opengate`. See the build links in
[README.md](README.md).


## Byte-level reference

The numbers below describe this firmware's current implementation. A **page
number** is a logical index into the emulated card, not a CPU address or a
physical flash address. A data-only byte offset is `page × 512 + byte_in_page`;
the 16 protocol spare bytes are not included in that offset. The compressed
firmware blob has its own unrelated offsets.

| Item | Value |
|---|---|
| Card data size | `0x04000000` bytes (64 MiB) |
| Page data size | `0x200` bytes (512) |
| Number of pages | `0x20000` (131,072), indexed `0x00000`–`0x1FFFF` |
| Valid data-only byte offsets | `0x0000000`–`0x3FFFFFF` |
| Per-page protocol spare area | 16 bytes, after the 512 data bytes |
| Empty, unrepresented data | `0xFF` repeated for the whole page |
| Scratch capacity | 32 distinct pages, 16,384 data bytes plus metadata |
| Invalid page | Any page `>= 0x20000`: read/write returns `-1` |

### Pages and offsets used by OpenGate

Offsets in this table refer to the start of the **512-byte data area** in the
logical card image. They do not identify flash locations. `read_sector` is the
page number used by the card-emulation code.

| Purpose | Page (decimal / hex) | Data offset |
|---|---|---|
| Special ECC response | 201 / `0x00C9` | `0x0019200` |
| Alternative page | 254 / `0x00FE` | `0x001FC00` |
| Alternative page | 57926 / `0xE246` | `0x1C48C00` |
| Alternative page | 57927 / `0xE247` | `0x1C48E00` |
| Payload first-page marker | 59490 / `0xE862` | `0x1D0C400` |
| Restore last-page marker | 59876 / `0xE9E4` | `0x1D3C800` |

The marker logic checks these two exact page numbers; it does not verify that
every intervening page was read. On a read-data command, observing `0xE862`
sets `payload_seen`. Observing `0xE9E4` after that clears `armed` **before**
the read-data handler obtains the page and emits its bytes. The labels are
layout markers generated from the loader ELF, not runtime CPU addresses.

### Sparse image encoding

Each `gb_pages[]` entry holds a 32-bit page key and a 32-bit byte offset into
`gb_blob[]`. Lookup is binary search. For a non-raw read of one of the three
alternative pages, the lookup first sets bit 31 in the key (`page |
0x80000000`); if that key is absent, it falls back to the ordinary key. For
example, page `0xFE` uses alternative key `0x800000FE`. This high bit is an
indexing marker, not part of the console's page address.

Each compressed run begins with a control byte `t`. Its length is
`(t & 0x7F) + 1`, between 1 and 128 bytes. If `t & 0x80` is set, the next byte
is repeated for that length; otherwise the next `length` bytes are copied
literally. Decoding continues until exactly 512 bytes have been produced. A run
that would exceed the page returns `-1`. The generated image is compiled into
the firmware; a blob offset is not the same as the logical card offset above.

### Data, spare bytes and ECC

For each page, `readptr` positions `0x000`–`0x1FF` address the 512 data bytes;
`0x200`–`0x20F` address the 16 spare bytes. While the built-in card is active
and the policy is armed, reads of page `0xC9` substitute these response bytes:

| `readptr` | Spare-byte index | Returned value |
|---|---|---|
| `0x209` | 9 | `0x70` |
| `0x20A` | 10 | `0x84` |
| `0x20B` | 11 | `0xFB` |

The substitution affects bytes returned by the read handler; it is not a
persistent write to the image. On reaching `readptr == 0x210`, the handler
resets the pointer to zero and increments `read_sector`. Other spare-byte and
ECC processing follows the card-emulation implementation, including its
variant-specific behavior.

### Bus identifiers and commands

The following values are defined in `src/ps2/card_emu/ps2_mc_commands.h` and
`src/ps2/mmceman/ps2_mmceman_commands.h`. The meaning of a byte depends on its
position: the reset detector examines the **first received transaction byte**;
the commands in the second column are subcommands after the `0x81` PS2 card
identifier. In particular, a first byte `0x21` in the probe detector must not
be assumed to be the `0x21` erase-address subcommand after `0x81`.

| Context | Byte | Meaning in this firmware |
|---|---|---|
| First byte | `0x21` | Counted by the boot-probe reset heuristic |
| First byte | `0x81` | PS2 memory-card command identifier |
| First byte | `0x8B` | MMCE command identifier; firmware initially responds `0xAA` |
| PS2 card subcommand | `0x21` / `0x22` / `0x23` | Set erase / write / read page address |
| PS2 card subcommand | `0x26` | Get card specifications |
| PS2 card subcommand | `0x27` / `0x28` | Set / get terminator |
| PS2 card subcommand | `0x42` / `0x43` | Write / read data |
| PS2 card subcommand | `0x81` / `0x82` | Commit data / erase |
| PS2 card subcommand | `0xF3` | Authentication reset |
| PS2 card subcommand | `0xF7` / `0xF0` | Key selection / authentication |
| PS2 card subcommand | `0xF1` / `0xF2` | Session-key operations |

The address handlers receive four bytes into a `uint32_t` through a union;
on the target's little-endian CPU the least significant byte comes first. For
example, page `0x0000E862` is represented by address bytes `62 E8 00 00`.
The handlers also receive a checksum byte, but the current code explicitly
marks checksum validation as TODO. The handlers issue `0x2B` and a terminator
as part of their response; the default terminator is `0xFF` and can be changed
through the protocol. This is an implementation reference, not a complete
wire-level trace or a claim that every transaction has the same timing.

### Reset and handover state

- A cluster begins on the first observed transaction or when the time between
  transaction **starts** is at least `100000` µs. A gap is measured with the
  32-bit microsecond timer; the previous start is updated on each transaction.
- Within that cluster, the detector counts first bytes equal to `0x21`. When
  the count becomes exactly `12` (`0x0C`) and the gap before the cluster is at
  least `1400000` µs, it requests re-arm. The count need not be consecutive;
  other first bytes do not themselves reset it unless a new cluster begins.
- PS2 mode communicates the request using an atomic flag. The card-manager
  task consumes it, selects BOOT/default, sets pending and requests a switch
  if the built-in card is neither active nor already pending.
- A normal card switch waits for its timeout, no buttons held, no MMCE operation
  in progress and an idle backend. For a pending built-in card, the switch
  skips the usual `500` ms delay and retry ramp; mount also clears the retry
  counter. These prerequisites still apply.
- The launcher watcher holds one file descriptor and a completion flag. It
  marks completion on a positive read with file size `>= 52` (`0x34`) bytes
  and file position `>=` size. A successful close clears the watcher; the
  card manager marks the handover finished only if raw reads are already off.
  The actual switch waits for the MMCE operation to finish.

The repository does not provide a complete BIOS instruction trace or payload
source-level execution trace in this document. Values above are derived from the
firmware and its embedded image layout; do not interpret logical card offsets
as ELF load addresses, PS2 RAM addresses or flash addresses.
