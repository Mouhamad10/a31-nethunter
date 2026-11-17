# a31-nethunter

Kali NetHunter for the **Samsung Galaxy A31 (SM-A315F, MediaTek MT6768)** — kernel feature set with
**wifi monitor + packet injection**, external-adapter drivers (**RTL8187L**, **RTL8822BU**, + others),
USB-gadget arsenal (BadUSB/HID, DriveDroid), and a lightweight GSI ROM to run it on 4 GB.

> Status: **engineering base, not yet a finished flashable** — the kernel is built on the booting 4.14
> tree and validated one flash at a time. Every claim is kept honest: nothing is "working" until it's
> confirmed on a real A31 with evidence.

## Why this approach
- Builds on the **only kernel that currently boots** — `SN-Abdullah-Al-Noman/SM-A315F` (4.14 "Atrocious"),
  keeping its dtb/dtbo + stock One UI 12 vendor. The custom 4.19 kernel does **not** boot yet, so nothing
  here waits on it.
- Reuses **`Vexicle/samsung_kernel_a325f`** — the Galaxy A32 4G, **same MT6768, same 4.14.x**, which
  already ships KernelSU + Kali NetHunter wifi-injection patches. That diff is ~80% of the kernel work,
  pre-proven on the same SoC.
- **External USB adapters are the real attack path** — the internal MT6768 radio has no confirmed
  injection. RTL8187L is the dependable lead adapter.

## Contents
| Path | What |
|---|---|
| [`kernel/a315f_nethunter_defconfig.fragment`](kernel/a315f_nethunter_defconfig.fragment) | NetHunter + RTL + USB-gadget + container CONFIGs to merge onto `a31_defconfig` |
| [`kernel/rtl-integration.md`](kernel/rtl-integration.md) | RTL8187L (in-kernel) + RTL8822BU (out-of-tree 88x2bu) integration |
| [`rom/README.md`](rom/README.md) | AndyYan LineageOS Light A13 GSI + 4 GB tuning |
| [`docs/ROADMAP.md`](docs/ROADMAP.md) | dependency-ordered 8-phase plan + blockers |
| [`docs/RUNBOOK.md`](docs/RUNBOOK.md) | flash/restore, KNOX/vbmeta, brick recovery |
| [`.github/workflows/build-kernel.yml`](.github/workflows/build-kernel.yml) | CI: clone tree → merge fragment → build `Image.gz` |

## Build the kernel
Actions → **Build A31 NetHunter kernel** → Run workflow. It clones the 4.14 tree, merges the fragment,
and builds `Image.gz` (artifact `a31-nethunter-kernel`, with `build.log` + the merged `.config`).

## Related repos
- [`a31-booster`](https://github.com/Mouhamad10/a31-booster) — the Magisk perf + blur module (shipped).
- [`oneos-a31`](https://github.com/Mouhamad10/oneos-a31) — OneOS (run a GSI next to One UI, no flashing).

## ⚠️ Read first
See [`docs/ROADMAP.md`](docs/ROADMAP.md) blockers: KNOX e-fuse is permanent, A31 root needs vbmeta
disabled (brick-loop risk), KernelSU must be integrated (not LKM), HID gadget may hit the `-22` bug.
Authorized / own-device security testing only.

## License
GPL-2.0 (kernel-oriented). The forked kernel source keeps its upstream GPL-2.0.
