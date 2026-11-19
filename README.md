# A31 NetHunter Kernel — Samsung Galaxy A31 (SM-A315F / MT6768)

A custom **4.14 kernel** that turns a Samsung Galaxy A31 running an Android GSI into a
**Kali-NetHunter-capable** device: external USB Wi-Fi adapters work in **monitor mode + packet
injection** on 2.4 GHz and 5 GHz, the external microSD mounts reliably, and the flaky LTE modem
is tamed. Everything is added **on top of the device's existing booting kernel** so the installed
GSI keeps working — no ROM change required, just one `fastboot flash boot`.

> This README is written to be self-contained: a person (or an AI assistant) with no prior context
> should be able to read it top to bottom and **rebuild, extend, and use** this project from scratch.

---

## 1. Device & environment (the exact target)

| Item | Value |
|---|---|
| Phone | Samsung Galaxy A31, model **SM-A315F** |
| SoC | **MediaTek MT6768** (Helio P65), arm64, 4 GB RAM |
| Base kernel source | **Atrocious Kernel V6, Linux 4.14.186** — `github.com/SN-Abdullah-Al-Noman/SM-A315F` (branch used: `master`) |
| Toolchain | **zyc-clang-14** (`github.com/karamdev1/zyc-clang-14-20260619`) + GNU `binutils` aarch64 (`as`/`ld`) |
| Running OS | an **Android GSI** (system-only image; build props spoof `SM-S901B`), rooted with **Magisk**, SELinux **permissive** |
| Boot partition size | **32 MiB** (33,554,432 bytes) — hard limit for the flashed image |
| Display / touch | smcdsd panel `ea8076`, Imagis `IST4050` touch |
| Wi-Fi adapters tested | **RTL8187** "Signal Max" (2.4 GHz) `0bda:8187`; **RTL8822BU** TP-Link Archer T4U (2.4+5 GHz) `2357:0115` |

Why "on top of the existing kernel": the running GSI depends on patches already present in the
Atrocious kernel (e.g. an eBPF backport). Building a *different* kernel base breaks the GSI, so this
project **reuses the exact Atrocious source + the device's own ramdisk/dtb** and only swaps the
compiled `Image.gz`.

---

## 2. What this kernel adds (feature list)

1. **Full 802.11 SoftMAC stack** — `MAC80211` + `CFG80211_WEXT` (both were *absent* from the stock
   Atrocious config, which is why NetHunter did not work before).
2. **RTL8187 driver** (in mainline tree) — the Signal Max adapter. Monitor mode via WEXT.
3. **RTL8822BU driver** (out-of-tree `morrownr/88x2bu-20210702`, **integrated into the kernel tree**)
   — the Archer T4U. Monitor mode via **nl80211** (not WEXT — see §6).
4. **Broad in-tree USB Wi-Fi coverage** so most common adapters "just work": `RTL8XXXU`,
   `ATH9K_HTC`, `CARL9170`, `RT2800USB` family, `MT7601U`, `ZD1211RW`.
5. **External microSD fix** — a one-line patch to the Samsung `sdfat` driver so the GSI's `vold`
   can mount exFAT cards (see §5).
6. **Bluetooth USB + USB-gadget HID** (NetHunter BadUSB / HID attacks) kept enabled.
7. Ships two tiny helper tools (`setmon`, `setmon_nl`) to toggle monitor mode without needing
   `iw`/`iwconfig` on the device.

Companion artifacts (separate, documented here for the full picture):
- **`a31_3gplus`** Magisk module — locks the radio to 3G+/HSPA+ because the A31 modem resets on LTE
  (port-level firmware issue). See §7.
- **`a31_sdcard`** Magisk module — userspace fallback that direct-mounts the microSD if `vold` still
  misses it. See §5.

---

## 3. How the kernel is built (CI pipeline)

Everything is built by GitHub Actions: **`.github/workflows/build-kernel.yml`** (`workflow_dispatch`).
Steps, in order:

