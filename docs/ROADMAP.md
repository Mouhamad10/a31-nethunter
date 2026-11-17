# A31 super-project roadmap (dependency-ordered)

Everything ships on the **booting 4.14 "Atrocious" tree** (`SN-Abdullah-Al-Noman/SM-A315F`) + its
untouched dtb/dtbo + stock One UI 12 vendor, pinned to firmware **A315FXXS5DXB1**. Nothing shippable
waits on the non-booting 4.19 kernel.

## Phases
- **P0 — Foundation (off-device, now).** Fork the 4.14 Atrocious tree as the single integration base.
  Diff it against **`Vexicle/samsung_kernel_a325f`** (A32 4G, SAME MT6768, SAME 4.14.x) — it already
  carries KernelSU, Kali NetHunter wifi-injection patches, overclock, fpsgo_v3, KVM. That diff is ~80%
  of the kernel work, pre-proven on the same SoC.
- **P1 — Magisk perf+aesthetics module (ship FIRST, no custom kernel).** → shipped as
  [`a31-booster`](https://github.com/Mouhamad10/a31-booster). On-demand max CPU/GPU + QS blur props.
- **P2 — Rooted 4.14 kernel.** Cherry-pick Vexicle's **integrated/GKI KernelSU** (NOT LKM — LKM fails
  on KNOX). A31 installs root to the **recovery** partition and needs vbmeta disabled. One flash to validate.
- **P3 — NetHunter kernel (parallel to P2).** Apply [`a315f_nethunter_defconfig.fragment`](../kernel/a315f_nethunter_defconfig.fragment)
  + [RTL drivers](../kernel/rtl-integration.md) + USB-gadget. Build AnyKernel3 zip + kalifs installer.
  External-adapter injection (RTL8187L/8822BU/8812AU) is the dependable path — ship before internal wifi.
- **P4 — Internal MT6768 wifi (best-effort).** Swap in `rsuntk mtk_connectivity_module` (staging-4.14) +
  physwizz method-2 (strip FM/GPS from the connectivity Makefile). Restores plain wifi on the custom
  kernel; internal monitor/injection is UNCONFIRMED on MT6768 — keep the external adapter as the real path.
- **P5 — Lightweight ROM (GSI).** Flash **AndyYan "LineageOS Light" A13, arm64, A-only, vanilla** on
  stock vendor + custom kernel. A13 avoids needing a permissive kernel. See [rom/README.md](../rom/README.md).
- **P6 — OneOS guest / DSU (no-flash).** OneOS ([`oneos-a31`](https://github.com/Mouhamad10/oneos-a31))
  runs a GSI as a container; the public no-flash precedent is **DSU Loader** (Dynamic System Update).
  Needs the container-prereq configs (namespaces/cgroups/overlayfs/binderfs/TUN — in the fragment) and
  an unlocked bootloader. A14+/16/17 GSIs need SELinux-permissive → routes through P7.
- **P7 — Kernel modernization (parallel, never blocks).** Fix 4.19 (likely dtb/dtbo/vendor) or jump to
  5.10 GKI — the only path to newer-GSI vendor patches at the Treble level.

## Hard blockers (read before any flash)
- **4.19 kernel does not boot** (dtb/dtbo/vendor mismatch). Don't gate anything shippable on it.
- **KNOX e-fuse is permanent** on bootloader unlock (0x1): kills Samsung Pay/Secure Folder/Health/some
  banking+DRM, wipes data. Irreversible — accept once up front.
- **A31 root → recovery partition + vbmeta disabled**; getting it wrong gives the documented
  download-mode/vbmeta boot loop. Keep Odin stock firmware + a backup before every flash.
- **KernelSU LKM fails on KNOX** — use integrated/GKI KernelSU or KernelSU-Next.
- **HID gadget (-22)** often fails even with `CONFIG_USB_CONFIGFS_F_HID=y` — BadUSB may need an `f_hid`
  code port; test HID early on-device.
- **Internal MT6768 wifi has no confirmed injection** — external USB adapter only.
- **QS blur from SF props alone won't render on stock One UI SystemUI** — needs Iconify / RRO overlay
  (fragile, re-test after each One UI OTA). The genuinely "lighter than One UI" result is the GSI (P5).
- **4GB RAM** is tight for host + NetHunter chroot + guest at once — expect to trade background apps/blur
  for guest stability.

## Suggested repo split (this repo can later fan out)
`a31-kernel-4.14` (fork), `a31-kernel-5.10-gki`, `a31-magisk-performance` (shipped as a31-booster),
`a31-nethunter` (this repo: AnyKernel3 + kalifs + device entry), `a31-rom-gsi`, `a31-oneos-guest`.
