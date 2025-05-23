/**
 ****************************************************************************************
 *
 * @file phy_trident.c
 *
 * @brief File containing the nX Trident-based physical layer driver API.
 *
 * Copyright (C) Beken 2011-2016
 *
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "rwnx_config.h"
#include "phy.h"
#include "ke_timer.h"
#include "rd.h"
#include "phy_trident.h"
#include "reg_mdm_stat.h"
#include "reg_mdm_cfg.h"
#include "reg_rc.h"
#include "reg_agc.h"
#include "include.h"
#include "arm_arch.h"
#include "dd_pub.h"
#include "drv_model_pub.h"
#include "sys_ctrl.h"
#include "sys_ctrl_pub.h"
#include "target_util_pub.h"
#include "uart_pub.h"
#include "intc_pub.h"
#include "power_save_pub.h"
#include "bk7011_cal_pub.h"

/*
 * STRUCTURES
 ****************************************************************************************
 */
/// Structure containing the parameters of the Trident PHY configuration
struct phy_trd_cfg_tag
{
    /// MDM type(nxm)(upper nibble) and MDM2RF path mapping(lower nibble)
    uint8_t path_mapping;
    /// TX DC offset compensation
    uint32_t tx_dc_off_comp;
};

/*
 * GLOBALS
 ****************************************************************************************
 */
/// Global PHY driver environment.
struct phy_env_tag phy_env[1];


/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
 //BK,170206,solve input-power vs PER discontinuous issue.
#define NEW_AGC_PARA  1
//BK,170206,solve large input-power 0% PER issue.
#define RF_DOWN_ENABLE 0 

//#define CFG_SYS_CLOSE_CCA

