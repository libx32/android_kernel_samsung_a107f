/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define LOG_TAG "LCM"

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
#include "disp_dts_gpio.h"
#endif

#ifndef MACH_FPGA
#include <lcm_pmic.h>
#endif

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

#define LCM_ID (0x78)

static const unsigned int BL_MIN_LEVEL = 20;
static struct LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))
#define MDELAY(n)		(lcm_util.mdelay(n))
#define UDELAY(n)		(lcm_util.udelay(n))


#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
		lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
	  lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
/* #include <linux/jiffies.h> */
/* #include <linux/delay.h> */
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#endif

/* static unsigned char lcd_id_pins_value = 0xFF; */
static const unsigned char LCD_MODULE_ID = 0x01;
#define LCM_DSI_CMD_MODE									0
#define FRAME_WIDTH										(720)
#define FRAME_HEIGHT										(1600)

#define LCM_PHYSICAL_WIDTH									(68260)
#define LCM_PHYSICAL_HEIGHT									(151680)


#define REGFLAG_DELAY		0xFFFC
#define REGFLAG_UDELAY		0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD
#define REGFLAG_RESET_LOW	0xFFFE
#define REGFLAG_RESET_HIGH	0xFFFF

static struct LCM_DSI_MODE_SWITCH_CMD lcm_switch_mode_cmd;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

extern void ili_resume_by_ddi(void);

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{ 0xFF, 0x03, {0x78, 0x07, 0x00} },
	{0x28, 0, {} },
	{REGFLAG_DELAY, 20, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} }

};

//static struct LCM_setting_table init_setting_cmd[] = {
//	{ 0xFF, 0x03, {0x78, 0x07, 0x03} },
//};

