// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) STMicroelectronics SA 2017
 *
 * Authors: Philippe Cornu <philippe.cornu@st.com>
 *          Yannick Fertre <yannick.fertre@st.com>
 *          Henson Li <lidongsheng111@gmail.com>
 */

#include <linux/backlight.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>

#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

/*** Manufacturer Command Set ***/
#define MCS_CMD_MODE_SW		0xFE /* CMD Mode Switch */
#define MCS_CMD1_UCS		0x00 /* User Command Set (UCS = CMD1) */
#define MCS_CMD2_P0		0x01 /* Manufacture Command Set Page0 (CMD2 P0) */
#define MCS_CMD2_P1		0x02 /* Manufacture Command Set Page1 (CMD2 P1) */
#define MCS_CMD2_P2		0x03 /* Manufacture Command Set Page2 (CMD2 P2) */
#define MCS_CMD2_P3		0x04 /* Manufacture Command Set Page3 (CMD2 P3) */

/* CMD2 P0 commands (Display Options and Power) */
#define MCS_STBCTR		0x12 /* TE1 Output Setting Zig-Zag Connection */
#define MCS_SGOPCTR		0x16 /* Source Bias Current */
#define MCS_SDCTR		0x1A /* Source Output Delay Time */
#define MCS_INVCTR		0x1B /* Inversion Type */
#define MCS_EXT_PWR_IC		0x24 /* External PWR IC Control */
#define MCS_SETAVDD		0x27 /* PFM Control for AVDD Output */
#define MCS_SETAVEE		0x29 /* PFM Control for AVEE Output */
#define MCS_BT2CTR		0x2B /* DDVDL Charge Pump Control */
#define MCS_BT3CTR		0x2F /* VGH Charge Pump Control */
#define MCS_BT4CTR		0x34 /* VGL Charge Pump Control */
#define MCS_VCMCTR		0x46 /* VCOM Output Level Control */
#define MCS_SETVGN		0x52 /* VG M/S N Control */
#define MCS_SETVGP		0x54 /* VG M/S P Control */
#define MCS_SW_CTRL		0x5F /* Interface Control for PFM and MIPI */

/* CMD2 P2 commands (GOA Timing Control) - no description in datasheet */
#define GOA_VSTV1		0x00
#define GOA_VSTV2		0x07
#define GOA_VCLK1		0x0E
#define GOA_VCLK2		0x17
#define GOA_VCLK_OPT1		0x20
#define GOA_BICLK1		0x2A
#define GOA_BICLK2		0x37
#define GOA_BICLK3		0x44
#define GOA_BICLK4		0x4F
#define GOA_BICLK_OPT1		0x5B
#define GOA_BICLK_OPT2		0x60
#define MCS_GOA_GPO1		0x6D
#define MCS_GOA_GPO2		0x71
#define MCS_GOA_EQ		0x74
#define MCS_GOA_CLK_GALLON	0x7C
#define MCS_GOA_FS_SEL0		0x7E
#define MCS_GOA_FS_SEL1		0x87
#define MCS_GOA_FS_SEL2		0x91
#define MCS_GOA_FS_SEL3		0x9B
#define MCS_GOA_BS_SEL0		0xAC
#define MCS_GOA_BS_SEL1		0xB5
#define MCS_GOA_BS_SEL2		0xBF
#define MCS_GOA_BS_SEL3		0xC9
#define MCS_GOA_BS_SEL4		0xD3

/* CMD2 P3 commands (Gamma) */
#define MCS_GAMMA_VP		0x60 /* Gamma VP1~VP16 */
#define MCS_GAMMA_VN		0x70 /* Gamma VN1~VN16 */

struct jd9366 {
	struct device *dev;
	struct drm_panel panel;
	struct gpio_desc *reset_gpio;
	struct regulator *supply;
	struct backlight_device *backlight;
	bool prepared;
	bool enabled;
};

static const struct drm_display_mode default_mode = {
	.clock = 68430,
	.hdisplay = 800,
	.hsync_start = 800 + 16,
	.hsync_end = 800 + 16 + 48,
	.htotal = 800 + 16 + 48 + 16,
	.vdisplay = 1280,
	.vsync_start = 1280 + 8,
	.vsync_end = 1280 + 8 + 4,
	.vtotal = 1280 + 8 + 4 + 4,
	.vrefresh = 60,
	.flags = 0,
	.width_mm = 107,
	.height_mm = 172,
};