const uint32_t agc_ram_parameter[] =
{
#if (CFG_SOC_NAME == SOC_BK7231N)
	0x30000000,
	0x01000000,
	0xa8000016,
	0x00000000,
	0x30000000,
	0x91000000,
	0x04104849,
	0x00000000,
	0x2000008f,
	0x04000072,
	0x00000000,
	0x34014000,
	0x10000001,
	0x0800001a,
	0x00000000,
	0x44008000,
	0x20000038,
	0x08000013,
	0x00000000,
	0x240c808f,
	0x080001b6,
	0x00000000,
	0x30000000,
	0x20090833,
	0x0400000b,
	0x00000000,
	0x30000000,
	0x41000000,
	0x0400001e,
	0x00000000,
	0x30000000,
	0x51000101,
	0x04000022,
	0x00000000,
	0x30000000,
	0x01000000,
	0x04000026,
	0x00000000,
	0x34010000,
	0x12007eeb,
	0x0870482a,
	0x00000000,
	0x34008001,
	0x64000001,
	0x0800002e,
	0x00000000,
	0x3402800f,
	0x68002101,
	0x08000032,
	0x00000000,
	0x0000139f,
	0x00000000,
	0x4400438f,
	0xa410497e,
	0x081049a2,
	0x00000000,
	0x30000000,
	0x94000000,
	0x0400003c,
	0x00000000,
	0x50000000,
	0x51000101,
	0x28000041,
	0x04000045,
	0x00000000,
	0x3400c000,
	0x120055e9,
	0x1c204832,
	0x00000000,
	0x3400c000,
	0x120037c9,
	0x1c204832,
	0x00000000,
	0x30000000,
	0x51000101,
	0x0410484d,
	0x00000000,
	0x30000000,
	0x13002189,
	0x1c000051,
	0x00000000,
	0x30000000,
	0x61020202,
	0x04000055,
	0x00000000,
	0x30000000,
	0x52000000,
	0x04000059,
	0x00000000,
	0x2400c000,
	0x0800005c,
	0x00000000,
	0x30000000,
	0x80000000,
	0x04104860,
	0x00000000,
	0x3400400f,
	0x52000000,
	0x08000064,
	0x00000000,
	0x340204af,
	0x31191910,
	0x08000068,
	0x00000000,
	0x30000eaf,
	0x51000101,
	0x0400006c,
	0x00000000,
	0x80000eaf,
	0x0c00007a,
	0x1000007d,
	0x14600080,
	0x04000032,
	0x00000000,
	0x340204af,
	0x31191910,
	0x08000076,
	0x00000000,
	0x3404ceaf,
	0x51000101,
	0x09800083,
	0x00000000,
	0x240a0eaf,
	0x09800083,
	0x00000000,
	0x24050eaf,
	0x09800083,
	0x00000000,
	0x24010eaf,
	0x09800083,
	0x00000000,
	0x60000eaf,
	0x65804888,
	0x6580008c,
	0x04104890,
	0x00000000,
	0x30000eaf,
	0x41000300,
	0x04000094,
	0x00000000,
	0x30000eaf,
	0x41000100,
	0x04000094,
	0x00000000,
	0x30000eaf,
	0x41000000,
	0x04000094,
	0x00000000,
	0x40000eaf,
	0xa400009c,
	0x04000098,
	0x00000000,
	0x30000000,
	0x14eaed83,
	0x1c00009c,
	0x00000000,
	0x24008000,
	0x0800009f,
	0x00000000,
	0x30000000,
	0x80000000,
	0x041048a3,
	0x00000000,
	0x30000000,
	0x10000000,
	0x040000a7,
	0x00000000,
	0x34014005,
	0x63000102,
	0x080000ab,
	0x00000000,
	0x30000007,
	0x50000001,
	0x040000af,
	0x00000000,
	0x30000007,
	0x32202071,
	0x040000b3,
	0x00000000,
	0x30000007,
	0x61030303,
	0x040000b7,
	0x00000000,
	0x340202ef,
	0x31202021,
	0x080000bb,
	0x00000000,
	0x34050aef,
	0x90000000,
	0x080000bf,
	0x00000000,
	0x30000aef,
	0x12000084,
	0x040000c3,
	0x00000000,
	0x30000aef,
	0x33000001,
	0x040000c7,
	0x00000000,
	0x60000eaf,
	0x658048cc,
	0x658000d0,
	0x041048d4,
	0x00000000,
	0x30000eaf,
	0x41000301,
	0x040000e0,
	0x00000000,
	0x30000eaf,
	0x41000101,
	0x040000e0,
	0x00000000,
	0x30000eaf,
	0x41000001,
	0x040000d8,
	0x00000000,
	0x30000eaf,
	0x51000001,
	0x040000dc,
	0x00000000,
	0x30000eaf,
	0x3400109c,
	0x040000e0,
	0x00000000,
	0x34190bef,
	0x52000000,
	0x080000e4,
	0x00000000,
	0x30000aef,
	0x33000101,
	0x040000e8,
	0x00000000,
	0xa0000eaf,
	0x6da048ef,
	0x658048f3,
	0x619000fb,
	0x69b000f7,
	0x041048ff,
	0x00000000,
	0x30000eaf,
	0x41030002,
	0x04000107,
	0x00000000,
	0x30000eaf,
	0x41000302,
	0x04000127,
	0x00000000,
	0x30000eaf,
	0x41010002,
	0x04000107,
	0x00000000,
	0x30000eaf,
	0x41000102,
	0x04000127,
	0x00000000,
	0x30000eaf,
	0x41000002,
	0x04000103,
	0x00000000,
	0x3000028f,
	0x51000001,
	0x0400019a,
	0x00000000,
	0x5000028f,
	0x51000001,
	0xa000019a,
	0x0400010c,
	0x00000000,
	0x3401428f,
	0x10000001,
	0x08000110,
	0x00000000,
	0x34010000,
	0x14eaed83,
	0x08000114,
	0x00000000,
	0x30000000,
	0x10000000,
	0x04000118,
	0x00000000,
	0x24014005,
	0x0800011b,
	0x00000000,
	0x3000128f,
	0x3400289c,
	0x0400011f,
	0x00000000,
	0x3000128f,
	0x35d4509c,
	0x04000123,
	0x00000000,
	0x3000128f,
	0x50000100,
	0x0400016b,
	0x00000000,
	0x7432028f,
	0x65000003,
	0x7c00012d,
	0x80104963,
	0x08000167,
	0x00000000,
	0x3000028f,
	0x41000000,
	0x04000131,
	0x00000000,
	0x8432028f,
	0x84000163, //huaming20201224 for rx sens
	0x80104963,
	0x08104937,
	0x8810493b,
	0x00000000,
	0x3000008f,
	0x41000000,
	0x881049a2,
	0x00000000,
	0x3000008f,
	0x41000000,
	0x04104834,
	0x00000000,
	0x34004005,
	0x61020202,
	0x08104943,
	0x00000000,
	0x6419000f,
	0x80104948,
	0x08000137,
	0x8810493b,
	0x00000000,
	0x3000000f,
	0x15eeea87,
	0x1c000150,
	0x00000000,
	0x3000000f,
	0x51000001,
	0x04000154,
	0x00000000,
	0x3000000f,
	0x10000001,
	0x0400014c,
	0x00000000,
	0x24008000,
	0x08000157,
	0x00000000,
	0x30000000,
	0x10000000,
	0x0400015b,
	0x00000000,
	0x34010005,
	0x61030303,
	0x0800015f,
	0x00000000,
	0x30000007,
	0x50000001,
	0x04000163,
	0x00000000,
	0x3000008f,
	0x41000000,
	0x88104834,
	0x00000000,
	0x3000038f,
	0x01000000,
	0x0400019a,
	0x00000000,
	0x4577138f,
	0x70000173,
	0x0800016f,
	0x00000000,
	0x3000038f,
	0x51000100,
	0x0400019a,
	0x00000000,
	0x3000138f,
	0x41000000,
	0x04000177,
	0x00000000,
	0x3000138f,
	0x65000003,
	0x0400017b,
	0x00000000,
	0x2000108f,
	0x881049a2,
	0x00000000,
	0x3000018f,
	0x66000001,
	0x04000182,
	0x00000000,
	0x34018000,
	0x10000001,
	0x08104986,
	0x00000000,
	0x30000000,
	0x61010101,
	0x0400018a,
	0x00000000,
	0x2403c00f,
	0x0800018d,
	0x00000000,
	0x5405000f,
	0x95000000,
	0xb4000192,
	0x08000196,
	0x00000000,
	0x3000000f,
	0x52000001,
	0x04000008,
	0x00000000,
	0x3000000f,
	0x52000000,
	0x0400000b,
	0x00000000,
	0x3401438f,
	0x10000001,
	0x0810499e,
	0x00000000,
	0x34010000,
	0x14ecec83,
	0x087049a6,
	0x00000000,
	0x34004000,
	0x95000000,
	0x0810480b,
	0x00000000,
	0x30000000,
	0x80000000,
	0x041049aa,
	0x00000000,
	0x30000000,
	0x95000000,
	0x041049ae,
	0x00000000,
	0x30000000,
	0x01000000,
	0x040001b2,
	0x00000000,
	0x30000000,
	0x12000084,
	0x0400002a,
	0x00000000,
	0x3000008f,
	0x14eaed83,
	0x1c0001ba,
	0x00000000,
	0x34010000,
	0x52000000,
	0x080001be,
	0x00000000,
	0x34008000,
	0x10000000,
	0x080001c2,
	0x00000000,
	0x24014005,
	0x080001c5,
	0x00000000,
	0x3000128f,
	0x3400209c,
	0x040001c9,
	0x00000000,
	0x3000128f,
	0x35d4509c,
	0x040001cd,
	0x00000000,
	0x3000128f,
	0x50000100,
	0x040001d1,
	0x00000000,
	0x4577138f,
	0x700001d5,
	0x0800016f,
	0x00000000,
	0x3000138f,
	0x41000000,
	0x040001d9,
	0x00000000,
	0x3000138f,
	0x65000003,
	0x040001dd,
	0x00000000,
	0x2000138f,
	0x881049a2,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x14090b11
#else
0x30000000,
0x01000000,
0xa8000016,
0x00000000,
0x30000000,
0x91000000,
0x04104845,
0x00000000,
0x2000008f,
0x0400006e,
0x00000000,
0x30000000,
0x41000000,
0x0400001a,
0x00000000,
0x40000010,
0x74000013,
0x04000034,
0x00000000,
0x240c808f,
0x0800019a,
0x00000000,
0x30000000,
0x20070633,
0x0400000b,
0x00000000,
0x30000000,
0x51000101,
0x0400001e,
0x00000000,
0x34010000,
0x12007eeb,
0x08704822,
0x00000000,
0x30000000,
0x01000000,
0x04000026,
0x00000000,
0x34008001,
0x64000001,
0x0800002a,
0x00000000,
0x3402800f,
0x68002101,
0x0800002e,
0x00000000,
0x0000139f,
0x00000000,
0x4400438f,
0xa410496a,
0x08104986,
0x00000000,
0x30000000,
0x94000000,
0x04000038,
0x00000000,
0x50000000,
0x51000101,
0x2800003d,
0x04000041,
0x00000000,
0x3400c000,
0x12005ee9,
0x1c20482e,
0x00000000,
0x3400c000,
0x12001ec9,
0x1c20482e,
0x00000000,
0x30000000,
0x51000101,
0x04104849,
0x00000000,
0x30000000,
0x13002189,
0x1c00004d,
0x00000000,
0x30000000,
0x61020202,
0x04000051,
0x00000000,
0x30000000,
0x52000000,
0x04000055,
0x00000000,
0x2400c000,
0x08000058,
0x00000000,
0x30000000,
0x80000000,
0x0410485c,
0x00000000,
0x3400400f,
0x52000000,
0x08000060,
0x00000000,
0x340204af,
0x31191910,
0x08000064,
0x00000000,
0x30000eaf,
0x51000101,
0x04000068,
0x00000000,
0x80000eaf,
0x0c000076,
0x10000079,
0x1460007c,
0x0400002e,
0x00000000,
0x340204af,
0x31191910,
0x08000072,
0x00000000,
0x3404ceaf,
0x51000101,
0x0980007f,
0x00000000,
0x240a0eaf,
0x0980007f,
0x00000000,
0x24050eaf,
0x0980007f,
0x00000000,
0x24010eaf,
0x0980007f,
0x00000000,
0x60000eaf,
0x65804884,
0x65800088,
0x0410488c,
0x00000000,
0x30000eaf,
0x41000300,
0x04000090,
0x00000000,
0x30000eaf,
0x41000100,
0x04000090,
0x00000000,
0x30000eaf,
0x41000000,
0x04000090,
0x00000000,
0x40000eaf,
0xa4000098,
0x04000094,
0x00000000,
0x30000000,
0x14eaed83,
0x1c000098,
0x00000000,
0x40000000,
0x2400009c,
0x0400009f,
0x00000000,
0x24010000,
0x0800009f,
0x00000000,
0x30000000,
0x80000000,
0x041048a3,
0x00000000,
0x34014005,
0x63000102,
0x080000a7,
0x00000000,
0x30000007,
0x50000001,
0x040000ab,
0x00000000,
0x30000007,
0x32202071,
0x040000af,
0x00000000,
0x30000007,
0x61030303,
0x040000b3,
0x00000000,
0x340202ef,
0x31202021,
0x080000b7,
0x00000000,
0x34050aef,
0x90000000,
0x080000bb,
0x00000000,
0x30000aef,
0x12000084,
0x040000bf,
0x00000000,
0x30000aef,
0x33000001,
0x040000c3,
0x00000000,
0x60000eaf,
0x658048c8,
0x658000cc,
0x041048d0,
0x00000000,
0x30000eaf,
0x41000301,
0x040000dc,
0x00000000,
0x30000eaf,
0x41000101,
0x040000dc,
0x00000000,
0x30000eaf,
0x41000001,
0x040000d4,
0x00000000,
0x30000eaf,
0x51000001,
0x040000d8,
0x00000000,
0x30000eaf,
0x3400109c,
0x040000dc,
0x00000000,
0x34190bef,
0x52000000,
0x080000e0,
0x00000000,
0x30000aef,
0x33000101,
0x040000e4,
0x00000000,
0xa0000eaf,
0x6da048eb,
0x658048ef,
0x619000f7,
0x69b000f3,
0x041048fb,
0x00000000,
0x30000eaf,
0x41030002,
0x04000103,
0x00000000,
0x30000eaf,
0x41000302,
0x0400011b,
0x00000000,
0x30000eaf,
0x41010002,
0x04000103,
0x00000000,
0x30000eaf,
0x41000102,
0x0400011b,
0x00000000,
0x30000eaf,
0x41000002,
0x040000ff,
0x00000000,
0x3000028f,
0x51000001,
0x04000182,
0x00000000,
0x5000028f,
0x51000001,
0xa0000182,
0x04000108,
0x00000000,
0x34010000,
0x14eaed83,
0x0800010c,
0x00000000,
0x24014005,
0x0800010f,
0x00000000,
0x3000128f,
0x3400289c,
0x04000113,
0x00000000,
0x3000128f,
0x35d4509c,
0x04000117,
0x00000000,
0x3000128f,
0x50000100,
0x04000157,
0x00000000,
0x7432028f,
0x65000003,
0x7c000121,
0x8010494f,
0x08000153,
0x00000000,
0x3000028f,
0x41000000,
0x04000125,
0x00000000,
0x8432028f,
0x84000133,
0x8010494f,
0x0810492b,
0x8810492f,
0x00000000,
0x3000008f,
0x41000000,
0x88104986,
0x00000000,
0x3000008f,
0x41000000,
0x04104830,
0x00000000,
0x34004005,
0x61020202,
0x08104937,
0x00000000,
0x6419000f,
0x8010493c,
0x0800012b,
0x8810492f,
0x00000000,
0x3000000f,
0x15eeea87,
0x1c000140,
0x00000000,
0x3000000f,
0x51000001,
0x04000144,
0x00000000,
0x24010000,
0x08000147,
0x00000000,
0x34010005,
0x61030303,
0x0800014b,
0x00000000,
0x30000007,
0x50000001,
0x0400014f,
0x00000000,
0x3000008f,
0x41000000,
0x88104830,
0x00000000,
0x3000038f,
0x01000000,
0x04000182,
0x00000000,
0x4577138f,
0x7000015f,
0x0800015b,
0x00000000,
0x3000038f,
0x51000100,
0x04000182,
0x00000000,
0x3000138f,
0x41000000,
0x04000163,
0x00000000,
0x3000138f,
0x65000003,
0x04000167,
0x00000000,
0x2000108f,
0x88104986,
0x00000000,
0x3000018f,
0x66000001,
0x0400016e,
0x00000000,
0x30000000,
0x61010101,
0x04000172,
0x00000000,
0x2403c00f,
0x08000175,
0x00000000,
0x5405000f,
0x95000000,
0xb400017a,
0x0800017e,
0x00000000,
0x3000000f,
0x52000001,
0x04000008,
0x00000000,
0x3000000f,
0x52000000,
0x0400000b,
0x00000000,
0x34010000,
0x14ecec83,
0x0870498a,
0x00000000,
0x34004000,
0x95000000,
0x0810480b,
0x00000000,
0x30000000,
0x80000000,
0x0410498e,
0x00000000,
0x30000000,
0x95000000,
0x04104992,
0x00000000,
0x30000000,
0x01000000,
0x04000196,
0x00000000,
0x30000000,
0x12000084,
0x04000026,
0x00000000,
0x3000008f,
0x14eaed83,
0x1c00019e,
0x00000000,
0x34010000,
0x52000000,
0x080001a2,
0x00000000,
0x24014005,
0x080001a5,
0x00000000,
0x3000128f,
0x3400209c,
0x040001a9,
0x00000000,
0x3000128f,
0x35d4509c,
0x040001ad,
0x00000000,
0x3000128f,
0x50000100,
0x040001b1,
0x00000000,
0x4577138f,
0x700001b5,
0x0800015b,
0x00000000,
0x3000138f,
0x41000000,
0x040001b9,
0x00000000,
0x3000138f,
0x65000003,
0x040001bd,
0x00000000,
0x2000138f,
0x88104986,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x14010b0b
#endif
};