1. **Install deps** — `gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu lld bc bison flex libssl-dev …`
2. **Clone Atrocious** kernel into `kernel/`.
3. **Patch sdfat** for the SD fix (`kernel/patch_sdfat.py`, see §5).
4. **Integrate RTL8822BU** — clone `morrownr/88x2bu-20210702` into
   `drivers/net/wireless/realtek/rtl8822bu/`, append `obj-$(CONFIG_RTL8822BU) += rtl8822bu/` to the
   realtek `Makefile`, `source` its `Kconfig`, and apply the 4.14 source-compat patch (see §6).
5. **Fetch toolchain** — clone zyc-clang-14 and symlink it to the *exact* hardcoded path the
   Atrocious Makefile expects: `../toolchain/clang/host/linux-x86/clang-r383902`. (With `O=out`,
   `make` runs from `kernel/out`, so the symlink is created under every candidate base.)
6. **Seed defconfig** — copy the device's **running config** (`kernel/atrocious_v6_running.config`,
   extracted from the phone's live `/proc/config.gz`) to `arch/arm64/configs/a315f_nethunter_defconfig`
   and **append** `kernel/a315f_nethunter_defconfig.fragment` (the NetHunter options).
   > Important lesson: the first attempts used `merge_config.sh -m -O …`, whose output `.config` is
   > **never read** by `make <name>_defconfig`. The fragment was silently dropped and the wifi stack
   > was missing. Appending straight to the defconfig fixed it.