static inline struct jd9366 *panel_to_jd9366(struct drm_panel *panel)
{
	return container_of(panel, struct jd9366, panel);
}

static void jd9366_dcs_write_buf(struct jd9366 *ctx, const void *data,
				  size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	int err;

	err = mipi_dsi_dcs_write_buffer(dsi, data, len);
	if (err < 0)
		DRM_ERROR_RATELIMITED("MIPI DSI DCS write buffer failed: %d\n",
				      err);
}

static void jd9366_dcs_write_cmd(struct jd9366 *ctx, u8 cmd, u8 value)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	int err;

	err = mipi_dsi_dcs_write(dsi, cmd, &value, 1);
	if (err < 0)
		DRM_ERROR_RATELIMITED("MIPI DSI DCS write failed: %d\n", err);
}

#define dcs_write_seq(ctx, seq...)				\
({								\
	static const u8 d[] = { seq };				\
								\
	jd9366_dcs_write_buf(ctx, d, ARRAY_SIZE(d));		\
})

/*
 * This panel is not able to auto-increment all cmd addresses so for some of
 * them, we need to send them one by one...
 */
#define dcs_write_cmd_seq(ctx, cmd, seq...)			\
({								\
	static const u8 d[] = { seq };				\
	unsigned int i;						\
								\
	for (i = 0; i < ARRAY_SIZE(d) ; i++)			\
		jd9366_dcs_write_cmd(ctx, cmd + i, d[i]);	\
})

