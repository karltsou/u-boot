/*
 * (C) Copyright 2018 Rockchip Electronics Co., Ltd
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <debug_uart.h>
#include <dm.h>
#include <ram.h>
#include <spl.h>
#include <asm/arch/bootrom.h>
#include <asm/arch-rockchip/sys_proto.h>
#include <asm/io.h>

DECLARE_GLOBAL_DATA_PTR;

#define BROM_BOOTSOURCE_ID_ADDR (CONFIG_ROCKCHIP_IRAM_START_ADDR + 0x10)
void board_return_to_bootrom(void)
{
	back_to_bootrom(BROM_BOOT_NEXTSTAGE);
}

__weak const char * const boot_devices[BROM_LAST_BOOTSOURCE + 1] = {
};

const char *board_spl_was_booted_from(void)
{
	u32  bootdevice_brom_id = readl(BROM_BOOTSOURCE_ID_ADDR);
	const char *bootdevice_ofpath = NULL;

	if (bootdevice_brom_id < ARRAY_SIZE(boot_devices))
		bootdevice_ofpath = boot_devices[bootdevice_brom_id];

	if (bootdevice_ofpath)
		debug("%s: brom_bootdevice_id %x maps to '%s'\n",
		      __func__, bootdevice_brom_id, bootdevice_ofpath);
	else
		debug("%s: failed to resolve brom_bootdevice_id %x\n",
		      __func__, bootdevice_brom_id);

	return bootdevice_ofpath;
}

u32 spl_boot_device(void)
{
	u32 boot_device = BOOT_DEVICE_MMC1;

#if defined(CONFIG_TARGET_CHROMEBOOK_JERRY) || \
		defined(CONFIG_TARGET_CHROMEBIT_MICKEY) || \
		defined(CONFIG_TARGET_CHROMEBOOK_MINNIE)
	return BOOT_DEVICE_SPI;
#endif
	if (CONFIG_IS_ENABLED(ROCKCHIP_BACK_TO_BROM))
		return BOOT_DEVICE_BOOTROM;

	return boot_device;
}

u32 spl_boot_mode(const u32 boot_device)
{
	return MMCSD_MODE_RAW;
}

__weak void rockchip_stimer_init(void)
{
#ifndef CONFIG_ARM64
	asm volatile("mcr p15, 0, %0, c14, c0, 0"
		     : : "r"(COUNTER_FREQUENCY));
#endif
	writel(0, CONFIG_ROCKCHIP_STIMER_BASE + 0x10);
	writel(0xffffffff, CONFIG_ROCKCHIP_STIMER_BASE);
	writel(0xffffffff, CONFIG_ROCKCHIP_STIMER_BASE + 4);
	writel(1, CONFIG_ROCKCHIP_STIMER_BASE + 0x10);
}

__weak int arch_cpu_init(void)
{
	return 0;
}

__weak int rk_board_init_f(void)
{
	return 0;
}

void board_init_f(ulong dummy)
{
#ifdef CONFIG_SPL_FRAMEWORK
	int ret;
#if !defined(CONFIG_SUPPORT_TPL)
	struct udevice *dev;
#endif
#endif

#if !defined(CONFIG_SUPPORT_TPL)
	rockchip_stimer_init();
	arch_cpu_init();
#endif
#define EARLY_UART
#if defined(EARLY_UART) && defined(CONFIG_DEBUG_UART)
	/*
	 * Debug UART can be used from here if required:
	 *
	 * debug_uart_init();
	 * printch('a');
	 * printhex8(0x1234);
	 * printascii("string");
	 */
	debug_uart_init();
	printascii("U-Boot SPL board init");
#endif

#ifdef CONFIG_SPL_FRAMEWORK
	ret = spl_early_init();
	if (ret) {
		printf("spl_early_init() failed: %d\n", ret);
		hang();
	}
#if !defined(CONFIG_SUPPORT_TPL)
	debug("\nspl:init dram\n");
	ret = uclass_get_device(UCLASS_RAM, 0, &dev);
	if (ret) {
		printf("DRAM init failed: %d\n", ret);
		return;
	}
#endif
	preloader_console_init();
#else
	/* Some SoCs like rk3036 does not use any frame work */
	sdram_init();
#endif

	rk_board_init_f();
#if CONFIG_IS_ENABLED(ROCKCHIP_BACK_TO_BROM) && !defined(CONFIG_SPL_BOARD_INIT)
	back_to_bootrom(BROM_BOOT_NEXTSTAGE);
#endif

}

#ifdef CONFIG_SPL_LOAD_FIT
int board_fit_config_name_match(const char *name)
{
	/* Just empty function now - can't decide what to choose */
	debug("%s: %s\n", __func__, name);

	return 0;
}
#endif

#ifdef CONFIG_SPL_BOARD_INIT
__weak int rk_spl_board_init(void)
{
	return 0;
}

static int setup_led(void)
{
#ifdef CONFIG_SPL_LED
	struct udevice *dev;
	char *led_name;
	int ret;

	led_name = fdtdec_get_config_string(gd->fdt_blob, "u-boot,boot-led");
	if (!led_name)
		return 0;
	ret = led_get_by_label(led_name, &dev);
	if (ret) {
		debug("%s: get=%d\n", __func__, ret);
		return ret;
	}
	ret = led_set_on(dev, 1);
	if (ret)
		return ret;
#endif

	return 0;
}

void spl_board_init(void)
{
	int ret;

	ret = setup_led();

	if (ret) {
		debug("LED ret=%d\n", ret);
		hang();
	}

	rk_spl_board_init();
#if CONFIG_IS_ENABLED(ROCKCHIP_BACK_TO_BROM)
	back_to_bootrom(BROM_BOOT_NEXTSTAGE);
#endif
	return;
}
#endif