void phy_pre_agc_init(void)
{
    uint32_t i;
    uint32_t len;
    uint32_t *src, *dst;
    uint32_t agc_cntl_reg;
    uint32_t clk_ctrl_reg;

    agc_cntl_reg = agc_rwnxagccntl_get();
    agc_cntl_reg |= AGC_AGCFSMRESET_BIT;
    agc_rwnxagccntl_set(agc_cntl_reg);

    clk_ctrl_reg = mdm_memclkctrl0_get();
    clk_ctrl_reg &= ~MDM_AGCMEMCLKCTRL_BIT;
    mdm_memclkctrl0_set(clk_ctrl_reg);

    /*init agc parameters*/
    len = sizeof(agc_ram_parameter) >> 2;
    dst = (uint32_t *)PHY_AGC_UCODE_ADDR;
    src = (uint32_t *)agc_ram_parameter;
    for(i = 0; i < len; i ++)
    {
        *dst ++ = *src ++;
    }

	mdm_agcclkforce_setf(1); /*the issue about reset agc*/
	
    agc_cntl_reg = agc_rwnxagccntl_get();
    agc_cntl_reg &= ~AGC_AGCFSMRESET_BIT;
    agc_rwnxagccntl_set(agc_cntl_reg);

    clk_ctrl_reg = mdm_memclkctrl0_get();
    clk_ctrl_reg |= MDM_AGCMEMCLKCTRL_BIT;
    mdm_memclkctrl0_set(clk_ctrl_reg);
}

/**
 ****************************************************************************************
 * @brief Perform a write access on radio registers through the radio controller.
 * The SPI command is sent only to the radios on the used RC paths (rc_path_sel).
 * To limit the RC register access, the whole RC register value is set in one access
 * except the START_DONE bit which will trigger the SPI transfer and also signal its end
 * when cleared by HW.
 *
 * @param[in] addr   Address of the register to write in.
 * @param[in] value  Value to write.
 *
 * @warning The function does not check the consistency of the parameters provided.
 ****************************************************************************************
 */
void phy_rc_rf_reg_write(uint8_t addr, uint16_t value)
{
}

/**
 ****************************************************************************************
 * @brief Trident board IO Expander init.
 * Given it is the components which routes important power down and lock detect signals,
 * it should be initialized first before the components are interacted with.
 ****************************************************************************************
 */
void phy_iox_init(void)
{
}

/**
 ****************************************************************************************
 * @brief Trident board 8b ADCs initialization for RSSI/Temperature/Power read back.
 ****************************************************************************************
 */
//todo init or cfg in case we change ADCs during run for rssi/power?
void phy_adc8b_init(void)
{
    //do nothing, use reg force when we'll need fcns to gather data
}

/**
 ****************************************************************************************
 * @brief Trident board 12b ADCs calibration.
 ****************************************************************************************
 */
void phy_adc12b_init(void)
{
#if (TIADC_VER == 1)            //TI ADC 
    //disable RC auto power mode
    trd_adc_autopower_setf(0);

    //reset ADCs,
    trd_adc_reset_setf(1);
    trd_adc_reset_setf(0);
    // prescaler=6: spi_freq=80/(presc+1) ; max 20MHz
    trd_adc_spi_prescaler_setf(4);

    //switch off all ADCs
    phy_trd_adc_reg_write(0x00, 0x00D, 7);

    //switch on used ADC
    phy_trd_adc_reg_write(0x00, 0x000, phy_env->rf_path_sel);

    //OTHER REG SETTINGS
    phy_trd_adc_reg_write(0x04, 0x000, phy_env->rf_path_sel);
    // 2's complement
    phy_trd_adc_reg_write(0x0A, 0x000, phy_env->rf_path_sel);
    phy_trd_adc_reg_write(0x0B, 0x000, phy_env->rf_path_sel);
    //fine gain 6dB
    phy_trd_adc_reg_write(0x0C, 0x600, phy_env->rf_path_sel);
    //Override, byte wise, MSB first, coarse gain 3.5dB,12 bits serial,ddr clocking, 2 wire interface
    phy_trd_adc_reg_write(0x0D, 0x421, phy_env->rf_path_sel); //401 without coarse gain
    //termination, drive clk output
    phy_trd_adc_reg_write(0x10, 0x000, phy_env->rf_path_sel);
    //bit/byte wise, termination of data outputs
    phy_trd_adc_reg_write(0x11, 0x000, phy_env->rf_path_sel);

    //DESERIALIZER----------------------------
    //Path A
    if (phy_is_bit_set(phy_env->rf_path_sel, RF_PATH_A))
    {
        //deserializer reset
        trd_deser_cha_rst_n_setf(0);
        //lift reset
        trd_deser_cha_rst_n_setf(1);

        //deserializer calibration
        trd_deser_cha_calib_start_setf(1);
        //poll until done
        while(!trd_deser_cha_calib_done_getf());

        //todo does the start have to be put back to 0 by SW or auto-clear at done?
        trd_deser_cha_calib_start_setf(0);
    }
    //Path B
    if (phy_is_bit_set(phy_env->rf_path_sel, RF_PATH_B))
    {
        //deserializer reset
        trd_deser_chb_rst_n_setf(0);
        //lift reset
        trd_deser_chb_rst_n_setf(1);

        //deserializer calibration
        trd_deser_chb_calib_start_setf(1);
        //poll until done
        while(!trd_deser_chb_calib_done_getf());

        //todo does the start have to be put back to 0 by SW or auto-clear at done?
        trd_deser_chb_calib_start_setf(0);
    }
    //Path C
    if (phy_is_bit_set(phy_env->rf_path_sel, RF_PATH_C))
    {
        //deserializer reset
        trd_deser_chc_rst_n_setf(0);
        //lift reset
        trd_deser_chc_rst_n_setf(1);

        //deserializer calibration
        trd_deser_chc_calib_start_setf(1);
        //poll until done
        while(!trd_deser_chc_calib_done_getf());

        //todo does the start have to be put back to 0 by SW or auto-clear at done?
        trd_deser_chc_calib_start_setf(0);
    }

#else               //Beken ADC

    rc_adda_reg0_set(0x0801A554);
    rc_adda_reg1_set(0x88924204);
    rc_adda_reg2_set(0x10484806);
    rc_adda_reg3_set(0x8C0C80C8);
    rc_adda_reg4_set(0x03010000);
    rc_adda_reg5_set(0xF80022ED);

    while(rc_adda_reg_stat_getf() & 0x3f);
#endif
}

