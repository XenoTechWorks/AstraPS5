# Astra Boot Preflight — manifest v1

> **Experimental test project.** Boot Preflight is a read-only **desktop/Linux image-integrity utility**. It does **not** identify your PS5, establish that firmware is supported, boot Linux, execute a loader, mount an image, or write to any disk.

## Why this exists

The long-term Astra Entry (PS5-side) application should refuse to launch a missing, incomplete, or corrupted gaming environment. This first milestone supplies a versioned manifest format and a SHA-256 verification routine that can be tested safely on normal Linux.

## Manifest format

Copy `examples/astraps5.boot.example` to the same directory where your intended `.img` file resides, then edit the values. Required fields:

| Key | Description |
| --- | --- |
| `format` | Must be exactly `astraps5-boot-v1` |
| `image` | A relative path to a regular image file in the same directory or its subdirectories; no `..`, absolute paths, or spaces |
| `sha256` | 64-character hexadecimal SHA-256 of that image (not of the manifest) |
| `console` | Explicit target: `ps5`, `ps5-slim`, or `ps5-pro` |
| `firmware` | Explicit firmware version, e.g. `13.60`. This is **metadata**, not validation of upstream support |
| `loader` | Currently must be `ps5-linux-loader`; purely declarative, not an installed-loader check |

Unknown and duplicate keys are rejected. Comments start with `#` on their own line. A manifest's maximum size is 64 KiB. Do not use wildcards to declare model/firmware compatibility.

## Example (not a bootable image)

```
format=astraps5-boot-v1
image=astra-system.img
sha256=YOUR_ACTUAL_64_CHARACTER_SHA256
console=ps5
firmware=13.60
loader=ps5-linux-loader
```

The example above is illustrative only: replace the checksum placeholder with the **actual** SHA-256. An image is only trusted when it matches the hash you intentionally put into the manifest. A matching checksum alone does not establish that an image came from a trustworthy source.

## Check

```bash
sha256sum /path/to/astra-system.img
# Edit your manifest with this hash, then:
./build/astra boot verify /path/to/astraps5.boot
./build/astra boot verify /path/to/astraps5.boot --json
```

A successful result only means *the file's SHA-256 matches the manifest*. The tool returns exit code `0` on a match, `1` on validation failure, and `2` for malformed command-line arguments. An explicit `console_checked=false` and `boot_supported=false` are returned in JSON to avoid confusing an image-integrity test with a proven PS5 boot.

## Boot handoff (future)

Hardware-specific steps, firmware detection, loader API integration, safer recovery, and image signatures will need separate work and real PS5 testing. `astra boot verify` neither initiates a handoff nor checks whether the supplied metadata is a valid PS5 combination.

See [app-first-boot.md](app-first-boot.md) for the actual long-term boot experience.