static struct LCM_setting_table init_setting_vdo[] = {
//平台AVDD=5.8V,AVEE=-5.8V
//平台Vsync=4,Vbp=8,Vfp=32,Hsync=12,Hbp=116,Hbp=112,CLK=720Mbps
//GIP timing
{0xFF,0x03,{0x78,0x07,0x01}},		//page1

{0x00,0x01,{0x43}},
{0x01,0x01,{0xe2}},
{0x02,0x01,{0x4A}},   //FTI rising
{0x03,0x01,{0x4E}},   //FTI falling
{0x04,0x01,{0x03}},
{0x05,0x01,{0x41}},
{0x06,0x01,{0x03}},
{0x07,0x01,{0x9B}},
{0x9a,0x01,{0x00}},
{0x9b,0x01,{0x00}},
{0x9c,0x01,{0x00}},
{0x08,0x01,{0x83}},
{0x09,0x01,{0x04}},
{0x0a,0x01,{0x30}},   //0930
{0x0b,0x01,{0x01}},
{0x0c,0x01,{0x03}},   //CLW rising
{0x0e,0x01,{0x87}},   //CLW falling

{0xFF,0x03,{0x78,0x07,0x11}},		//page11
{0x00,0x01,{0x03}},  //CKV CLW rising
{0x01,0x01,{0x87}},  //CKV CLW falling

{0xFF,0x03,{0x78,0x07,0x01}},    //page1
{0x31,0x01,{0x07}},   //GOUTR_01
{0x32,0x01,{0x07}},   //GOUTR_02
{0x33,0x01,{0x07}},   //GOUTR_03
{0x34,0x01,{0x3D}},   //GOUTR_04
{0x35,0x01,{0x3D}},   //GOUTR_05 TP_SW
{0x36,0x01,{0x28}},   //GOUTR_06 CT_SW
{0x37,0x01,{0x01}},   //GOUTR_07 U2D
{0x38,0x01,{0x3C}},   //GOUTR_08 D2U
{0x39,0x01,{0x3C}},   //GOUTR_09 GOFF
{0x3A,0x01,{0x28}},   //GOUTR_10 XDONB
{0x3B,0x01,{0x28}},   //GOUTR_11 XDONB
{0x3C,0x01,{0x2C}},   //GOUTR_12 RST
{0x3D,0x01,{0x0C}},   //GOUTR_13 VEND
{0x3E,0x01,{0x09}},   //GOUTR_14 STV_L
{0x3F,0x01,{0x13}},   //GOUTR_15 XCLK_E
{0x40,0x01,{0x13}},   //GOUTR_16 XCLK_E
{0x41,0x01,{0x11}},   //GOUTR_17 CLK_E
{0x42,0x01,{0x11}},   //GOUTR_18 CLK_E
{0x43,0x01,{0x30}},   //GOUTR_19 MUX_B
{0x44,0x01,{0x30}},   //GOUTR_20 MUX_B
{0x45,0x01,{0x2F}},   //GOUTR_21 MUX_G
{0x46,0x01,{0x2F}},   //GOUTR_22 MUX_G
{0x47,0x01,{0x2E}},   //GOUTR_23 MUX_R
{0x48,0x01,{0x2E}},   //GOUTR_24 MUX_R

{0x49,0x01,{0x07}},   //GOUTL_01
{0x4A,0x01,{0x07}},   //GOUTL_02
{0x4B,0x01,{0x07}},   //GOUTL_03
{0x4C,0x01,{0x3D}},   //GOUTL_04
{0x4D,0x01,{0x3D}},   //GOUTL_05 TP_SW
{0x4E,0x01,{0x28}},   //GOUTL_06 CT_SW
{0x4F,0x01,{0x01}},   //GOUTL_07 U2D
{0x50,0x01,{0x3C}},   //GOUTL_08 D2U
{0x51,0x01,{0x3C}},   //GOUTL_09 GOFF
{0x52,0x01,{0x28}},   //GOUTL_10 XDONB
{0x53,0x01,{0x28}},   //GOUTL_11 XDONB
{0x54,0x01,{0x2C}},   //GOUTL_12 RST
{0x55,0x01,{0x0C}},   //GOUTL_13 VEND
{0x56,0x01,{0x08}},   //GOUTL_14 STV_R
{0x57,0x01,{0x12}},   //GOUTL_15 XCLK_O
{0x58,0x01,{0x12}},   //GOUTL_16 XCLK_O
{0x59,0x01,{0x10}},   //GOUTL_17 CLK_O
{0x5A,0x01,{0x10}},   //GOUTL_18 CLK_O
{0x5B,0x01,{0x30}},   //GOUTL_19 MUX_B
{0x5C,0x01,{0x30}},   //GOUTL_20 MUX_B
{0x5D,0x01,{0x2F}},   //GOUTL_21 MUX_G
{0x5E,0x01,{0x2F}},   //GOUTL_22 MUX_G
{0x5F,0x01,{0x2E}},   //GOUTL_23 MUX_R
{0x60,0x01,{0x2E}},   //GOUTL_24 MUX_R

{0x61,0x01,{0x07}},
{0x62,0x01,{0x07}},
{0x63,0x01,{0x07}},
{0x64,0x01,{0x3D}},
{0x65,0x01,{0x3D}},
{0x66,0x01,{0x28}},
{0x67,0x01,{0x3C}},
{0x68,0x01,{0x00}},
{0x69,0x01,{0x3C}},
{0x6A,0x01,{0x28}},
{0x6B,0x01,{0x28}},
{0x6C,0x01,{0x2C}},
{0x6D,0x01,{0x09}},
{0x6E,0x01,{0x0C}},
{0x6F,0x01,{0x10}},
{0x70,0x01,{0x10}},
{0x71,0x01,{0x12}},
{0x72,0x01,{0x12}},
{0x73,0x01,{0x30}},
{0x74,0x01,{0x30}},
{0x75,0x01,{0x2F}},
{0x76,0x01,{0x2F}},
{0x77,0x01,{0x2E}},
{0x78,0x01,{0x2E}},

{0x79,0x01,{0x07}},
{0x7A,0x01,{0x07}},
{0x7B,0x01,{0x07}},
{0x7C,0x01,{0x3D}},
{0x7D,0x01,{0x3D}},
{0x7E,0x01,{0x28}},
{0x7F,0x01,{0x3C}},
{0x80,0x01,{0x00}},
{0x81,0x01,{0x3C}},
{0x82,0x01,{0x28}},
{0x83,0x01,{0x28}},
{0x84,0x01,{0x2C}},
{0x85,0x01,{0x08}},
{0x86,0x01,{0x0C}},
{0x87,0x01,{0x11}},
{0x88,0x01,{0x11}},
{0x89,0x01,{0x13}},
{0x8A,0x01,{0x13}},
{0x8B,0x01,{0x30}},
{0x8C,0x01,{0x30}},
{0x8D,0x01,{0x2F}},
{0x8E,0x01,{0x2F}},
{0x8F,0x01,{0x2E}},
{0x90,0x01,{0x2E}},

{0x91,0x01,{0xC1}},
{0x92,0x01,{0x19}},
{0x93,0x01,{0x08}},
{0x94,0x01,{0x00}},
{0x95,0x01,{0x01}},
{0x96,0x01,{0x19}},
{0x97,0x01,{0x08}},
{0x98,0x01,{0x00}},
{0xA0,0x01,{0x83}},
{0xA1,0x01,{0x44}},
{0xA2,0x01,{0x83}},
{0xA3,0x01,{0x44}},
{0xA4,0x01,{0x61}},
{0xA5,0x01,{0x03}},  //STCH FTI1
{0xA6,0x01,{0x02}},  //STCH FTI2
{0xA7,0x01,{0x50}},
{0xA8,0x01,{0x1A}},
{0xB0,0x01,{0x00}},
{0xB1,0x01,{0x00}},
{0xB2,0x01,{0x00}},
{0xB3,0x01,{0x00}},
{0xB4,0x01,{0x00}},
{0xC5,0x01,{0x29}},

{0xFF,0x03,{0x78,0x07,0x11}},
{0x1C,0x01,{0x90}},
{0x1D,0x01,{0x88}},
{0x1E,0x01,{0x1F}},
{0x1F,0x01,{0x1F}},

{0xFF,0x03,{0x78,0x07,0x01}},

{0xD1,0x01,{0x11}},
{0xD2,0x01,{0x00}},
{0xD3,0x01,{0x01}},
{0xD4,0x01,{0x00}},
{0xD5,0x01,{0x00}},
{0xD6,0x01,{0x3D}},
{0xD7,0x01,{0x00}},
{0xD8,0x01,{0x09}},
{0xD9,0x01,{0x54}},
{0xDA,0x01,{0x00}},
{0xDB,0x01,{0x00}},
{0xDC,0x01,{0x00}},
{0xDD,0x01,{0x00}},

{0xDE,0x01,{0x00}},
{0xDF,0x01,{0x00}},
{0xE0,0x01,{0x00}},
{0xE1,0x01,{0x00}},
{0xE2,0x01,{0x00}},
{0xE3,0x01,{0x13}},  //                 0907  
{0xE4,0x01,{0x52}},
{0xE5,0x01,{0x09}},
{0xE6,0x01,{0x44}},		//sleep in/out blanking 3 frame black
{0xE7,0x01,{0x00}},
{0xE8,0x01,{0x01}},
{0xED,0x01,{0x55}},
{0xEF,0x01,{0x30}},
{0xF0,0x01,{0x00}},
{0xF4,0x01,{0x54}},

{0xFF,0x03,{0x78,0x07,0x02}},
{0x01,0x01,{0xD5}},       	//time out 7th frame GIP toggle
{0x47,0x01,{0x03}},		//CKH RGBRGB
//{0x4F,0x01,{0x01}},		//Dummy CKH connect
{0x46,0x01,{0x22}},		//VBP VFP CKH
{0x6B,0x01,{0x11}},

{0xFF,0x03,{0x78,0x07,0x12}},
{0x10,0x01,{0x15}},      	//t8_de
{0x12,0x01,{0x0F}},      	//t9_de
{0x13,0x01,{0x46}},     	//t7_de
{0x16,0x01,{0x0F}},		//SOURCE SDT
{0x3A,0x01,{0x05}},		//PCT2   0819
//{0x1E,0x01,{0x44}},		//CKH EQT=GND
{0xC0,0x01,{0xA0}}, 		//BIST RTNA
{0xC2,0x01,{0x08}}, 		//BIST VBP  8
{0xC3,0x01,{0x28}}, 		//BIST VFP  40

{0xFF,0x03,{0x78,0x07,0x05}},
{0x56,0x01,{0xFF}},		//P2P_TD Noise test
{0x1C,0x01,{0x85}},		//VCOM=-0.1V 8E
{0x72,0x01,{0x60}},  		//VGH=9.5V
{0x74,0x01,{0x56}},  		//VGL= -9V
{0x76,0x01,{0x5B}},  		//VGHO=8.5V
{0x7A,0x01,{0x51}},  		//VGL0= -8V
{0x7B,0x01,{0x7E}},		//GVDDP=5.0V
{0x7C,0x01,{0x7E}},		//GVDDN=-5.0V

{0xAE,0x01,{0x29}},       	//pwr_d2a_cp_vgh_en
{0xB1,0x01,{0x39}},       	//pwr_d2a_cp_vgl_en

{0x46,0x01,{0x58}},       	//pwr_tcon_vgho_en
{0x47,0x01,{0x78}},       	//pwr_tcon_vglo_en

{0xB5,0x01,{0x58}},       	//PWR_D2A_HVREG_VGHO_EN CHECK
{0xB7,0x01,{0x78}},       	//PWR_D2A_HVREG_VGLO_EN CHECK
{0xC9,0x01,{0x90}},       	//Power off

{0xFF,0x03,{0x78,0x07,0x06}},
//{0x3E,0x01,{0xE2}},  	//11 dont reload otp
{0xC0,0x01,{0x40}},		//Res=720*1600 Y
{0xC1,0x01,{0x16}},		//Res=720*1600 Y
{0xC2,0x01,{0xF8}},		//Res=720*1600 X
{0xC3,0x01,{0x06}}, 		//ss_reg
{0x96,0x01,{0x50}},		//save power mipi bais
{0xDD,0x01,{0x17}}, 	//3LANE

{0xB4,0x01,{0xD9}},      	//D_2020 9_Set
{0xB5,0x01,{0x24}},     	//Date24

//Gamma Register
{0xFF,0x03,{0x78,0x07,0x08}},
{0xE0,0x1F,
{0x00,0x30,0x44,0x67,0x00,0xA1,0xCA,0xED,0x15,0x24,0x4C,0x8F,0x29,0xBC,0x04,0x3B,0x2A,0x73,0xB2,0xDB,0x3F,0x0F,0x34,0x5B,0x3F,0x75,0x94,0xBF,0x0F,0xC9,0xCF}},
{0xE1,0x1F,
{0x00,0x30,0x44,0x67,0x00,0xA1,0xCA,0xED,0x15,0x24,0x4C,0x8F,0x29,0xBC,0x04,0x3B,0x2A,0x73,0xB2,0xDB,0x3F,0x0F,0x34,0x5B,0x3F,0x75,0x94,0xBF,0x0F,0xC9,0xCF}},

{0xFF,0x03,{0x78,0x07,0x0B}},
{0xC0,0x01,{0x88}},
{0xC1,0x01,{0x23}},
{0xC2,0x01,{0x06}},
{0xC3,0x01,{0x06}},
{0xC4,0x01,{0xCB}},
{0xC5,0x01,{0xCB}},
{0xD2,0x01,{0x45}},
{0xD3,0x01,{0x10}},
{0xD4,0x01,{0x04}},
{0xD5,0x01,{0x04}},
{0xD6,0x01,{0x7E}},
{0xD7,0x01,{0x7E}},
{0xAB,0x01,{0xE0}},

{0xFF,0x03,{0x78,0x07,0x0C}},		//TP Modulation  0923
{0x00,0x01,{0x19}},
{0x01,0x01,{0x32}},
{0x02,0x01,{0x19}},
{0x03,0x01,{0x38}},
{0x04,0x01,{0x1A}},
{0x05,0x01,{0x38}},
{0x06,0x01,{0x18}},
{0x07,0x01,{0x2F}},
{0x08,0x01,{0x19}},
{0x09,0x01,{0x34}},
{0x0A,0x01,{0x18}},
{0x0B,0x01,{0x2D}},
{0x0C,0x01,{0x18}},
{0x0D,0x01,{0x2E}},
{0x0E,0x01,{0x19}},
{0x0F,0x01,{0x33}},
{0x10,0x01,{0x17}},
{0x11,0x01,{0x29}},
{0x12,0x01,{0x1A}},
{0x13,0x01,{0x37}},
{0x14,0x01,{0x1A}},
{0x15,0x01,{0x35}},
{0x16,0x01,{0x1B}},
{0x17,0x01,{0x3B}},
{0x18,0x01,{0x1B}},
{0x19,0x01,{0x3C}},
{0x1A,0x01,{0x18}},
{0x1B,0x01,{0x2C}},
{0x1C,0x01,{0x19}},
{0x1D,0x01,{0x30}},
{0x1E,0x01,{0x1A}},
{0x1F,0x01,{0x36}},
{0x20,0x01,{0x1B}},
{0x21,0x01,{0x3A}},
{0x22,0x01,{0x1B}},
{0x23,0x01,{0x39}},
{0x24,0x01,{0x18}},
{0x25,0x01,{0x2B}},
{0x26,0x01,{0x18}},
{0x27,0x01,{0x2A}},
{0x28,0x01,{0x24}},
{0x29,0x01,{0x64}},

{0xFF,0x03,{0x78,0x07,0x0E}},
{0x00,0x01,{0xA3}}, 		//LH mode
{0x02,0x01,{0x0F}},
{0x04,0x01,{0x00}}, 		//TSHD_1VP off & TSVD free run off
{0x20,0x01,{0x07}}, 		//8 unit
{0x27,0x01,{0x00}}, 		//Unit line=200
{0x29,0x01,{0xC8}}, 		//Unit line=200
{0x25,0x01,{0x0A}}, 		//TP2_unit0=170us
{0x26,0x01,{0xBA}}, 		//TP2_unit0=170us
{0x2D,0x01,{0x1F}}, 		//RTN 9.0 usec
{0x30,0x01,{0x04}}, 		//RTN 9.0 usec
{0x21,0x01,{0x24}}, 		//TSVD1 rising position shift 0924
{0x23,0x01,{0x44}}, 		//TSVD2 rising position shift 0821

{0xB0,0x01,{0x21}},       	//TP1 unit
{0xC0,0x01,{0x01}}, 		//TP3 unit

{0x05,0x01,{0x20}}, 		//PageC TP modulation ON (20_ON / 24_OFF)
{0x2B,0x01,{0x13}}, 		//PageC TP modulation step 0923

{0xFF,0x03,{0x78,0x07,0x1E}},
{0xAB,0x01,{0x27}}, 		//TSVD2 position 0821
{0xA2,0x01,{0x00}}, 		//TSHD_LV_FALLING 0904
{0x00,0x01,{0x90}}, 		//60Hz TP1-1
{0x06,0x01,{0x90}}, 		//60Hz TP3-4
{0x07,0x01,{0x90}}, 		//60Hz TP3-3
{0x08,0x01,{0x90}}, 		//60Hz TP3-2
{0x09,0x01,{0x90}}, 		//60Hz TP3-1

{0x0A,0x01,{0x15}},      	//TP3 t8_de
{0x0C,0x01,{0x0F}},      	//TP3 t9_de
{0x0D,0x01,{0x46}},     	//TP3 t7_de
//+ExtB P200912-01579 01603 01637,liutongxing.wt,modify,20200915,modify for 12bit backlight mode
{0xFF,0x03,{0x78,0x07,0x03}},    //Page3
{0x83,0x01,{0x20}},
{0x84,0x01,{0x00}},

{0xFF,0x03,{0x78,0x07,0x00}},	//Page0
//{0x36,0x01,{0x02}},
{0x51,0x02,{0x00,0x00}},
//+ExtB P200717-03737 ,yangzheng.wt,modify,20200717,modify for The fingerprint unlocking speed is slow when the screen is off
{0x68,0x02,{0x02,0x01}},
//-ExtB P200717-03737 ,yangzheng.wt,modify,20200717,modify for The fingerprint unlocking speed is slow when the screen is off
{0x53,0x01,{0x2C}},
{0x55,0x01,{0x00}},
{0x11,0x01,{0x00}},		//Sleep out
{REGFLAG_DELAY,120,{}},
{0x29,0x01,{0x00}},		//Display on
{REGFLAG_DELAY,20,{}},
{0x35,0x01,{0x00}}
};