/**
 ****************************************************************************************
 * @brief Adjust TX digital gains wrt DC offset compensation to avoid DACs saturation
 ****************************************************************************************
 */
static void adjust_txdiggains(uint32_t dc_cmp)
{
    uint8_t i_abscmp, j_abscmp, max_abscmps;

    i_abscmp = (dc_cmp & MDM_TXIDCOFFSET0_MASK) >> MDM_TXIDCOFFSET0_LSB;
    j_abscmp = (dc_cmp & MDM_TXQDCOFFSET0_MASK) >> MDM_TXQDCOFFSET0_LSB;
    max_abscmps = i_abscmp > j_abscmp ? i_abscmp : j_abscmp;

    // 12bits range
    mdm_fectrl0_pack(MDM_TX80DIGGAINLIN0_RST,
                     MDM_TX40DIGGAINLIN0_RST * (2047 - max_abscmps) / 2047,
                     MDM_TX20DIGGAINLIN0_RST * (2047 - max_abscmps) / 2047);
}


/**
 ****************************************************************************************
 * @brief Modem initialization function.
 * This function is called at reset time together with radio init, and
 * prepares the modem for rx or tx.
 ****************************************************************************************
 */
static void phy_mdm_init(uint32_t tx_dc_off_comp)
{
    intc_service_register(FIQ_MODEM, PRI_FIQ_MODEM, phy_mdm_isr);

    //turn on the 3 blocks: TX, AGC-CCA, RX - they are at 1 by default in regs ...
    mdm_rxtxpwrctrl_pack(1, 1, 1);

    mdm_dcoffset0_set(tx_dc_off_comp);
    adjust_txdiggains(tx_dc_off_comp);

    // Parameters for 40MHz bandwidth by default
    mdm_txctrl0_set(0x00000168);

    mdm_rxctrl0_set(0x00160005);
    mdm_tbectrl0_set(0x0C0F0702);

#if (NX_MDM_VER == 11)
    mdm_tbectrl2_set(0x0FF07005);
#endif

    mdm_waithtstf_setf(15);
    mdm_delaynormalgi_setf(17);

    // CPE mode
    mdm_cpemode_setf(0);

    // Enable CCA lock IRQ
    mdm_irqctrl_set(MDM_IRQCCATIMEOUTEN_BIT);

#if (CFG_SOC_NAME == SOC_BK7231N)
    REG_WRITE((REG_MDM_CFG_BASE_ADDR + 0x226 * 4), REG_READ(REG_MDM_CFG_BASE_ADDR + 0x226 * 4) & 0xFFFE); //peak_cancel_bypass=0
    if (((DEVICE_ID_BK7231N_P & DEVICE_ID_MASK) == (sctrl_ctrl(CMD_GET_DEVICE_ID, NULL) & DEVICE_ID_MASK))) {
        REG_WRITE((REG_MDM_CFG_BASE_ADDR + 0xC0C * 4), 1); //11B_TX_GAUSS_FILTER_ENABLE=1
    }
#endif

    PHY_WPRT("phy_mdm_init-->Static MDM settings done\r\n");
}


void phy_enable_lsig_intr(void)
{
	uint32_t val;
	
	val = mdm_irqctrl_get();
	val |= MDM_IRQLSIGVALIDEN_BIT;
	mdm_irqctrl_set(val);

	os_printf("lsigEn-0x2000:%x\r\n", mdm_irqctrl_get());

}

void phy_disable_lsig_intr(void)
{
	uint32_t val;
	
	val = mdm_irqctrl_get();
	val &= (~MDM_IRQLSIGVALIDEN_BIT);
	mdm_irqctrl_set(val);
	
	os_printf("lsigDis-0x2000:%x\r\n", mdm_irqctrl_get());
}


/**
 ****************************************************************************************
 * @brief AGC initialization function.
 ****************************************************************************************
 */
static void phy_agc_init(void)
{
    phy_pre_agc_init();

    // AGC events
#if (NX_MDM_VER == 10)
    agc_rwnxagcevt2_set(0x3955b004);
#else
#if (CFG_SOC_NAME == SOC_BK7231N)
    /* qunshan20210325 change from 0x806 to 0x506,aim to increase RSSI +3 */
    REG_PL_WR(REG_AGC_BASE_ADDR + 0x890, 0x506);  // Enable AGC OPT
    agc_rwnxagcevtsat_set(0x05044804);
    agc_rwnxagcevtdet_set(0x3D401008);
    agc_rwnxagcevtdis_set(0x3955B00B);
    agc_rwnxagcevtdsssdet_set(0x3ff7500f);

    // ADC sat thd
    agc_rwnxagcsat_set(0x8373335);
#else
    REG_PL_WR(REG_AGC_BASE_ADDR + 0x890, (REG_PL_RD(REG_AGC_BASE_ADDR + 0x890) | 0x2));  // Enable AGC OPT
    agc_rwnxagcevtsat_set(0x05044804);
    agc_rwnxagcevtdet_set(0x3D401008);
    agc_rwnxagcevtdis_set(0x3955B00B);
    agc_rwnxagcevtdsssdet_set(0x3ff7500f);

    // ADC sat thd
    agc_rwnxagcsat_set(0x8373335);
#endif
#endif

    //agc_rwnxagcevt2_set(0x3955b004);

    // AGCCROSS (disable crossing detection)
    agc_rwnxagccross_set(0x002803f0);

    // AGCRAMP (reduce ramp-down detection level)
    agc_rwnxagcramp_set(0x07200710);

    // RWNXAGCCCA1 (CCA{FALL,RISE}THRDBM)
    // for when RWNXAGCCCACTRL[CCAENERGYEN] is on
#if defined(CFG_SYS_CLOSE_CCA)
    agc_rwnxagccca1_set((agc_rwnxagccca1_get() & ~0x000ff0ff) | 0x000bf0c3);
#else
    agc_rwnxagccca1_set((agc_rwnxagccca1_get() & ~0x000ff1ff) | 0x000bf0c3);
#endif
    //os_printf("agc_rwnxagccca1=0x%x\n", agc_rwnxagccca1_get());

    // RWNXAGCCCACTRL
    agc_rwnxagcccactrl_set((agc_rwnxagcccactrl_get() & ~0x00000fff) | 0x00000377);

    // Configure CCA timeout
    agc_rwnxagcccatimeout_set(8000000); // 100ms

    // AGCGain Range
#if (CFG_SOC_NAME == SOC_BK7231N)
    agc_rwnxagcgainrg_set(0x4e08);
#else
    agc_rfgainmaxdb_setf(0x4b);
#endif

    // AGC Power Estimate Bias
#if (CFG_SOC_NAME == SOC_BK7231N)
    agc_vpeakadcqdbv_setf((uint8_t) - 32); //change MDM_reg819<23:16> from 0xEC to 0xE0 by cunliang20201112
#else
    agc_vpeakadcqdbv_setf((uint8_t) - 32);
#endif

    PHY_WPRT("phy_agc_init-->cca\r\n");

}

