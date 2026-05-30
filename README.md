# AMIX Hydra Ethernet Driver

A STREAMS/DLPI network driver for the **Hydra Systems AmigaNet rev 1.2a** Zorro II Ethernet card (National Semiconductor DP8390/NE2000) for **AMIX 2.1p2** — Commodore's System V Release 4 Unix for the Amiga.

## Background

AMIX (Amiga Unix) is SVR4 Unix for Motorola 68020+ Amiga systems, originally developed by Commodore-Amiga and later sold by Haage & Partner. It includes a fully-functional STREAMS networking stack with DLPI-based Ethernet drivers.

The existing AMIX kernel ships with the `aen` driver for the Commodore A2065 Ethernet card (AMD LANCE-based). This project adds support for a second NIC: the **Hydra Systems AmigaNet**, a Zorro II card built around the ubiquitous **National Semiconductor DP8390 (NE2000)** chip.

The driver follows the same DLPI/STREAMS architecture as the A2065 driver, making it a useful reference for anyone porting other Zorro II network cards to AMIX.

## Hardware

| Feature | Detail |
|---------|--------|
| Card | Hydra Systems AmigaNet rev 1.2a |
| Chipset | National Semiconductor DP8390 (NE2000-compatible) |
| Bus | Zorro II (autoconfig ID: 1053/1 = 0x041D0001) |
| NIC offset | board + 0xffe1 (odd byte lane, D0–D7) |
| MAC PROM | board + 0xffc0 (16-bit bus, every other byte) |
| SRAM | 16 KB on-card packet buffer |
| Media | 10Base2 (BNC) and 10BaseT (RJ45) |

## Repository Structure

```
├── stand/           Boot code (level 1 + level 2 bootstrap, COFF/ELF tools)
├── usr/
│   └── sys/
│       ├── amiga/
│       │   ├── driver/
│       │   │   ├── hydra/        ← This driver (ne2000.h, hydra.c, hydra.h, ...)
│       │   │   ├── aen/          ← A2065 LANCE driver (reference implementation)
│       │   │   └── ...
│       │   ├── kernel/
│       │   │   ├── support.c     ← autocon() / bootinfo handling
│       │   │   └── servant.s     ← halt/reboot (ASM)
│       │   ├── boot/             ← boot2.c bootstrap loader
│       │   ├── config/           ← kernel config (unix.c, mapfiles)
│       │   └── Makefile          ← top-level platform build
│       ├── master.d/
│       │   ├── kernel.c          ← cdevsw, int2_tbl, init_tbl
│       │   └── kernel            ← SVR4 master.d config
│       ├── ml/                   ← machine layer (3B2 derived)
│       ├── os/                   ← OS core
│       └── Makefile              ← top-level kernel build
├── zorro_probe.c                 ← User-space Zorro II detection tool
└── README.md
```

## Driver Architecture

The driver is a STREAMS DLPI provider (`struct streamtab hydrainfo`) registered at cdevsw slot 47, using major/minor device `hya`. It follows the same pattern as the A2065 (`aen`) driver:

- **`hydraopen()`** — STREAMS open; calls `hydraautoconfig()` to detect the card, then `hydra_initialize()` to set it up
- **`hydraautoconfig()`** — three-method card detection:
  1. `autocon(NE8390_BOARD_ID, ...)` — uses kernel's bootinfo table (works at runtime)
  2. Direct Zorro II slot probe — reads MAC PROM at each slot's base+0xffc0 (independent of bootinfo)
  3. A2065 fallback — for testing in FS-UAE which only emulates the Commodore A2065
- **`hydrawput()`** — STREAMS write-side put procedure, handles DLPI primitives (DL_INFO_REQ, DL_BIND_REQ, DL_UNITDATA_REQ, etc.)
- **`hydraintr()`** — INT2 interrupt handler, registered in `int2_tbl[]`, processes RX/TX/error interrupts from the DP8390
- **`setup_ne2000()`** — initializes the DP8390 registers, ring buffers, and physical address

## Building

### Prerequisites

- **m68k-amix cross-compiler** (GCC 2.7.2.3 targeting AMIX SVR4)
- Build host: Linux (cross-compilation) or native AMIX

### Kernel build (cross-compile)

```sh
cd usr/sys
make CC=m68k-amix-gcc CFLAGS="-O -D_KERNEL -DSVR40 -DSVR4"
```

This produces `unix` (ELF) and (via `elf2coff`) a COFF kernel image ready for boot.

### Boot image

```sh
cd stand
make oldboot KERNEL=/path/to/unix
```

This generates a bootable floppy/hard-disk image with the compressed kernel, bootstrap, and info blocks. See `stand/README` for details.

### Zorro probe utility

```sh
gcc -O -o zorro_probe zorro_probe.c
./zorro_probe   # (run as root)
```

Scans Zorro II slots via `/dev/mem` and probes the hydra and aen STREAMS drivers.

## Testing

The kernel boots and the driver initializes when `ifconfig hya0 plumb` is run:

```sh
su
ifconfig hya0 plumb
ifconfig hya0
ping 192.168.1.1   # (once network is configured)
```

On systems without a Hydra card (e.g., FS-UAE emulating an A2065), the driver falls back to a testing mode using a fake MAC address, allowing the STREAMS/DLPI plumbing to be validated without the target hardware.

## Known Issues

- **Bootinfo timing**: The kernel's `autocon()` function reads from the `bootinfo` table, which is populated by `config()` in `support.c`. During very early boot (`init_tbl` processing), the table may not yet be valid. The driver therefore configures itself lazily at `ifconfig` time (like the A2065 driver), avoiding `init_tbl` entirely.
- **A2065 fallback**: When no Hydra is detected but an A2065 is present (FS-UAE), the driver creates a software-emulated board — DLPI operations work, but actual packet I/O is not possible since the NE2000 register set differs from LANCE.

## License

AMIX is proprietary software of Commodore-Amiga / Haage & Partner. This driver is provided as a reference implementation for educational and research purposes. No AMIX kernel binaries are included in this repository — only build scripts and driver source code intended to be compiled against a licensed AMIX 2.1p2 installation.

## References

- *AMIX System Administrator's Guide* (Commodore-Amiga)
- *DP8390/NS32490 NIC Datasheet* (National Semiconductor)
- *NE2000 Programmer's Guide* (Novell/National)
- *Amiga Hardware Reference Manual* (Commodore)
- *System V Release 4 STREAMS Programmer's Guide* (AT&T)