static struct LCM_setting_table bl_level[] = {
	{ 0xFF, 0x03, {0x78, 0x07, 0x00} },
	{ 0x51, 0x02, {0x0F, 0xFF} },
	{ REGFLAG_END_OF_TABLE, 0x00, {} }
};
//+add by songbinbo.wt for ILITEK tp reset sequence 20190417
extern void lcd_load_ili_fw(void);
//-add by songbinbo.wt for ILITEK tp reset sequence 20190417
static void push_table(void *cmdq, struct LCM_setting_table *table,
	unsigned int count, unsigned char force_update)
{
	unsigned int i;
	unsigned cmd;

	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;

		switch (cmd) {

		case REGFLAG_DELAY:
			if (table[i].count <= 10)
				MDELAY(table[i].count);
			else
				MDELAY(table[i].count);
			break;

		case REGFLAG_UDELAY:
			UDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE:
			break;

		default:
			dsi_set_cmdq_V22(cmdq, cmd, table[i].count, table[i].para_list, force_update);
		}
	}
}


static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}


static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->physical_width = LCM_PHYSICAL_WIDTH/1000;
	params->physical_height = LCM_PHYSICAL_HEIGHT/1000;
	//params->physical_width_um = LCM_PHYSICAL_WIDTH;
	//params->physical_height_um = LCM_PHYSICAL_HEIGHT;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
	params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
	//lcm_dsi_mode = CMD_MODE;
