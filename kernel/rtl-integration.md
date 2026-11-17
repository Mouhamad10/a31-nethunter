# RTL USB wifi adapters — monitor + injection on the A31

The A31's **internal** MT6768 (connac/gen4m) radio has **no confirmed monitor/injection**. All real
wifi attacks go through an **external USB adapter over a powered OTG hub**. Two adapters are targeted
here (yours): **RTL8187L** and **RTL8822BU**.

## RTL8187L — primary, ship first (reliable)
- Driver: **in-kernel `CONFIG_RTL8187`** (`drivers/net/wireless/realtek/rtl818x/rtl8187/`), pure
  mac80211, **no firmware blob** (firmware is in the chip ROM).
- Monitor works out of the box; injection validated with `aireplay-ng --test`. 2.4 GHz b/g only (no 5 GHz).
- The only trap on MTK defconfigs is the vendor gate: **`CONFIG_WLAN_VENDOR_REALTEK=y`** (else RTL8187 is hidden).
- Already enabled in [`a315f_nethunter_defconfig.fragment`](a315f_nethunter_defconfig.fragment).

## RTL8822BU — your second adapter (weak injector, module)
- Do **NOT** use in-kernel `rtw88` (`rtw_8822bu`): USB support only landed ~kernel **6.2** — it does
  not exist on 4.14/4.19/5.10, and even on 6.x its monitor/injection is buggy.
- Use the **out-of-tree `morrownr/88x2bu-20210702`** (fallback `RinCat/RTL88x2BU`). Build as a module:
  - Makefile: `CONFIG_PLATFORM_I386_PC=n`, `CONFIG_PLATFORM_AUTODETECT=n`, `ARCH:=arm64`,
    `CROSS_COMPILE:=<prefix>`, `KSRC:=<SM-A315F 4.14 tree>`, keep `CONFIG_WIFI_MONITOR=y`.
  - Firmware is baked into the `.ko` (no `/lib/firmware`).
  - Build against the **EXACT** flashed kernel source + `.config` + toolchain (vermagic/Module.symvers)
    or `insmod` fails. The CI pins the module build to the same commit/.config as the shipped boot.img.
- Caveat: RTL8822BU is a **weak injection chip** (morrownr marks it "not recommended"). For stronger
  injection prefer **RTL8812AU/8814AU** or MediaTek **mt76** (MT7921AU/MT7612U).

## Inline integration (8822bu + 8812au + 8188eus)
Use **`kimocoder/nethunter_wireless_modules`**: drop `rtl8812au/`, `rtl8822bu/`, `rtl8188eus/` into
`drivers/net/wireless/realtek/`, add `source` lines to that `Kconfig` and `obj-$(CONFIG_...)` lines to
the `Makefile`. **Rule:** only ONE of `CONFIG_88XXAU` (8812au) / `CONFIG_RTL8822BU` may be `=y` inline
at a time; build the other and `rtl8188eus` (always) as `=m`.

## One-pass on-device validation (the whole wifi test budget)
1. plug adapter via powered OTG hub → `iw dev` enumerates it
2. `airmon-ng start wlanX` → monitor
3. `aireplay-ng --test` → injection

> Power note: a single USB-C port can't be gadget (HID/BadUSB) AND host (wifi) at once — use a
> **powered** OTG hub / Y-cable. OTG alone gives ~450 mA; RTL8187L/Alfa draw ~500 mA and can brown out under TX.