static void jd9366_init_sequence(struct jd9366 *ctx)
{
	//Page0
	dcs_write_seq(ctx, 0xE0,0x00);

	//--- PASSWORD  ----//
	dcs_write_seq(ctx, 0xE1,0x93);
	dcs_write_seq(ctx, 0xE2,0x65);
	dcs_write_seq(ctx, 0xE3,0xF8);


	//Page0
	dcs_write_seq(ctx, 0xE0,0x00);
	//--- Sequence Ctrl  ----//
	dcs_write_seq(ctx, 0x70,0x10);	//DC0,DC1
	dcs_write_seq(ctx, 0x71,0x13);	//DC2,DC3
	dcs_write_seq(ctx, 0x72,0x06);	//DC7
	dcs_write_seq(ctx, 0x80,0x03);	//0x03:4-Lane；0x02:3-Lane
	//--- Page4  ----//
	dcs_write_seq(ctx, 0xE0,0x04);
	dcs_write_seq(ctx, 0x2D,0x03);
	//--- Page1  ----//
	dcs_write_seq(ctx, 0xE0,0x01);

	//Set VCOM
	dcs_write_seq(ctx, 0x00,0x00);
	dcs_write_seq(ctx, 0x01,0xA0);
	//Set VCOM_Reverse
	dcs_write_seq(ctx, 0x03,0x00);
	dcs_write_seq(ctx, 0x04,0xA0);

	//Set Gamma Power, VGMP,VGMN,VGSP,VGSN
	dcs_write_seq(ctx, 0x17,0x00);
	dcs_write_seq(ctx, 0x18,0xB1);
	dcs_write_seq(ctx, 0x19,0x01);
	dcs_write_seq(ctx, 0x1A,0x00);
	dcs_write_seq(ctx, 0x1B,0xB1);  //VGMN=0
	dcs_write_seq(ctx, 0x1C,0x01);
		            
	//Set Gate Power
	dcs_write_seq(ctx, 0x1F,0x3E);     //VGH_R  = 15V                       
	dcs_write_seq(ctx, 0x20,0x2D);     //VGL_R  = -12V                      
	dcs_write_seq(ctx, 0x21,0x2D);     //VGL_R2 = -12V                      
	dcs_write_seq(ctx, 0x22,0x0E);     //PA[6]=0, PA[5]=0, PA[4]=0, PA[0]=0 

	//SETPANEL
	dcs_write_seq(ctx, 0x37,0x19);	//SS=1,BGR=1

	//SET RGBCYC
	dcs_write_seq(ctx, 0x38,0x05);	//JDT=101 zigzag inversion
	dcs_write_seq(ctx, 0x39,0x08);	//RGB_N_EQ1, modify 20140806
	dcs_write_seq(ctx, 0x3A,0x12);	//RGB_N_EQ2, modify 20140806
	dcs_write_seq(ctx, 0x3C,0x78);	//SET EQ3 for TE_H
	dcs_write_seq(ctx, 0x3E,0x80);	//SET CHGEN_OFF, modify 20140806 
	dcs_write_seq(ctx, 0x3F,0x80);	//SET CHGEN_OFF2, modify 20140806


	//Set TCON
	dcs_write_seq(ctx, 0x40,0x06);	//RSO=800 RGB
	dcs_write_seq(ctx, 0x41,0xA0);	//LN=640->1280 line

	//--- power voltage  ----//
	dcs_write_seq(ctx, 0x55,0x01);	//DCDCM=0001, JD PWR_IC
	dcs_write_seq(ctx, 0x56,0x01);
	dcs_write_seq(ctx, 0x57,0x69);
	dcs_write_seq(ctx, 0x58,0x0A);
	dcs_write_seq(ctx, 0x59,0x0A);	//VCL = -2.9V
	dcs_write_seq(ctx, 0x5A,0x28);	//VGH = 19V
	dcs_write_seq(ctx, 0x5B,0x19);	//VGL = -11V



	//--- Gamma  ----//
	dcs_write_seq(ctx, 0x5D,0x7C);              
	dcs_write_seq(ctx, 0x5E,0x65);      
	dcs_write_seq(ctx, 0x5F,0x53);    
	dcs_write_seq(ctx, 0x60,0x48);    
	dcs_write_seq(ctx, 0x61,0x43);    
	dcs_write_seq(ctx, 0x62,0x35);    
	dcs_write_seq(ctx, 0x63,0x39);    
	dcs_write_seq(ctx, 0x64,0x23);    
	dcs_write_seq(ctx, 0x65,0x3D);    
	dcs_write_seq(ctx, 0x66,0x3C);    
	dcs_write_seq(ctx, 0x67,0x3D);    
	dcs_write_seq(ctx, 0x68,0x5A);    
	dcs_write_seq(ctx, 0x69,0x46);    
	dcs_write_seq(ctx, 0x6A,0x57);    
	dcs_write_seq(ctx, 0x6B,0x4B);    
	dcs_write_seq(ctx, 0x6C,0x49);    
	dcs_write_seq(ctx, 0x6D,0x2F);    
	dcs_write_seq(ctx, 0x6E,0x03);    
	dcs_write_seq(ctx, 0x6F,0x00);    
	dcs_write_seq(ctx, 0x70,0x7C);    
	dcs_write_seq(ctx, 0x71,0x65);    
	dcs_write_seq(ctx, 0x72,0x53);    
	dcs_write_seq(ctx, 0x73,0x48);    
	dcs_write_seq(ctx, 0x74,0x43);    
	dcs_write_seq(ctx, 0x75,0x35);    
	dcs_write_seq(ctx, 0x76,0x39);    
	dcs_write_seq(ctx, 0x77,0x23);    
	dcs_write_seq(ctx, 0x78,0x3D);    
	dcs_write_seq(ctx, 0x79,0x3C);    
	dcs_write_seq(ctx, 0x7A,0x3D);    
	dcs_write_seq(ctx, 0x7B,0x5A);    
	dcs_write_seq(ctx, 0x7C,0x46);    
	dcs_write_seq(ctx, 0x7D,0x57);    
	dcs_write_seq(ctx, 0x7E,0x4B);    
	dcs_write_seq(ctx, 0x7F,0x49);    
	dcs_write_seq(ctx, 0x80,0x2F);    
	dcs_write_seq(ctx, 0x81,0x03);    
	dcs_write_seq(ctx, 0x82,0x00);    


	//Page2, for GIP
	dcs_write_seq(ctx, 0xE0,0x02);

	//GIP_L Pin mapping
	dcs_write_seq(ctx, 0x00,0x47);
	dcs_write_seq(ctx, 0x01,0x47);  
	dcs_write_seq(ctx, 0x02,0x45);
	dcs_write_seq(ctx, 0x03,0x45);
	dcs_write_seq(ctx, 0x04,0x4B);
	dcs_write_seq(ctx, 0x05,0x4B);
	dcs_write_seq(ctx, 0x06,0x49);
	dcs_write_seq(ctx, 0x07,0x49);
	dcs_write_seq(ctx, 0x08,0x41);
	dcs_write_seq(ctx, 0x09,0x1F);
	dcs_write_seq(ctx, 0x0A,0x1F);
	dcs_write_seq(ctx, 0x0B,0x1F);
	dcs_write_seq(ctx, 0x0C,0x1F);
	dcs_write_seq(ctx, 0x0D,0x1F);
	dcs_write_seq(ctx, 0x0E,0x1F);
	dcs_write_seq(ctx, 0x0F,0x43);
	dcs_write_seq(ctx, 0x10,0x1F);
	dcs_write_seq(ctx, 0x11,0x1F);
	dcs_write_seq(ctx, 0x12,0x1F);
	dcs_write_seq(ctx, 0x13,0x1F);
	dcs_write_seq(ctx, 0x14,0x1F);
	dcs_write_seq(ctx, 0x15,0x1F);

	//GIP_R Pin mapping
	dcs_write_seq(ctx, 0x16,0x46);
	dcs_write_seq(ctx, 0x17,0x46);
	dcs_write_seq(ctx, 0x18,0x44);
	dcs_write_seq(ctx, 0x19,0x44);
	dcs_write_seq(ctx, 0x1A,0x4A);
	dcs_write_seq(ctx, 0x1B,0x4A);
	dcs_write_seq(ctx, 0x1C,0x48);
	dcs_write_seq(ctx, 0x1D,0x48);
	dcs_write_seq(ctx, 0x1E,0x40);
	dcs_write_seq(ctx, 0x1F,0x1F);
	dcs_write_seq(ctx, 0x20,0x1F);
	dcs_write_seq(ctx, 0x21,0x1F);
	dcs_write_seq(ctx, 0x22,0x1F);
	dcs_write_seq(ctx, 0x23,0x1F);
	dcs_write_seq(ctx, 0x24,0x1F);
	dcs_write_seq(ctx, 0x25,0x42);
	dcs_write_seq(ctx, 0x26,0x1F);
	dcs_write_seq(ctx, 0x27,0x1F);
	dcs_write_seq(ctx, 0x28,0x1F);
	dcs_write_seq(ctx, 0x29,0x1F);
	dcs_write_seq(ctx, 0x2A,0x1F);
	dcs_write_seq(ctx, 0x2B,0x1F);
		                  
	//GIP_L_GS Pin mapping
	dcs_write_seq(ctx, 0x2C,0x11);
	dcs_write_seq(ctx, 0x2D,0x0F);   
	dcs_write_seq(ctx, 0x2E,0x0D); 
	dcs_write_seq(ctx, 0x2F,0x0B); 
	dcs_write_seq(ctx, 0x30,0x09); 
	dcs_write_seq(ctx, 0x31,0x07); 
	dcs_write_seq(ctx, 0x32,0x05); 
	dcs_write_seq(ctx, 0x33,0x18); 
	dcs_write_seq(ctx, 0x34,0x17); 
	dcs_write_seq(ctx, 0x35,0x1F); 
	dcs_write_seq(ctx, 0x36,0x01); 
	dcs_write_seq(ctx, 0x37,0x1F); 
	dcs_write_seq(ctx, 0x38,0x1F); 
	dcs_write_seq(ctx, 0x39,0x1F); 
	dcs_write_seq(ctx, 0x3A,0x1F); 
	dcs_write_seq(ctx, 0x3B,0x1F); 
	dcs_write_seq(ctx, 0x3C,0x1F); 
	dcs_write_seq(ctx, 0x3D,0x1F); 
	dcs_write_seq(ctx, 0x3E,0x1F); 
	dcs_write_seq(ctx, 0x3F,0x13); 
	dcs_write_seq(ctx, 0x40,0x1F); 
	dcs_write_seq(ctx, 0x41,0x1F);
	 
	//GIP_R_GS Pin mapping
	dcs_write_seq(ctx, 0x42,0x10);
	dcs_write_seq(ctx, 0x43,0x0E);   
	dcs_write_seq(ctx, 0x44,0x0C); 
	dcs_write_seq(ctx, 0x45,0x0A); 
	dcs_write_seq(ctx, 0x46,0x08); 
	dcs_write_seq(ctx, 0x47,0x06); 
	dcs_write_seq(ctx, 0x48,0x04); 
	dcs_write_seq(ctx, 0x49,0x18); 
	dcs_write_seq(ctx, 0x4A,0x17); 
	dcs_write_seq(ctx, 0x4B,0x1F); 
	dcs_write_seq(ctx, 0x4C,0x00); 
	dcs_write_seq(ctx, 0x4D,0x1F); 
	dcs_write_seq(ctx, 0x4E,0x1F); 
	dcs_write_seq(ctx, 0x4F,0x1F); 
	dcs_write_seq(ctx, 0x50,0x1F); 
	dcs_write_seq(ctx, 0x51,0x1F); 
	dcs_write_seq(ctx, 0x52,0x1F); 
	dcs_write_seq(ctx, 0x53,0x1F); 
	dcs_write_seq(ctx, 0x54,0x1F); 
	dcs_write_seq(ctx, 0x55,0x12); 
	dcs_write_seq(ctx, 0x56,0x1F); 
	dcs_write_seq(ctx, 0x57,0x1F); 

	//GIP Timing  
	dcs_write_seq(ctx, 0x58,0x40);
	dcs_write_seq(ctx, 0x59,0x00);
	dcs_write_seq(ctx, 0x5A,0x00);
	dcs_write_seq(ctx, 0x5B,0x30);
	dcs_write_seq(ctx, 0x5C,0x03);
	dcs_write_seq(ctx, 0x5D,0x30);
	dcs_write_seq(ctx, 0x5E,0x01);
	dcs_write_seq(ctx, 0x5F,0x02);
	dcs_write_seq(ctx, 0x60,0x00);
	dcs_write_seq(ctx, 0x61,0x01);
	dcs_write_seq(ctx, 0x62,0x02);
	dcs_write_seq(ctx, 0x63,0x03);
	dcs_write_seq(ctx, 0x64,0x6B);
	dcs_write_seq(ctx, 0x65,0x00);
	dcs_write_seq(ctx, 0x66,0x00);
	dcs_write_seq(ctx, 0x67,0x73);
	dcs_write_seq(ctx, 0x68,0x05);
	dcs_write_seq(ctx, 0x69,0x06);
	dcs_write_seq(ctx, 0x6A,0x6B);
	dcs_write_seq(ctx, 0x6B,0x08);
	dcs_write_seq(ctx, 0x6C,0x00);
	dcs_write_seq(ctx, 0x6D,0x04);
	dcs_write_seq(ctx, 0x6E,0x04);
	dcs_write_seq(ctx, 0x6F,0x88);
	dcs_write_seq(ctx, 0x70,0x00);
	dcs_write_seq(ctx, 0x71,0x00);
	dcs_write_seq(ctx, 0x72,0x06);
	dcs_write_seq(ctx, 0x73,0x7B);
	dcs_write_seq(ctx, 0x74,0x00);
	dcs_write_seq(ctx, 0x75,0x07);
	dcs_write_seq(ctx, 0x76,0x00);
	dcs_write_seq(ctx, 0x77,0x5D);
	dcs_write_seq(ctx, 0x78,0x17);
	dcs_write_seq(ctx, 0x79,0x1F);
	dcs_write_seq(ctx, 0x7A,0x00);
	dcs_write_seq(ctx, 0x7B,0x00);
	dcs_write_seq(ctx, 0x7C,0x00);
	dcs_write_seq(ctx, 0x7D,0x03);
	dcs_write_seq(ctx, 0x7E,0x7B);


	//Page1
	dcs_write_seq(ctx, 0xE0,0x01);
	dcs_write_seq(ctx, 0x0E,0x01);	//LEDON output VCSW2


	//Page3
	dcs_write_seq(ctx, 0xE0,0x03);
	dcs_write_seq(ctx, 0x98,0x2F);	//From 2E to 2F, LED_VOL

	//Page4
	dcs_write_seq(ctx, 0xE0,0x04);
	dcs_write_seq(ctx, 0x09,0x10);
	dcs_write_seq(ctx, 0x2B,0x2B);
	dcs_write_seq(ctx, 0x2E,0x44);

	//Page0
	dcs_write_seq(ctx, 0xE0,0x00);
	dcs_write_seq(ctx, 0xE6,0x02);
	dcs_write_seq(ctx, 0xE7,0x02);

	//SLP OUT
	//SSD_CMD(0x11);  	// SLPOUT
	dcs_write_seq(ctx, 0x11);
	//Delayms(120);
	msleep(120);


	//DISP ON

	//SSD_CMD(0x29);  	// DSPON
	dcs_write_seq(ctx, 0x29);
	//Delayms(5);
	msleep(5);

	//--- TE----//
	dcs_write_seq(ctx, 0x35,0x00);
}