#else
	params->dsi.mode = SYNC_PULSE_VDO_MODE;
	//params->dsi.switch_mode = CMD_MODE;
	//lcm_dsi_mode = SYNC_PULSE_VDO_MODE;
	//params->dsi.mode   = SYNC_PULSE_VDO_MODE;	//SYNC_EVENT_VDO_MODE
#endif
	//LCM_LOGI("lcm_get_params lcm_dsi_mode %d\n", lcm_dsi_mode);
	params->dsi.switch_mode_enable = 0;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_THREE_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;
	/* video mode timing */

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active = 2;
	params->dsi.vertical_backporch = 6;
	params->dsi.vertical_frontporch = 38;
	//params->dsi.vertical_frontporch_for_low_power = 540;/*disable dynamic frame rate*/
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 12;
	params->dsi.horizontal_backporch = 112;
	params->dsi.horizontal_frontporch = 116;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_range = 4;
	params->dsi.ssc_disable = 1;
	/*params->dsi.ssc_disable = 1;*/
#ifndef CONFIG_FPGA_EARLY_PORTING
#if (LCM_DSI_CMD_MODE)
	params->dsi.PLL_CLOCK = 270;	/* this value must be in MTK suggested table */
#else
	params->dsi.PLL_CLOCK = 400;	/* this value must be in MTK suggested table */
