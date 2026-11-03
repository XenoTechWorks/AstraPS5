# Nonbootable test bundle

These files intentionally contain only the five ASCII bytes `hello`.
A matching SHA-256 hash must never be mistaken for a bootable kernel,
initramfs, root filesystem, a signed release, or PS5 hardware support.
The fixture exists solely to exercise `astra boot inspect` and `astra boot plan`.