static int jd9366_disable(struct drm_panel *panel)
{
	struct jd9366 *ctx = panel_to_jd9366(panel);

	if (!ctx->enabled)
		return 0;

	backlight_disable(ctx->backlight);

	ctx->enabled = false;

	return 0;
}

static int jd9366_unprepare(struct drm_panel *panel)
{
	struct jd9366 *ctx = panel_to_jd9366(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret)
		DRM_WARN("failed to set display off: %d\n", ret);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret)
		DRM_WARN("failed to enter sleep mode: %d\n", ret);

	msleep(120);

	if (ctx->reset_gpio) {
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		msleep(20);
	}

	regulator_disable(ctx->supply);

	ctx->prepared = false;

	return 0;
}

static int jd9366_prepare(struct drm_panel *panel)
{
	struct jd9366 *ctx = panel_to_jd9366(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	int ret;

	if (ctx->prepared)
		return 0;

	ret = regulator_enable(ctx->supply);
	if (ret < 0) {
		DRM_ERROR("failed to enable supply: %d\n", ret);
		return ret;
	}

	if (ctx->reset_gpio) {
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		msleep(20);
		gpiod_set_value_cansleep(ctx->reset_gpio, 0);
		msleep(100);
	}

	jd9366_init_sequence(ctx);

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret)
		return ret;

	msleep(125);

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret)
		return ret;

	msleep(20);

	ctx->prepared = true;

	return 0;
}