/**
 ****************************************************************************************
 * @brief Set band 2.4GHz/5GHz in RC and RF chips.
 * @param[in] band        Band value - 0=2.4GHz, 1=5GHz
 * @param[in] freq        Channel frequency in MHz
 ****************************************************************************************
 */
static void phy_set_band(uint8_t band, uint16_t freq)
{
    //MDM and AGC band related settings
    if (band  == PHY_BAND_2G4)
    {
        //mdm settings
        mdm_rxallowdsss_setf(1);
        //agc cca
        agc_ofdmonly_setf(0);
    }
    else
    {
        //mdm settings
        mdm_rxallowdsss_setf(0);
        //agc cca
        agc_ofdmonly_setf(1);
    }
}

/**
 ****************************************************************************************
 * @brief Function used to regroup the waiting , polling and Lock Detect values
 * @return Boolean value of lock state: true=locked, false=lock failed
 ****************************************************************************************
 */
static bool phy_locked(void)
{
    return true;
}

#if !CFG_SUPPORT_CALIBRATION
#define RC_TRX_REG28_ADDR          0x08628078

__INLINE void rc_trx_reg28_set(uint32_t value)
{
    REG_PL_WR(RC_TRX_REG28_ADDR, value);
}
#endif
/**
 ****************************************************************************************
 * @brief MAXIM Radio(s) initialization function.
 * This function is called at FW initialization or after phy_stop().
 * It will set RC register values and also RF register value using RC SPI control. Most
 * of the register values set in the radio(s) are static for the duration of SW run.
 ****************************************************************************************
 */
#if CFG_SUPPORT_CALIBRATION
extern INT32 rwnx_cal_load_trx_rcbekn_reg_val(void);
#endif
static void phy_rf_init(void)
{
#if !CFG_SUPPORT_CALIBRATION
    //initial Beken TRX
#if (CFG_RUNNING_PLATFORM == FPGA_PLATFORM)
    rc_trx_reg0_set(0x0813EF5E);        //RF Config. V1.6
    rc_trx_reg1_set(0x01000000);
    rc_trx_reg2_set(0x00000000);
    rc_trx_reg3_set(0x00000000);
    rc_trx_reg4_set(0x28021D00);
    rc_trx_reg5_set(0x05326C7A);
    rc_trx_reg6_set(0x72ec820e);  //0x00FF104C
    rc_trx_reg7_set(0x0922483E); //0x0922483F  20150630
    rc_trx_reg8_set(0x0768A94A);
    rc_trx_reg9_set(0x000003AF);
    rc_trx_reg10_set(0x82383330);
    rc_trx_reg11_set(0x16A46D84);
    rc_trx_reg12_set(0x01A187DD);// //0x01A184CC 722  //0x01A187FF //0x01A1F4F1  Feb 09 2015 Allen
    rc_trx_reg13_set(0xFDF90238);
    rc_trx_reg14_set(0xFA01BCF0);
    rc_trx_reg15_set(0x40000030);
    rc_trx_reg16_set(0xF0000000);
    rc_trx_reg17_set(0x00000000);
    rc_trx_reg18_set(0x202D5D01);//0x206D5D01
    rc_trx_reg19_set(0x05164145);
    rc_trx_reg20_set(0x8080FF30); //
    rc_trx_reg21_set(0x80808080);
    rc_trx_reg22_set(0x80808080);
    rc_trx_reg23_set(0x80808080);
    rc_trx_reg24_set(0x80808080);
    rc_trx_reg25_set(0x80808080);
    rc_trx_reg26_set(0x80808080);
    rc_trx_reg27_set(0x80808080);
    rc_trx_reg28_set(0x2004AFF9);
#else
    rc_trx_reg0_set(0x0811EF5E);
    rc_trx_reg1_set(0x01000000);
    rc_trx_reg2_set(0x00000000);
    rc_trx_reg3_set(0x00000000);
    rc_trx_reg4_set(0x38025E88);
    rc_trx_reg5_set(0x2555EC7A);
    rc_trx_reg6_set(0x5FA4410C);
    rc_trx_reg7_set(0x202C48F5);
    rc_trx_reg8_set(0x076CA9CA);
    rc_trx_reg9_set(0x000003AF);
    rc_trx_reg10_set(0x82382330);
    rc_trx_reg11_set(0x28444884);
    rc_trx_reg12_set(0x01A183FD);
    rc_trx_reg13_set(0xDDF90339);
    rc_trx_reg14_set(0xDA01BCF0);
    rc_trx_reg15_set(0x00018000);
    rc_trx_reg16_set(0xD0000000);
    rc_trx_reg17_set(0x00000000);
    rc_trx_reg18_set(0xD0000C01);
    rc_trx_reg19_set(0x7B305ECC);
    rc_trx_reg20_set(0x827C827C);
    rc_trx_reg21_set(0x86788678);
    rc_trx_reg22_set(0x8C748C74);
    rc_trx_reg23_set(0xA45F9868);
    rc_trx_reg24_set(0xA45FA45F);
    rc_trx_reg25_set(0xA55EA45F);
    rc_trx_reg26_set(0xA55DA55E);
    rc_trx_reg27_set(0xA55DA55D);
    rc_trx_reg28_set(0x20000000);
#endif
#else

#endif

    while(rc_beken_spi_get() & 0x0fffffff);   //wait rf register write done

    rc_ch0_rx_onoff_delay_pack(  1, 1);
    rc_ch0_tx_onoff_delay_pack(  1, 1);   //old on delay 0x41
    rc_ch0_pa_onoff_delay_pack(  1, 0x10);//old on delay 0x41
    rc_ch0_shdn_onoff_delay_pack(1, 1);
    rc_ch0_en_setf(1);

    rc_rc_en_setf(1);
    PHY_WPRT("phy_rf_init-->RC static regs set and RC enabled\r\n");

    if (!phy_locked())
    {
        //todo do what???
    }
}

/**
 ****************************************************************************************
 * @brief Trident board components init
 ****************************************************************************************
 */
void phy_rcbeken_init()
{
#if !CFG_SUPPORT_CALIBRATION
#if (CFG_RUNNING_PLATFORM == FPGA_PLATFORM)
    rc_beken_spi_set(0x70000000);       //set spi_prescaler. spi clock frequency set 5Mbps
    rc_ch0_force_set(0x00010003);
    rc_misc_force_set(0x00000001);

    //rc_rx_avg_calc_set(0x2004aff9);   //huaming
    rc_rx_avg_calc_set(0x3004aff9);
    rc_rx_calib_en_set(0x00); // 0x4004aff9
    rc_rx_dc_wr_set(0x0004aff9);
    rc_rx_error_wr_set(0x01ff0000);
    rc_tx_mode_cfg_set(0x06000200);
    rc_tx_sin_cfg_set(0x2cc0c000);
    rc_tx_dc_comp_set(0x0202020b);
    rc_tx_gain_comp_set(0x03ff03e0); //20150722  0x03ff03e2

    rc_tx_phase_ty2_comp_set(0x021D01FE);
    rc_tx_other_cfg_set(0xc8ac9d40);

    rc_agc_cfg_set(0x00445460);
    rc_trx_spi_intlv_set(0x00000064);

#else
    rc_beken_spi_set(0xF0000000);       //set spi_prescaler. spi clock frequency set 5Mbps
    rc_ch0_force_set(0x00010005);
    rc_misc_force_set(0x00000002);

    //rc_rx_avg_calc_set(0x2004aff9);   //huaming
    rc_rx_avg_calc_set(0x1002DF4B);
    rc_rx_calib_en_set(0x00000000); // 0x4004aff9
    rc_rx_dc_wr_set(0x00000000);
    rc_rx_error_wr_set(0x020003EC);
    rc_tx_mode_cfg_set(0x218B018B);
    rc_tx_sin_cfg_set(0x2CC02000);
    rc_tx_dc_comp_set(0x020201F1);
    rc_tx_gain_comp_set(0x03FF03F3); //20150722  0x03ff03e2

    rc_tx_phase_ty2_comp_set(0x01F80200);
    rc_tx_other_cfg_set(0x48A79D40); // soc platform, rc_beken 0x1050148<31bit> should be 0

    rc_agc_cfg_set(0x0002567A);
    rc_trx_spi_intlv_set(0x80000064);

#endif
#else
    rwnx_cal_load_trx_rcbekn_reg_val();
#endif
    rc_dsel_va_setf(0x1); /* TSSI or AGC gain setting for versiong A,  output 0: TSSI 1:AGC gain setting*/
}