#endif
	//params->dsi.PLL_CK_CMD = 220;
	//params->dsi.PLL_CK_VDO = 255;
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif
	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;

	params->backlight_cust_count=1;
	params->backlight_cust[0].max_brightness = 255;
	params->backlight_cust[0].min_brightness = 10;
	params->backlight_cust[0].max_bl_lvl = 4095;
	params->backlight_cust[0].min_bl_lvl = 50;

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	params->round_corner_en = 0;
	params->corner_pattern_width = 720;
	params->corner_pattern_height = 32;
#endif
}

static void lcm_init_power(void)
{
	/*pr_debug("lcm_init_power\n");*/
	display_bias_enable();
	MDELAY(15);

}
extern bool ili_gesture_status;
static void lcm_suspend_power(void)
{
	/*pr_debug("lcm_suspend_power\n");*/
	SET_RESET_PIN(0);
	MDELAY(2);
	if(ili_gesture_status == 0)
	{
		display_bias_disable();
		pr_debug("[LCM] lcm suspend power down.\n");
	}
}

static void lcm_resume_power(void)
{
	/*pr_debug("lcm_resume_power\n");*/
	display_bias_enable();
}

#ifdef BUILD_LK
static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{
   mt_set_gpio_mode(GPIO, GPIO_MODE_00);
   mt_set_gpio_dir(GPIO, GPIO_DIR_OUT);
   mt_set_gpio_out(GPIO, (output>0)? GPIO_OUT_ONE: GPIO_OUT_ZERO);
}
#endif