static int jd9366_enable(struct drm_panel *panel)
{
	struct jd9366 *ctx = panel_to_jd9366(panel);

	if (ctx->enabled)
		return 0;

	backlight_enable(ctx->backlight);

	ctx->enabled = true;

	return 0;
}

static int jd9366_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(panel->drm, &default_mode);
	if (!mode) {
		DRM_ERROR("failed to add mode %ux%ux@%u\n",
			  default_mode.hdisplay, default_mode.vdisplay,
			  default_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(panel->connector, mode);

	panel->connector->display_info.width_mm = mode->width_mm;
	panel->connector->display_info.height_mm = mode->height_mm;

	return 1;
}

static const struct drm_panel_funcs jd9366_drm_funcs = {
	.disable = jd9366_disable,
	.unprepare = jd9366_unprepare,
	.prepare = jd9366_prepare,
	.enable = jd9366_enable,
	.get_modes = jd9366_get_modes,
};

static int jd9366_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct jd9366 *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->reset_gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset_gpio)) {
		ret = PTR_ERR(ctx->reset_gpio);
		dev_err(dev, "cannot get reset GPIO: %d\n", ret);
		return ret;
	}

	ctx->supply = devm_regulator_get(dev, "power");
	if (IS_ERR(ctx->supply)) {
		ret = PTR_ERR(ctx->supply);
		dev_err(dev, "cannot get regulator: %d\n", ret);
		return ret;
	}

	ctx->backlight = devm_of_find_backlight(dev);
	if (IS_ERR(ctx->backlight))
		return PTR_ERR(ctx->backlight);

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_MODE_LPM;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &jd9366_drm_funcs;

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "mipi_dsi_attach() failed: %d\n", ret);
		drm_panel_remove(&ctx->panel);
		return ret;
	}

	return 0;
}

static int jd9366_remove(struct mipi_dsi_device *dsi)
{
	struct jd9366 *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id boe_jd9366_of_match[] = {
	{ .compatible = "boe,jd9366" },
	{ }
};
MODULE_DEVICE_TABLE(of, boe_jd9366_of_match);

static struct mipi_dsi_driver boe_jd9366_driver = {
	.probe = jd9366_probe,
	.remove = jd9366_remove,
	.driver = {
		.name = "panel-boe-jd9366",
		.of_match_table = boe_jd9366_of_match,
	},
};
module_mipi_dsi_driver(boe_jd9366_driver);

MODULE_AUTHOR("Philippe Cornu <philippe.cornu@st.com>");
MODULE_AUTHOR("Yannick Fertre <yannick.fertre@st.com>");
MODULE_AUTHOR("Henson Li <lidongsheng111@gmail.com>");
MODULE_DESCRIPTION("DRM Driver for BOE JD9366 MIPI DSI panel");
MODULE_LICENSE("GPL v2");