static void phy_trident_init(void)
{
    //IO Expander first - special component
    phy_iox_init();

    //RF Reset
    rc_spi_reset_setf(1);
    rc_spi_reset_setf(0);

    phy_rcbeken_init();		//rc_beken initial

    PHY_WPRT("phy_trident_init-->RF register reset done\r\n");

    //phy path to rf path mapping (given at phy init from nvds)

    //front end delay
    rc_fe_rx_del_set(0x12c);

    phy_adc12b_init();
    phy_adc8b_init();
}

static int force2040_toggle(uint8_t chantype)
{
    uint32_t rxmodes, f2040_val, f2040_msk;
    volatile uint32_t i;

    f2040_msk = MDM_FORCE40_BIT | MDM_FORCE20_BIT;
    if (chantype == PHY_CHNL_BW_20)
        f2040_val = MDM_FORCE20_BIT;
    else if (chantype == PHY_CHNL_BW_40)
        f2040_val = MDM_FORCE40_BIT;
    else
        f2040_val = 0;

    rxmodes = (mdm_rxmodes_get() & ~f2040_msk) | f2040_val;

    mdm_rxmodes_set(rxmodes);
    for(i = 0; i < 1000; i ++);

    /*
    modem reset
    */
    sddev_control(SCTRL_DEV_NAME, CMD_SCTRL_MODEM_CORE_RESET, 0);

    /* init some paramters of rf */
    phy_trident_init();
    phy_rf_init();

    return 0;
}

/**
 ****************************************************************************************
 * @brief Bandwidth change function.
 * This function is called during set channel procedure if the bandwidth configuration
 * has changed
 *
 * @param[in] bw  Bandwidth type
 ****************************************************************************************
 */
static void phy_change_bw(uint8_t bw)
{
    switch(bw)
    {
    case PHY_CHNL_BW_20:
        mdm_txstartdelay_setf(0x000000b4); 
        mdm_tbectrl0_set(0x0C0F0700);

#if (NX_MDM_VER == 11)
        mdm_tbectrl2_set(0x00F07005);
#endif

        agc_rwnxagcaci20marg0_set(0);
        agc_rwnxagcaci20marg1_set(0);
        agc_rwnxagcaci20marg2_set(0);
        break;

    case PHY_CHNL_BW_40:
        mdm_txstartdelay_setf(0x00000160);
        mdm_tbectrl0_set(0x0C0F0702);

#if (NX_MDM_VER == 11)
        mdm_tbectrl2_set(0x0FF07005);
#endif

        agc_rwnxagcaci20marg0_set(AGC_RWNXAGCACI20MARG0_RESET);
        agc_rwnxagcaci20marg1_set(AGC_RWNXAGCACI20MARG1_RESET);
        agc_rwnxagcaci20marg2_set(AGC_RWNXAGCACI20MARG2_RESET);
        break;

    case PHY_CHNL_BW_80:
    case PHY_CHNL_BW_160:
    case PHY_CHNL_BW_80P80:
        break;
    default:
        break;
    }

    force2040_toggle(bw);
}

void phy_init(const struct phy_cfg_tag *config)
{
    const struct phy_trd_cfg_tag *cfg = (const struct phy_trd_cfg_tag *)&config->parameters;

    //PHY ENVIRONMENT---------------------------------------------------------------------
    //mapping mdm phy paths to rf phy paths
    phy_env->phy2rf             = cfg->path_mapping & (0x0F);

    //Extract which RC and RF paths are used, MIMO or not
    phy_get_config(cfg->path_mapping);

    //Band = 2.4GHz by default
    phy_env->band                =
        phy_env->chnl_prim20_freq    =
            phy_env->chnl_center1_freq   =
                phy_env->chnl_center2_freq   =
                    phy_env->chnl_type           = PHY_UNUSED;

    PHY_WPRT("phy_init-->RC_PATHs=0x%X , RF_PATHS=0x%X\n", phy_env->rc_path_sel, phy_env->rf_path_sel);

    //PHY blocks initialization-----------------------------------------------------------
    //TRIDENT components
    phy_trident_init();

    //Radios alone through RC
    phy_rf_init();

    //MODEM - contains AGC?
    phy_mdm_init(cfg->tx_dc_off_comp);

    //AGC - separate or in MDM?
    phy_agc_init();

#if defined(CFG_SYS_CLOSE_CCA)
	extern uint8_t phy_close_cca(void);
	phy_close_cca();
#endif
}

void rcbeken_reconfigure(void)
{
    UINT32 tmp;

    phy_rcbeken_init();
    rc_fe_rx_del_set(0x12c);

    /*restore phy channel*/
    #if (CFG_SOC_NAME == SOC_BK7231)
    tmp = rc_trx_reg7_getf();
    tmp = (tmp & 0xff01ffff) | ((phy_env->chnl_center1_freq - 2400) << 17);
    rc_trx_reg7_set(tmp);
    #else
    tmp = rc_trx_reg5_getf();
    tmp = (tmp & 0x01ffffff) | ((phy_env->chnl_center1_freq - 2400) << 25);
    rc_trx_reg5_set(tmp);
    #endif // (CFG_SOC_NAME == SOC_BK7231)

    rc_ch0_rx_onoff_delay_pack(  1, 1);
    rc_ch0_tx_onoff_delay_pack(  1, 1);   //old on delay 0x41
    rc_ch0_pa_onoff_delay_pack(  1, 0x10);//old on delay 0x41
    rc_ch0_shdn_onoff_delay_pack(1, 1);
    rc_ch0_en_setf(1);
    rc_rc_en_setf(1);
}

#define RXVECT1_ARRAY_MAX       30

