# Astra Boot Manager v1 — offline preflight and plans

**Status:** Implemented as a *read-only developer tool*. It does not boot a PC, modify partitions, launch QEMU, communicate with PS5 homebrew, or bypass PS5 secure boot. Image matching is **not authenticity verification**.

## Why separate from `astra boot verify`?

The existing `astra boot verify <manifest>` accepts the strict `astraps5-boot-v1` format and verifies a single image. It is unchanged.

The new boot-manager format describes three distinct files: a kernel, an initramfs, and a root filesystem image. `inspect` checks that the files exist within the manifest directory and that their SHA-256 hashes match. `plan` does the same checks and lists the engineering steps needed before a real loader handoff can be implemented.

## CLI

```bash
./build/astra boot inspect /path/to/astra.bootplan
./build/astra boot inspect /path/to/astra.bootplan --json
./build/astra boot plan /path/to/astra.bootplan
./build/astra boot plan /path/to/astra.bootplan --json
```

Exit code 0 means **offline bundle verification succeeded**. Exit 1 means a manifest or asset failed validation. Exit 2 means invalid arguments. There is no boot action or "ready to boot" claim.

## Boot plan manifest

Required keys:

| Key | Description |
| --- | --- |
| `format` | `astraps5-boot-plan-v1` |
| `target` | `linux-pc` or `ps5-linux` |
| `loader` | `qemu` for `linux-pc`, or `ps5-linux-loader` for `ps5-linux` |
| `kernel` / `kernel_sha256` | Relative path and SHA-256 for the kernel file |
| `initrd` / `initrd_sha256` | Relative path and SHA-256 for the initramfs |
| `rootfs` / `rootfs_sha256` | Relative path and SHA-256 for the root image |

For `ps5-linux` only, `console` (`ps5`, `ps5-slim`, or `ps5-pro`) and an explicit `firmware` (for example `13.60`) are required **metadata**, not evidence of actual support. The `linux-pc` target must omit those fields.

Example layout:

```text
bundle/
├── astra.bootplan
├── boot/
│   ├── vmlinuz
│   └── initrd.img
└── system/
    └── rootfs.img
```

Get actual hashes using `sha256sum boot/vmlinuz boot/initrd.img system/rootfs.img`. Use these hashes to replace the `0000...` example values. The template is deliberately not bootable.

The manifest is capped at 64 KiB, unknown and duplicate keys are rejected, paths must be relative without traversal, and symlinked assets are refused. There is no shell execution or storage modification. A matching hash proves only that the image equals the bytes named by the manifest; a compromised manifest could contain matching malicious hashes.

## Road ahead

1. **Image authenticity:** add a signed release-index format and trusted publisher keys; do not treat raw hashes as signatures.
2. **QEMU boot proof of concept:** build a working x86_64 kernel + initramfs + rootfs, then implement a separate opt-in developer runner.
3. **Startup service:** start Astra Shell automatically once the Astra Linux system has booted; ensure the shell can be recovered if it crashes.
4. **PS5 Linux integration:** test Astra Core and Shell on an already-working, compatible PS5 Linux installation; validate kernel, graphics, input, audio, and storage.
5. **Hardware-dependent boot adapter:** integrate a verified upstream PS5 Linux loader, preserving its security/firmware and licensing constraints and fail-safe recovery. No automatic cold boot on PS5 is currently implemented.

The distinction matters: `plan-prepared` means the **offline plan data** is ready for future integration, not that the console is capable of starting the OS.
