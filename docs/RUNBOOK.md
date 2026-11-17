# A31 flash + restore runbook (one on-device pass per phase)

Goal: every phase is produced off-device so each on-device session is a **single flash**, honouring the
minimize-testing constraint.

## Before ANY flash
- Keep **Odin stock firmware** (A315FXXS5DXB1) on the PC.
- Back up current `boot`, `recovery`, `vbmeta` to the PC (`dd` via root, or Odin read).
- Charge > 60%.

## KNOX / bootloader (one-time, irreversible)
Unlocking the bootloader trips the **KNOX e-fuse (0x1) permanently**: Samsung Pay / Secure Folder /
Health / some banking + DRM stop working, and data is wiped. Accept once, up front.

## Root install quirk (A31)
Root/KernelSU effectively installs via the **recovery** partition and needs **vbmeta disabled**
(`--disable-verity --disable-verification`). Getting this wrong produces the documented
**download-mode / vbmeta boot loop**. If it loops:
1. Download mode (Vol Down + Power ~7 s, then Vol Up + Vol Down, plug USB).
2. Odin **AP** = a `.tar` of the backed-up stock `recovery`/`vbmeta`/`boot`.

## Per-phase validation (keep to ONE pass each)
- **Rooted kernel (P2):** flash boot → confirm boots + `su` works → done.
- **NetHunter (P3):** flash boot (NetHunter defconfig) + AnyKernel3 → plug external adapter via powered
  OTG hub → `iw dev` → `airmon-ng` → `aireplay-ng --test`. HID: test BadUSB early (the `-22` bug).
- **GSI (P5):** fastboot flash system + vbmeta → first boot → apply 4 GB tuning props.

## Recovery of last resort
Odin flash the full stock firmware (A315FXXS5DXB1) = back to stock One UI. Keep it downloaded.
