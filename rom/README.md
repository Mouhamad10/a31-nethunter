# Lightweight ROM for the A31 (4 GB) — GSI path

There is **no maintained device-native custom ROM** for the A31, so the lightweight path is a **Treble
GSI** on the stock vendor + the custom 4.14 kernel.

## Recommended image
**AndyYan "LineageOS Light" GSI** — arm64, **A-only**, **vanilla** (no-GApps, `arm64_avN`), **Android 13**.
- Lightest maintained Treble-correct option; One UI's feature level without One UI's RAM cost.
- **Stay on A13**: it boots on the existing 4.14 vendor+kernel WITHOUT an SELinux-permissive patch
  (A14 QPR2+/15/16/17 GSIs need permissive → defer to the P7 kernel track).
- Confirm the Treble profile with **Treble Info** first (A31 launched on A10 → A-only / system-as-root /
  dynamic partitions; if it reports **VNDKLite**, use the `-lite` build).
- `crDroid` GSI is a heavier fallback only if you insist on built-in blur/customization. Avoid
  PixelExperience (discontinued).

> This GSI — not patched stock One UI — is where the genuinely "lighter than One UI" look realistically lands.

## Flash (one pass)
```
# decompress
unxz lineage-*-arm64_avN.img.xz
# fastbootd
fastboot reboot fastboot
fastboot flash system lineage-*-arm64_avN.img
# patched/empty vbmeta (disable verity/verification)
fastboot --disable-verity --disable-verification flash vbmeta vbmeta.img
fastboot -w            # format data on first boot
fastboot reboot
```

## 4 GB tuning (apply after first boot)
- zram: **~2.5–3 GB, zstd**
- `vm.swappiness` ≈ **130**
- **PSI-based lmkd**, tuned for headroom (kernel has `CONFIG_PSI=y`)
- Props: `persist.sys.phh.mtk_ged_kpi=1` + TrebleDroid telephony toggles (VoLTE/IMS is a known MTK-GSI
  weak spot)
- vanilla / microG (biggest RAM win), background process limit **3–4**, dexopt **speed-profile**
- **Do NOT** set `ro.config.low_ram=true`

> Image is not redistributed here — download from AndyYan's SourceForge and verify its hash.