void phy_unsupported_modulation_check(void)
{
     uint32_t rx_vect0;
     uint32_t rx_vect1[RXVECT1_ARRAY_MAX];
     uint32_t rx_vect2;
     int8_t rx_rssi;
     uint16_t rx_length, mcs;
     int i;

    rx_vect0 = mdm_rxvector0_get();

    do
    {
        // Ensure that the packet could be a non-legacy one
        if ((rx_vect0 & MDM_RXLEGRATE_MASK) != (11 << MDM_RXLEGRATE_LSB))
            break;

        // Read the RSSI, which is already available
        rx_rssi = 0xFF00 | mdm_rssi1_getf();
        if (rx_rssi < -40)
            break;

        // Poll on the RXFORMAT until it is equal to HT, or timeout otherwise
        // This loop duration, if we timeout, shall be at least 4us
        for (i=0; i<100; i++)
        {
            rx_vect2 = mdm_rxvector2_get();
            if ((rx_vect2 & MDM_RXFORMAT_MASK) == 2)
                break;
        }

        // Check if the received format is HT
        if ((rx_vect2 & MDM_RXFORMAT_MASK) != 2)
            break;
		//os_printf("HT Format\r\n");
        // Now read the RX vector 1 several times and store each value
        // The duration between the first and the last read shall be around 6us
        // This reads are done sequentially and not in a loop because the RX
        // vector 1 is valid only for a few hundreds of ns and we don't want to
        // suffer from the overhead of the looping
        i = 0;
        rx_vect1[i++] = mdm_rxvector1_get();
        rx_vect1[i++] = mdm_rxvector1_get();
        rx_vect1[i++] = mdm_rxvector1_get();
        rx_vect1[i++] = mdm_rxvector1_get();
        rx_vect1[i++] = mdm_rxvector1_get();
        rx_vect1[i++] = mdm_rxvector1_get();
        rx_vect1[i++] = mdm_rxvector1_get();
        rx_vect1[i++] = mdm_rxvector1_get();
        rx_vect1[i++] = mdm_rxvector1_get();
        rx_vect1[i++] = mdm_rxvector1_get();
        rx_vect1[i++] = mdm_rxvector1_get();
        rx_vect1[i++] = mdm_rxvector1_get();
        rx_vect1[i++] = mdm_rxvector1_get();
        rx_vect1[i++] = mdm_rxvector1_get();
        rx_vect1[i++] = mdm_rxvector1_get();
        rx_vect1[i++] = mdm_rxvector1_get();
        rx_vect1[i++] = mdm_rxvector1_get();
        rx_vect1[i++] = mdm_rxvector1_get();
        rx_vect1[i++] = mdm_rxvector1_get();
        rx_vect1[i++] = mdm_rxvector1_get();
        rx_vect1[i++] = mdm_rxvector1_get();
        rx_vect1[i++] = mdm_rxvector1_get();
        rx_vect1[i++] = mdm_rxvector1_get();
        rx_vect1[i++] = mdm_rxvector1_get();
        rx_vect1[i++] = mdm_rxvector1_get();
        rx_vect1[i++] = mdm_rxvector1_get();
        rx_vect1[i++] = mdm_rxvector1_get();
        rx_vect1[i++] = mdm_rxvector1_get();
        rx_vect1[i++] = mdm_rxvector1_get();
        rx_vect1[i++] = mdm_rxvector1_get(); // Don't read beyond the array size you defined !!!

        // Search in the RX vector 1 array the index of the latest non-null
        // length value
        for (i=(RXVECT1_ARRAY_MAX - 1); i>=0; i--)
        {
            if ((rx_vect1[i] & MDM_RXHTLENGTH_MASK) != 0)
                break;
        }
        
        // If we did not find any non-null length, don't consider the packet
        if (i == -1)
            break;
		
    	//REG_WRITE( (0x0802800 +(17*4)), 0);
        //os_printf("HT Format Length %d (i=%d) rssi=%d\r\n",rx_vect1[i] & MDM_RXHTLENGTH_MASK,i,rx_rssi);
        // Check if the modulation is supported. We know that by checking if the
        // index of the latest non-null length is the last one. Indeed in case
        // the modulation is supported, the value of the rx vector 1 is kept valid
        // until the end of the reception. Otherwise it is reset only a few hundreds
        // of ns after becoming valid. Having a valid length in the latest RX vector 1
        // read therefore means that the modulation is supported and the packet will
        // be uploaded in the normal RX path
        if (i == (RXVECT1_ARRAY_MAX - 1))
            break;

        // Unsupported modulation received
        // TODO Forward the length and RSSI indication of this packet to the upper layers
        rx_length = rx_vect1[i] & MDM_RXHTLENGTH_MASK;

		mcs = ((rx_vect1[i] & MDM_RXMCS_MASK) >> MDM_RXMCS_LSB);
		//if((mcs < 8) || (mcs > 15))
		{
			//break;
		}

		#if 1
        os_printf("Length %d (%d-%d), rssi %d, mcs %d, idx %i\n", 
				rx_length,
				rx_vect1[i-1] & MDM_RXHTLENGTH_MASK,
   				rx_vect1[i+1] & MDM_RXHTLENGTH_MASK,
				rx_rssi, 
				mcs,
				i);
		#else
		hal_monitor_sig_content(100 + rx_rssi, rx_length);
		#endif
    } while(0);
}

extern void rwnx_cal_set_rxswitch(UINT32 enable);
extern UINT32 rwnx_cal_get_rxswitch(void);


void phy_enable_rx_switch(void)
{
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    rwnx_cal_set_rxswitch(1);
    GLOBAL_INT_RESTORE();

}

void phy_disable_rx_switch(void)
{
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    rwnx_cal_set_rxswitch(0);
    GLOBAL_INT_RESTORE();

}

static uint8_t large_singal_status;

uint8_t check_large_singal_status(void)
{
    return large_singal_status;
}

void update_large_singal_status(uint8_t status)
{
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    large_singal_status = status;
    GLOBAL_INT_RESTORE();
}

int large_signal_cnt;
int small_signal_cnt;
int no_action_signal_cnt;
void phy_large_signal_support(int8_t rssi)
{
    extern int large_signal_cnt;
    extern int small_signal_cnt;

    uint32_t rxswitch_en;

    GLOBAL_INT_DECLARATION();
    rxswitch_en = rwnx_cal_get_rxswitch();

    if (rssi >= 128)
    {
        rssi -= 256;
    }
    if (!rxswitch_en)
    {
        rssi += 15;
    }

    if (rssi >= -25)
    {
        large_signal_cnt ++;
        small_signal_cnt = 0;
        no_action_signal_cnt = 0;
    }
    else if (rssi <= -35)
    {
        large_signal_cnt = 0;
        small_signal_cnt ++;
        no_action_signal_cnt = 0;
    }
    else
    {
        no_action_signal_cnt++;
        if(no_action_signal_cnt >= large_signal_cnt)
            large_signal_cnt = 0;
        if(no_action_signal_cnt >= small_signal_cnt)
            small_signal_cnt = 0;
    }

    if (rxswitch_en && (large_signal_cnt >= 10))
    {
        //close rx switch
        GLOBAL_INT_DISABLE();
        large_singal_status = 1;
        phy_disable_rx_switch();
        GLOBAL_INT_RESTORE();
    }

    if(!rxswitch_en && (small_signal_cnt >= 10))
    {
        //open rx switch
        GLOBAL_INT_DISABLE();
        large_singal_status = 0;
        phy_enable_rx_switch();
        GLOBAL_INT_RESTORE();
    }
}

void phy_mdm_isr(void)
{
    uint32_t irq_status = mdm_irqstat_get();

    // Acknowledge the pending interrupts
    mdm_irqack_set(irq_status);	
	
    if (irq_status & MDM_IRQLSIGVALIDEN_BIT)
    {
#if CFG_RX_SENSITIVITY_TEST
        extern UINT32 g_rxsens_start;
        if (g_rxsens_start)
            phy_large_signal_support(mdm_rssi1_getf());
        else
#endif
        phy_unsupported_modulation_check();
    }

    ASSERT_REC(!(irq_status & MDM_IRQCCATIMEOUT_BIT));
}

void phy_rc_isr(void)
{
}

void phy_get_version(uint32_t *version_1, uint32_t *version_2)
{
    *version_1 = mdm_nxversion_get();
    // TODO Add version reading for other PHY elements than modem.
    *version_2 = 0;
}

