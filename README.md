# vcore

`vcore` is a small educational 64-bit operating system.  The project is
split into a bootloader, kernel sources, and a few user-space programs.

## Repository Layout

- `boot/` – boot sector and second-stage loader
- `src/` – kernel source code
- `user/` – example user programs
- `lib/` – common runtime for the user programs
- `link.lds` – linker script used when building the kernel

## Toolchain

Building requires `nasm`, a GCC cross‑compiler for x86_64 (set with the
`CROSS` make variable) and `qemu-system-x86_64` for running the image.
The Makefile uses tools like `$(CROSS)gcc`, `$(CROSS)ld`, and
`$(CROSS)objcopy`.

A helper script `scripts/build-toolchain.sh` can build a suitable
cross‑compiler.  It downloads and compiles binutils and GCC into
`$HOME/opt/cross` (or the directory specified by the `PREFIX` variable).
The script also installs all required build dependencies on Debian
systems using `apt`.  After running the script, point `CROSS` at the
resulting prefix.

```bash
./scripts/build-toolchain.sh
CROSS=$HOME/opt/cross/bin/x86_64-elf- make
```

## Building

Run `make` at the repository root.  This compiles the kernel, bootloader
and user programs and produces `os.img`.  If `grub-mkrescue` is
available an additional ISO image `os.iso` is created.  The ISO build
relies on the `grub-mkrescue` tool (install the `grub-pc-bin` package on Debian-based systems).  The resulting image should be
around 105&nbsp;MB because it embeds a small FAT filesystem.

```bash
make
```

## Running

You can boot the resulting image with QEMU using the `run` target:

```bash
make run
```

This executes `qemu-system-x86_64 -m 1024 -drive format=raw,file=os.img`.

**Note:** allocate at least 1&nbsp;GB of RAM when running the image.

Alternatively an ISO image can be built using GRUB.  Run

```bash
make iso
```
(requires the `grub-mkrescue` tool)

which produces `os.iso`.  Boot it with the `run-iso` target:

```bash
make run-iso
```

or run

```bash
qemu-system-x86_64 -cdrom os.iso
```

The original bootloader based image `os.img` continues to work.

## Networking

The kernel contains a very small driver for the Intel E1000 network
card.  When run under QEMU with `-net nic,model=e1000 -net user` an
example user program `ping` sends a UDP packet to the host and prints
the reply.

## License

This project is distributed under the terms of the GNU General Public
License version&nbsp;3.  See the `LICENSE` file for full details.
