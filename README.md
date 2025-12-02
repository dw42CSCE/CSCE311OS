# RISC-V Mini OS playground

Tiny cooperative kernel for QEMU `virt`: built-in shell, demo apps, a toy in-memory file system, a user-program loader (scripts with basic capability checks), and sync primitives to show concurrency.

## Environment setup (lab machine)
1) Install toolchain/qemu (Debian/Ubuntu example):
```
sudo apt-get install qemu-system-misc gcc-riscv64-unknown-elf
```
2) Build and run:
```
make            # builds kernel.bin with riscv64-unknown-elf-gcc
qemu-system-riscv64 -machine virt -nographic -bios none -kernel kernel.bin
```
Inside QEMU you land at `$` (shell). `Ctrl-A` then `x` exits QEMU.

### Shortcut script
`sudo ./runqemu.sh` will:
- Look for `qemu-system-riscv64` in PATH or `../tools/qemu-system-riscv64`.
- If missing, attempt to download it into `../tools` via `apt-get download qemu-system-misc`.
- Build `kernel.bin` if absent, then launch QEMU with the right flags.

## Shell commands
- `help` / `stop`
- `ls` / `apps` – list built-in apps; `run <app>` spawns as a thread (`ps` to view, `kill <tid>` to drop)
- `fs ls|read <f>|write <f> <data>|rm <f>|format` – RAM-backed file store (16 files, 256B each). `fs ls` now shows byte sizes.
- `prog ls|runall|load <name> <caps> <script>|loadfile <name> <caps> <file>|save <name> <file>|run <name>|drop <name>` – load/run user scripts; scripts can live in FS now.

## Apps and concurrency demos
- `run pinger` / `run counter` to see interleaved cooperative threads.
- `run sync` spawns producer/consumer using mutex + semaphores.
- `run fs-demo` writes/reads `hello.txt` via the toy FS.
- `run prog-demo` loads a sample script that prints, touches FS, and spawns another app.
- `run sleepers` shows the new `thread_sleep` API with staggered wakeups.
- `run barrier` uses the new barrier primitive to synchronize 3 workers across phases.
- `run prog-file` writes a script to FS, loads it via `prog loadfile`, and runs it.

## User programs (loader)
Example:
```
prog load demo 15 "print hi;write note demo;read note;spawn pinger;exit"
prog run demo
```
Capability bitmask: `1=UART`, `2=FS read`, `4=FS write`, `8=spawn apps`. Scripts are semicolon/newline-separated commands: `print <text>`, `yield`, `sleep <n>`, `write <file> <data>`, `read <file>`, `spawn <app>`, `exit`. Interpreter enforces caps; each script runs as its own thread.

You can also keep scripts on the in-memory FS: `prog loadfile <name> <caps> <filename>` reads a file and loads it as a program, while `prog save <name> <filename>` persists a loaded script back to the FS. `prog runall` spawns every loaded program at once.

## File system
`fs format` clears; `fs write name data` stores up to 256 bytes per file; `fs read name` prints contents; `fs ls` lists files; `fs rm` deletes.

## Source map (what each file does)
- `entry.S` – boot entry; sets stack and jumps to `kernel_main`.
- `kernel.c` – shell, command parser, and scheduler tick integration; initializes FS and program loader.
- `thread.c` / `thread.h` – cooperative threading, scheduler, spawn/kill/ps, context save/restore.
- `context.S` – context switch routine saving/restoring ra/sp/s0–s11.
- `thread_trampoline.c` – trampoline into new thread start routine.
- `apps.c` / `apps.h` – built-in apps and demos (`pinger`, `counter`, `sync`, `fs-demo`, `prog-demo`); `app_spawn`, `app_list`.
- `sync.c` / `sync.h` – mutex and semaphore primitives (busy-wait + yield).
- `fs.c` / `fs.h` – in-memory file store backing the `fs` shell commands and app usage.
- `prog.c` / `prog.h` – script loader/interpreter with capability checks; `prog load/run/drop/ls`.
- `uart.c` / `uart.h` – minimal 16550-style UART access.
- `string.c` / `string.h` – tiny string/memory helpers used across the kernel.
- `linker.ld` – layout, stack symbol.
- `Makefile` – builds `kernel.bin` with riscv64-unknown-elf toolchain.

## Notes / limits
- Cooperative only: progress depends on `thread_yield`/`sched_tick`; no preemption or timers.
- All state is RAM-only; power cycle loses FS/programs (but you can round-trip scripts with `prog save`/`loadfile`).
- Capability checks are coarse; there’s no memory isolation beyond the interpreter.
- UART is the only I/O; keep scripts short (<256 chars) to fit buffers.