static void lcm_init(void)
{
	//pr_debug("[LCM] lcm_init\n");
	/*lcm_reset_pin(1);
	MDELAY(10);*/
	MDELAY(10);
	lcm_reset_pin(0);
	MDELAY(1);
	lcm_reset_pin(1);
	//+modify by linminyi1.wt for ILITEK tp reset sequence 20200909
	ili_resume_by_ddi();
	//-modify by linminyi1.wt for ILITEK tp reset sequence 20200909
	MDELAY(20);
	push_table(NULL, init_setting_vdo, sizeof(init_setting_vdo) / sizeof(struct LCM_setting_table), 1);
	//pr_debug("[LCM] ili7806s--tcl--sm5109----init success ----\n");

}

static void lcm_suspend(void)
{
	//pr_debug("[LCM] lcm_suspend\n");

	push_table(NULL, lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
	MDELAY(10);

}

static void lcm_resume(void)
{
	//pr_debug("[LCM] lcm_resume\n");

	lcm_init();
}

#if 1
static unsigned int lcm_compare_id(void)
{

	return 1;
}
#endif

/* return TRUE: need recovery */
/* return FALSE: No need recovery */
static unsigned int lcm_esd_check(void)
{
#ifndef BUILD_LK
	char buffer[3];
	int array[4];

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x0A, buffer, 1);

	if (buffer[0] != 0x9c) {
		LCM_LOGI("[LCM ERROR] [0x53]=0x%02x\n", buffer[0]);
		return TRUE;
	}
	LCM_LOGI("[LCM NORMAL] [0x53]=0x%02x\n", buffer[0]);
	return FALSE;
#else
	return FALSE;
#endif

}