7. **Build `Image.gz`** with this invocation (each flag matters — see §4):
   ```
   make O=out ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CLANG_TRIPLE=aarch64-linux-gnu- \
        "CC=clang -B$HOME/crosbin" "KCFLAGS=-w -fno-builtin-stpcpy" KCPPFLAGS=-w \
        HOSTCC=gcc HOSTCXX=g++
   ```
   plus `scripts/config` force-enables of the whole wifi list, then `olddefconfig`, then an
   **in-CI assert** that `CONFIG_MAC80211=y`, `CONFIG_RTL8187=y`, `CONFIG_CFG80211_WEXT=y`,
   `CONFIG_RTL8822BU=y` are really in the final config (fail fast, never ship a silently-broken kernel),
   then `make -j Image.gz`, then a check that `Image.gz` actually exists (the `… | tee | tail` pipe
   hides make's exit code, so this guard is required).
8. **Upload artifact** — `Image.gz` + `build.log` + `kernel.config`.

To build: Actions → "Build A31 NetHunter kernel" → *Run workflow*. Download the artifact when green.

---

## 4. Toolchain gotchas (why each build flag exists)

Compiling a 4.14 kernel (written for clang-11 / `clang-r383902`) with **clang-14** needed these:

- **`CC=clang -B$HOME/crosbin`** — clang's integrated assembler chokes on the 4.14 VDSO macros, and
  zyc's bundled `as` is LLVM-based (rejects GNU `-EL`). The fix: a dir `~/crosbin` containing
  `as → aarch64-linux-gnu-as` and `ld → aarch64-linux-gnu-ld`, pointed to with `-B`, so clang calls
  the **GNU** assembler/linker for the target only. Host tools stay x86 via `HOSTCC=gcc`.
- **`KCFLAGS=-w` / `KCPPFLAGS=-w`** — clang-14 emits many new warnings; the kernel's `-Werror` turns
  them fatal. `-w` silences warnings (the same approach the proven "karam" build uses).
- **`-fno-builtin-stpcpy`** — clang-14 rewrites some `strcpy()` into `stpcpy()`, which 4.14 does not
  provide → `undefined reference to stpcpy` at the vmlinux link. This flag stops the rewrite.
- **Disable `CC_STACKPROTECTOR_STRONG`** — clang-14 fails the 4.14 `-fstack-protector-strong` probe;
  dropping it lets the compiler check pass (fine for this use).

---

## 5. The microSD fix (sdfat `time_offset`)

**Symptom:** external exFAT microSD shows as *unmountable*; `sm list-volumes` → `public:… unmountable`.

**Root cause:** the GSI's modern `vold` mounts the card with the option **`time_offset=<minutes>`**.
The old Samsung **sdfat** driver's option parser doesn't know `time_offset` and rejects the whole
mount with `-EINVAL` (`unrecognized mount option`). The kernel + card are fine — only the option is.

**Fix (`kernel/patch_sdfat.py`):** add `time_offset` to `fs/sdfat/sdfat.c`'s option enum + token table
+ a `case Opt_time_offset:` that parses the int and ignores it (sdfat keeps local-time stamps, which
is harmless). After this, `vold` mounts the card natively and it appears everywhere like a normal phone.

**Userspace fallback (`a31_sdcard` Magisk module):** if `vold` still misses it, the module's
`service.sh` direct-mounts the card with the kernel `sdfat` driver to `/mnt/media_rw/<UUID>` and
bind-mounts it into `/mnt/runtime/*` + `/storage/<UUID>` so apps can see it. (Note: a raw bind gives
root/file-manager access but **not** full MediaStore visibility — that needs the `vold` path above,
i.e. the kernel patch.)

---

## 6. The RTL8822BU integration (Archer T4U) — the full story

The 8822BU driver is **not in the 4.14 tree**. Steps that made it work:

1. **Source:** `morrownr/88x2bu-20210702` (its USB id table already lists `2357:0115` = Archer T4U).
   `CONFIG_WIFI_MONITOR = y` is the default in its Makefile, so **monitor support is compiled in**.
2. **In-tree wiring:** copy to `drivers/net/wireless/realtek/rtl8822bu/`, add
   `obj-$(CONFIG_RTL8822BU) += rtl8822bu/` to `drivers/net/wireless/realtek/Makefile`, add
   `source ".../rtl8822bu/Kconfig"`, set `CONFIG_RTL8822BU=y`. The driver's own Makefile has the
   in-tree (`KERNELRELEASE`) branch, so Kbuild compiles its ~209 source files into the image.
3. **4.14 source-compat patch (the one real build error):**
   `os_dep/linux/usb_intf.c` declares `extern int console_suspend_enabled;` but 4.14 declares it as
   **`bool`** → `redeclaration … 'int' vs 'bool'`. The CI `sed`-patches that `int` → `bool`. After
   that single fix the whole driver compiled clean.
4. **Monitor mode — the key lesson:** this driver **rejects WEXT** (`SIOCSIWMODE` → *Operation not
   permitted*). Do **not** use `iwconfig`-style monitor on it. Use **nl80211**
   (`iw dev wlanX set type monitor`, or the bundled `setmon_nl`). With nl80211 the interface switches
   to radiotap monitor (`/sys/class/net/wlanX/type` → `803`) and injection works on 2.4 + 5 GHz.

The 8822BU driver adds ~0.9 MB to `Image.gz` (16.2 → 17.1 MB); the final boot image is ~17.8 MiB,
well under the 32 MiB partition (≈14 MiB headroom).

---

## 7. The LTE modem problem & 3G+ lock (`a31_3gplus`)

The A31 modem **crashes/resets on LTE every few minutes** (kernel log shows `md_state change 2→3→4`
modem reboots, plus `EMI MPU violation` floods). This is a **port/firmware-level** issue (the
`a31_lte_guard` module existed precisely for this), not caused by this kernel. VoLTE off did not help.

**Pragmatic fix:** lock the radio to **3G+/HSPA+ (no LTE)** so the modem stays stable. The
`a31_3gplus` Magisk module applies, on every boot, the allowed-network-types bitmask
`1100001110000111` = `GPRS|EDGE|UMTS|HSDPA|HSUPA|HSPA|HSPA+|GSM` (3G + 3G+ with GSM fallback, **no
LTE**) via `cmd phone set-allowed-network-types-for-users -s 0 <mask>`, and re-applies for 10 min
after boot to beat the modem reverting it. Delete the module to allow LTE again.

> True stable 4G on this port would need correct modem firmware (`md1img` / NON-HLOS) matched to the
> DT/EMI map — out of scope here.

---

## 8. How to flash & use

**Flash** (boot image is produced by repacking the device's `boot_current_full.img` with the new
`Image.gz`, then stripping the trailing zero-padding so it fits the 32 MiB partition):
```
adb reboot bootloader
fastboot flash boot boot_nethunter.img
fastboot reboot
```
Keep a backup of the original boot partition first. (TWRP → *Install Image → Boot* also works.)

**Enable monitor mode:**
```
# RTL8187 (Signal Max) — WEXT works:
su -c 'ip link set wlan1 down; /data/local/tmp/setmon wlan1 monitor; ip link set wlan1 up'

# RTL8822BU (Archer T4U) — must use nl80211:
su -c 'ip link set wlan1 down; /data/local/tmp/setmon_nl wlan1 monitor; ip link set wlan1 up'
```
Verify: `cat /sys/class/net/wlan1/type` → `803` (radiotap monitor). Return to normal with the
`managed` argument.

**Host-mode / power:** plug the adapter through a **powered USB-OTG hub** (external power bank). The
phone enters USB host mode automatically; `wlan1` + a new `phyN` appear once the driver binds.

The helper tools (`tools/setmon.c` WEXT, `tools/setmon_nl.c` nl80211) are tiny static aarch64 C
programs — build with `aarch64-linux-gnu-gcc -static -O2 -o setmon tools/setmon.c`.

---

## 9. Repository layout

```
.github/workflows/build-kernel.yml   # the whole CI build
kernel/
  a315f_nethunter_defconfig.fragment  # NetHunter kernel options appended to the running config
  atrocious_v6_running.config         # the device's live config (build base)
  patch_sdfat.py                      # sdfat time_offset patch (SD fix)
  rtl-integration.md                  # notes on the realtek driver wiring
tools/
  setmon.c                            # set monitor via WEXT (RTL8187)
  setmon_nl.c                         # set monitor via nl80211 (RTL8822BU)
```

---

## 10. Future development

- **Kali chroot:** install the NetHunter app + a light `kalifs-arm64-nano` (~150 MB) rootfs to get
  `airmon-ng` / `airodump-ng` / `aireplay-ng` / `aircrack-ng` for real capture, injection and
  WPA-handshake work. Tip: download the rootfs on a PC and `adb push` it (slow mobile data).
- **More adapters:** the in-tree drivers already cover many chips; for strong 5 GHz monitor an
  `mt7612u` / `rtl8812au` is also an option.
- **Rebuild on a newer Atrocious commit:** keep the fragment + patches; re-run the workflow.
- **Internal branding** (planned, not yet applied): stamp the kernel/vendor identity string and
  display name as requested, visible in Settings → About and in a bootloop banner.

---

## 11. Reproduce from zero (checklist for a future maintainer / AI)

1. Pull the device's live config → commit as `kernel/atrocious_v6_running.config`.
2. Keep `a315f_nethunter_defconfig.fragment` (the wifi/gadget options) and `patch_sdfat.py`.
3. Ensure `build-kernel.yml` still: appends the fragment to the defconfig, patches sdfat, integrates
   `88x2bu` (+ the `console_suspend_enabled` int→bool patch), uses the clang-14 flags in §4, and
   asserts the wifi configs before compiling.
4. Run the workflow → download `Image.gz`.
5. Repack: take the device's current `boot.img`, swap the kernel, **strip trailing zero padding** so
   the result ≤ 32 MiB, flash.
6. On device: plug an adapter through a powered OTG hub → `wlan1` appears → set monitor via the right
   interface (WEXT for 8187, **nl80211 for 8822BU**).

That is the entire project.