extern void rwnx_cal_set_40M_setting(void);
void phy_set_channel(uint8_t band, uint8_t type, uint16_t prim20_freq,
                     uint16_t center1_freq, uint16_t center2_freq, uint8_t index)
{
    /*todo 11ac: when 2 radio transceivers on one path, careful how many writes go to both,
            and when the two need separate SPi commands*/
    uint8_t psel = 0;
    uint32_t tmp;

    //if same channel is set just skip
    if ((phy_env->band == band)
            && (phy_env->chnl_type == type)
            && (phy_env->chnl_prim20_freq == prim20_freq)
            && (phy_env->chnl_center1_freq == center1_freq)
            && (phy_env->chnl_center2_freq == center2_freq))
    {
        return;
    }

    PHY_WPRT("phy_set_channel-->center1:%d center2:%d\r\n", center1_freq, center2_freq);
    //todo add check for band 5+type 80, 160 and 80+80

    /* Every time a channel is changed, RC is disabled causing the entire PHY shut down.
     * Once all settings are made, RC is enable and SW polls status of wake up and lock
     * to know when ready to use
     */
    //Disable RC to turn everything off
    rc_rc_en_setf(0);
    //todo poll shutdown status bits for radios of interest

    if (phy_env->chnl_type != type)
    {
        phy_change_bw(type);
    }

    //keep currently set values
    phy_env->band              = band;
    phy_env->chnl_type         = type;
    phy_env->chnl_prim20_freq  = prim20_freq;
    phy_env->chnl_center1_freq = center1_freq;
    phy_env->chnl_center2_freq = center2_freq;

    //BAND--------------------------------------------------------------------------------
    //set band info where needed(RC, Radios, ...)
    phy_set_band(band, center1_freq);

    //TYPE--------------------------------------------------------------------------------
    //todo what values for LPF in RF?Now it's at default
    switch(type)
    {
    case PHY_CHNL_BW_20:
        psel = 0;
        break;

    case PHY_CHNL_BW_40:
        psel = (center1_freq + 10 - prim20_freq) / 20;
        break;

    case PHY_CHNL_BW_80:
        psel = (prim20_freq - (center1_freq - 30)) / 20;
    case PHY_CHNL_BW_160:
    case PHY_CHNL_BW_80P80:
        break;
    }

    //CHANNEL-----------------------------------------------------------------------------

    //Set Channel for Beken TRX
    #if (CFG_SOC_NAME == SOC_BK7231) 
    tmp = rc_trx_reg7_getf();
    tmp = (tmp & 0xff01ffff) | ((center1_freq - 2400) << 17);
    rc_trx_reg7_set(tmp);
    #else
    tmp = rc_trx_reg5_getf();
    tmp = (tmp & 0x01ffffff) | ((center1_freq - 2400) << 25);
    rc_trx_reg5_set(tmp);
    #endif // (CFG_SOC_NAME == SOC_BK7231)  

    // Set the PSEL value in modem
    mdm_psselect20_setf(psel & 0x01);
    mdm_psselect40_setf((psel & 0x02) >> 1);

    //Enable RC and thus trigger the configuration start
    rc_rc_en_setf(1);

    //Lock--------------------------------------------------------------------------------
    if (!phy_locked())
    {
        //failure measures - reset, shut down...
    }

    rc_ch0_tx_onoff_delay_set(0x00010010);
    rc_ch0_pa_onoff_delay_set(0x000A00E0);

    if(type == PHY_CHNL_BW_40) {
        rwnx_cal_set_40M_setting();
    }
    else
    {
#if (SOC_BK7231N == CFG_SOC_NAME)
		extern void rwnx_cal_set_20M_setting(void);
        rwnx_cal_set_20M_setting();
#endif
    }
    
}

void phy_get_channel(struct phy_channel_info *info, uint8_t index)
{
    // Map the band, channel type, primary channel index and center 1 index on info1
    info->info1 = phy_env->band | (phy_env->chnl_type << 8) | (phy_env->chnl_prim20_freq << 16);

    // Map center 2 index on info2
    info->info2 =  phy_env->chnl_center1_freq | (phy_env->chnl_center2_freq << 16);
}

void phy_stop(void)
{
    //RC disable only - everything will be shut down
    rc_rc_en_setf(0);

    //todo stop modem and trident components too?
    //todo anything else?
}


bool phy_has_radar_pulse(int rd_idx)
{
    return (mdm_radfifoempty_getf() == 0);
}

bool phy_get_radar_pulse(int rd_idx, struct phy_radar_pulse *pulse)
{
    // Check if FIFO is empty
    if (mdm_radfifoempty_getf())
        return (false);

    // FIFO is not empty, get an element
    pulse->pulse = REG_PL_RD(0x60C04000);

    return (true);
}

void phy_agc_reset(void)
{
    uint32_t mdm_reg;

    mdm_reg = mdm_swreset_get();
    mdm_reg |= MDM_MDMSWRESET_BIT;
    mdm_swreset_set(mdm_reg);

    bk_delay(10);

    mdm_reg = mdm_swreset_get();
    mdm_reg &= ~MDM_MDMSWRESET_BIT;
    mdm_swreset_set(mdm_reg);
}

bool phy_bfmee_supported(void)
{
    return false;
}


bool phy_bfmer_supported(void)
{
    return false;
}

bool phy_mu_mimo_rx_supported(void)
{
    return false;
}

bool phy_mu_mimo_tx_supported(void)
{
    return false;
}

#if RW_MUMIMO_RX_EN
void phy_set_group_id_info(uint32_t membership_addr, uint32_t userpos_addr)
{
}
#endif

uint8_t phy_get_nss(void)
{
    return 0;
}

uint8_t phy_get_ntx(void)
{
    return (0);
}

//cca ctrl
void phy_open_cca(void)
{
    agc_rwnxagcccactrl_set((agc_rwnxagcccactrl_get() & ~0x00000fff) | 0x00000377);
}

void phy_close_cca(void)
{
    agc_rwnxagcccactrl_set((agc_rwnxagcccactrl_get() & ~0x00000fff) & ~(0x00000377));
}

void phy_show_cca(void)
{
}
/**
 * TX power = idx * 0.5 + TX_GAIN_MIN + PA_GAIN
 *
 * idx = 2 * (TX power - TX_GAIN_MIN - PA_GAIN)
 *
 * idx = TX power - 5;//tpc
 */
void phy_get_rf_gain_idx(int8_t *power, uint8_t *idx)
{
    int8_t min, max, oft;

    if (phy_env->band == PHY_BAND_2G4)
    {
        min = PHY_TRIDENT_MIN_PWR_24G;
#if PHY_TRIDENT_LIMIT_PWR > PHY_TRIDENT_MAX_PWR_24G
        max = PHY_TRIDENT_MAX_PWR_24G;
#else
        max = PHY_TRIDENT_LIMIT_PWR;
#endif
        oft = (PHY_TRIDENT_TX_GAIN_MIN + PHY_TRIDENT_PA_GAIN_24G);
    }
    else
    {
        min = PHY_TRIDENT_MIN_PWR_5G;
#if PHY_TRIDENT_LIMIT_PWR > PHY_TRIDENT_MAX_PWR_24G
        max = PHY_TRIDENT_MAX_PWR_5G;
#else
        max = PHY_TRIDENT_LIMIT_PWR;
#endif
        oft = (PHY_TRIDENT_TX_GAIN_MIN + PHY_TRIDENT_PA_GAIN_5G);
    }

    if (*power > max)
    {
        *power = max;
    }
    else if (*power < min)
    {
        *power = min;
    }

    *idx = 2 * (*power - oft);
}

void phy_get_rf_gain_capab(int8_t *max, int8_t *min)
{
    if (phy_env->band == PHY_BAND_2G4)
    {
        *max = PHY_TRIDENT_MAX_PWR_24G;
        *min = PHY_TRIDENT_MIN_PWR_24G;
    }
    else
    {
        *max = PHY_TRIDENT_MAX_PWR_5G;
        *min = PHY_TRIDENT_MIN_PWR_5G;
    }
}

void rc_reset_patch(void)
{
    if(rc_rc_en_getf() == 0)
    {
        os_printf("********* RC_BEKEN_RESET ***********!\r\n");

        rcbeken_reconfigure();
        phy_agc_reset();
    }
}

void phy_init_after_wakeup(void)
{
    uint16_t freq;
	struct phy_cfg_tag cfg;

    freq = phy_env->chnl_center1_freq;
    phy_env->chnl_center1_freq = 0;

	cfg.parameters[0] = 1;
	cfg.parameters[1] = 0;
    phy_init(&cfg);
    
    phy_set_channel(PHY_BAND_2G4, PHY_CHNL_BW_20, freq, freq, 0, PHY_PRIM);
}

void phy_wakeup_rf_reinit(void)
{
    struct phy_env_tag phy_env_sleep;

    phy_env_sleep.band                = phy_env->band;
    phy_env_sleep.chnl_prim20_freq    = phy_env->chnl_prim20_freq;
    phy_env_sleep.chnl_center1_freq   = phy_env->chnl_center1_freq;
    phy_env_sleep.chnl_center2_freq   = phy_env->chnl_center2_freq;
    phy_env_sleep.chnl_type           = phy_env->chnl_type;

    phy_env->band                =
    phy_env->chnl_prim20_freq    =
    phy_env->chnl_center1_freq   =
    phy_env->chnl_center2_freq   =
    phy_env->chnl_type           = PHY_UNUSED;

    // recover trx setting
    rwnx_cal_recover_rf_setting();

    //MODEM - contains AGC?
    phy_mdm_init(0);

    // recover channel setting
    phy_set_channel(phy_env_sleep.band, phy_env_sleep.chnl_type, phy_env_sleep.chnl_prim20_freq,
                        phy_env_sleep.chnl_center1_freq, phy_env_sleep.chnl_center2_freq, PHY_PRIM);

}

void phy_wakeup_wifi_reinit(void)
{

    //AGC - separate or in MDM?
    phy_agc_init();

    rwnx_cal_recover_wifi_setting();
}
/// @}