static unsigned int lcm_ata_check(unsigned char *buffer)
{
#ifndef BUILD_LK
	unsigned int ret = 0;
	unsigned int x0 = FRAME_WIDTH / 4;
	unsigned int x1 = FRAME_WIDTH * 3 / 4;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);

	unsigned int data_array[3];
	unsigned char read_buf[4];

	LCM_LOGI("ATA check size = 0x%x,0x%x,0x%x,0x%x\n", x0_MSB, x0_LSB, x1_MSB, x1_LSB);
	data_array[0] = 0x0005390A;	/* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00043700;	/* read id return two byte,version and id */
	dsi_set_cmdq(data_array, 1, 1);

	read_reg_v2(0x2A, read_buf, 4);

	if ((read_buf[0] == x0_MSB) && (read_buf[1] == x0_LSB)
	    && (read_buf[2] == x1_MSB) && (read_buf[3] == x1_LSB))
		ret = 1;
	else
		ret = 0;

	x0 = 0;
	x1 = FRAME_WIDTH - 1;

	x0_MSB = ((x0 >> 8) & 0xFF);
	x0_LSB = (x0 & 0xFF);
	x1_MSB = ((x1 >> 8) & 0xFF);
	x1_LSB = (x1 & 0xFF);

	data_array[0] = 0x0005390A;	/* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);
	return ret;
#else
	return 0;
#endif
}
static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{

	// set 12bit
	bl_level[1].para_list[0] = (level&0xF00)>>8;
	bl_level[1].para_list[1] = (level&0xFF);
	pr_err("[tcl_ili7806s]para_list[0]=%x,para_list[1]=%x\n",bl_level[1].para_list[0],bl_level[1].para_list[1]);

	//bl_level[1].para_list[0] = 0x0F;
	//bl_level[1].para_list[1] = 0xFF;

	push_table(handle, bl_level, sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}

static void *lcm_switch_mode(int mode)
{
#ifndef BUILD_LK
/* customization: 1. V2C config 2 values, C2V config 1 value; 2. config mode control register */
	if (mode == 0) {	/* V2C */
		lcm_switch_mode_cmd.mode = CMD_MODE;
		lcm_switch_mode_cmd.addr = 0xBB;	/* mode control addr */
		lcm_switch_mode_cmd.val[0] = 0x13;	/* enabel GRAM firstly, ensure writing one frame to GRAM */
		lcm_switch_mode_cmd.val[1] = 0x10;	/* disable video mode secondly */
	} else {		/* C2V */
		lcm_switch_mode_cmd.mode = SYNC_PULSE_VDO_MODE;
		lcm_switch_mode_cmd.addr = 0xBB;
		lcm_switch_mode_cmd.val[0] = 0x03;	/* disable GRAM and enable video mode */
	}
	return (void *)(&lcm_switch_mode_cmd);
#else
	return NULL;
#endif
}

#if (LCM_DSI_CMD_MODE)

/* partial update restrictions:
 * 1. roi width must be 1080 (full lcm width)
 * 2. vertical start (y) must be multiple of 16
 * 3. vertical height (h) must be multiple of 16
 */
static void lcm_validate_roi(int *x, int *y, int *width, int *height)
{
	unsigned int y1 = *y;
	unsigned int y2 = *height + y1 - 1;
	unsigned int x1, w, h;

	x1 = 0;
	w = FRAME_WIDTH;

	y1 = round_down(y1, 16);
	h = y2 - y1 + 1;

	/* in some cases, roi maybe empty. In this case we need to use minimu roi */
	if (h < 16)
		h = 16;

	h = round_up(h, 16);

	/* check height again */
	if (y1 >= FRAME_HEIGHT || y1 + h > FRAME_HEIGHT) {
		/* assign full screen roi */
		LCM_LOGD("%s calc error,assign full roi:y=%d,h=%d\n", __func__, *y, *height);
		y1 = 0;
		h = FRAME_HEIGHT;
	}

	/*LCM_LOGD("lcm_validate_roi (%d,%d,%d,%d) to (%d,%d,%d,%d)\n",*/
	/*	*x, *y, *width, *height, x1, y1, w, h);*/

	*x = x1;
	*width = w;
	*y = y1;
	*height = h;
}
#endif
/*bug 350122 - add white point reading function in lk , houbenzhong.wt, 20180411, begin*/
/*struct boe_panel_white_point{
	unsigned short int white_x;
	unsigned short int white_y;
};

//static struct boe_panel_white_point white_point;
*/

static struct LCM_setting_table set_cabc[] = {
	{ 0xFF, 0x03, {0x78, 0x07, 0x00} },
	{ 0x55, 0x01, {0x02} },
	{ REGFLAG_END_OF_TABLE, 0x00, {} }
};

static int cabc_status = 0;
static void lcm_set_cabc_cmdq(void *handle, unsigned int enable)
{
	pr_err("[truly_ili7806s] cabc set to %d\n", enable);
	if (enable==1){
		set_cabc[1].para_list[0]=0x02;
		push_table(handle, set_cabc, sizeof(set_cabc) / sizeof(struct LCM_setting_table), 1);
		pr_err("[truly_ili7806s] cabc set enable, set_cabc[0x%2x]=%x \n",set_cabc[1].cmd,set_cabc[1].para_list[0]);
	}else if (enable == 0){
		set_cabc[1].para_list[0]=0x00;
		push_table(handle, set_cabc, sizeof(set_cabc) / sizeof(struct LCM_setting_table), 1);
		pr_err("[truly_ili7806s] cabc set disable, set_cabc[0x%2x]=%x \n",set_cabc[1].cmd,set_cabc[1].para_list[0]);
	}
	cabc_status = enable;
}

static void lcm_get_cabc_status(int *status)
{
	pr_err("[truly_ili7806s] cabc get to %d\n", cabc_status);
	*status = cabc_status;
}

//#define WHITE_POINT_BASE_X 167
//#define WHITE_POINT_BASE_Y 192
#if (LCM_DSI_CMD_MODE)
struct LCM_DRIVER ili7806s_hd_plus_dsi_vdo_txd_na_lcm_drv = {
	.name = "ili7806s_hd_plus_dsi_cmd_txd_na_lcm_drv",
#else

struct LCM_DRIVER ili7806s_hd_plus_dsi_vdo_txd_na_lcm_drv = {
	.name = "ili7806s_hd_plus_vdo_txd_na",
#endif
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.esd_check = lcm_esd_check,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
/*bug 350122 - add white point reading function in lk , houbenzhong.wt, 20180411, begin*/
//	.get_white_point = lcm_white_x_y,
/*bug 350122 - add white point reading function in lk , houbenzhong.wt, 20180411, end*/
	.ata_check = lcm_ata_check,
	.switch_mode = lcm_switch_mode,
	.set_cabc_cmdq = lcm_set_cabc_cmdq,
	.get_cabc_status = lcm_get_cabc_status,
#if (LCM_DSI_CMD_MODE)
	.validate_roi = lcm_validate_roi,
#endif

};
