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
#include <linux/delay.h>

#include <video/mipi_display.h>

#include <drm/drm_crtc.h>
#include <drm/drm_device.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>


struct nwe080 {
	struct device *dev;
	struct drm_panel panel;
	struct gpio_desc *reset_gpio;
	struct regulator *supply;
	struct backlight_device *backlight;
	bool prepared;
	bool enabled;
};

static const struct drm_display_mode default_mode = {
	// panel-NWE080
	.clock = 70858,
	.hdisplay = 800,
	.hsync_start = 800 + 45,
	.hsync_end = 800 + 45 + 45,
	.htotal = 800 + 45 + 45 + 4,
	.vdisplay = 1280,
	.vsync_start = 1280 + 10,
	.vsync_end = 1280 + 10 + 25,
	.vtotal = 1280 + 10 + 25 + 6,
	.flags = 0,
	.width_mm = 107,
	.height_mm = 172,
};

static inline struct nwe080 *panel_to_nwe080(struct drm_panel *panel)
{
	return container_of(panel, struct nwe080, panel);
}

static int nwe080_switch_page(struct nwe080 *ctx, u8 page)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	u8 buf[4] = { 0xff, 0x98, 0x81, page };
	int ret;

	ret = mipi_dsi_dcs_write_buffer(dsi, buf, sizeof(buf));
	if (ret < 0)
		return ret;

	return 0;
}

static int nwe080_send_cmd_data(struct nwe080 *ctx, u8 cmd, u8 data)
{
	u8 buf[2] = { cmd, data };
	int ret;
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);

	ret = mipi_dsi_dcs_write_buffer(dsi, buf, sizeof(buf));
	if (ret < 0)
		return ret;

	return 0;
}

static void nwe080_init_sequence(struct nwe080 *ctx) {
	nwe080_switch_page(ctx, 3);
	//GIP_1
	nwe080_send_cmd_data(ctx, 0x01, 0x00);
	nwe080_send_cmd_data(ctx, 0x02, 0x00);
	nwe080_send_cmd_data(ctx, 0x03, 0x73);
	nwe080_send_cmd_data(ctx, 0x04, 0x00);
	nwe080_send_cmd_data(ctx, 0x05, 0x00);
	nwe080_send_cmd_data(ctx, 0x06, 0x0A);
	nwe080_send_cmd_data(ctx, 0x07, 0x00);
	nwe080_send_cmd_data(ctx, 0x08, 0x00);
	nwe080_send_cmd_data(ctx, 0x09, 0x20);
	nwe080_send_cmd_data(ctx, 0x0a, 0x20);
	nwe080_send_cmd_data(ctx, 0x0b, 0x00);
	nwe080_send_cmd_data(ctx, 0x0c, 0x00);
	nwe080_send_cmd_data(ctx, 0x0d, 0x00);
	nwe080_send_cmd_data(ctx, 0x0e, 0x00);
	nwe080_send_cmd_data(ctx, 0x0f, 0x1E);
	nwe080_send_cmd_data(ctx, 0x10, 0x1E);
	nwe080_send_cmd_data(ctx, 0x11, 0x00);
	nwe080_send_cmd_data(ctx, 0x12, 0x00);
	nwe080_send_cmd_data(ctx, 0x13, 0x00);
	nwe080_send_cmd_data(ctx, 0x14, 0x00);
	nwe080_send_cmd_data(ctx, 0x15, 0x00);
	nwe080_send_cmd_data(ctx, 0x16, 0x00);
	nwe080_send_cmd_data(ctx, 0x17, 0x00);
	nwe080_send_cmd_data(ctx, 0x18, 0x00);
	nwe080_send_cmd_data(ctx, 0x19, 0x00);
	nwe080_send_cmd_data(ctx, 0x1A, 0x00);
	nwe080_send_cmd_data(ctx, 0x1B, 0x00);
	nwe080_send_cmd_data(ctx, 0x1C, 0x00);
	nwe080_send_cmd_data(ctx, 0x1D, 0x00);
	nwe080_send_cmd_data(ctx, 0x1E, 0x40);
	nwe080_send_cmd_data(ctx, 0x1F, 0x80);
	nwe080_send_cmd_data(ctx, 0x20, 0x06);
	nwe080_send_cmd_data(ctx, 0x21, 0x01);
	nwe080_send_cmd_data(ctx, 0x22, 0x00);
	nwe080_send_cmd_data(ctx, 0x23, 0x00);
	nwe080_send_cmd_data(ctx, 0x24, 0x00);
	nwe080_send_cmd_data(ctx, 0x25, 0x00);
	nwe080_send_cmd_data(ctx, 0x26, 0x00);
	nwe080_send_cmd_data(ctx, 0x27, 0x00);
	nwe080_send_cmd_data(ctx, 0x28, 0x33);
	nwe080_send_cmd_data(ctx, 0x29, 0x03);
	nwe080_send_cmd_data(ctx, 0x2A, 0x00);
	nwe080_send_cmd_data(ctx, 0x2B, 0x00);
	nwe080_send_cmd_data(ctx, 0x2C, 0x00);
	nwe080_send_cmd_data(ctx, 0x2D, 0x00);
	nwe080_send_cmd_data(ctx, 0x2E, 0x00);
	nwe080_send_cmd_data(ctx, 0x2F, 0x00);
	nwe080_send_cmd_data(ctx, 0x30, 0x00);
	nwe080_send_cmd_data(ctx, 0x31, 0x00);
	nwe080_send_cmd_data(ctx, 0x32, 0x00);
	nwe080_send_cmd_data(ctx, 0x33, 0x00);
	nwe080_send_cmd_data(ctx, 0x34, 0x04);
	nwe080_send_cmd_data(ctx, 0x35, 0x00);
	nwe080_send_cmd_data(ctx, 0x36, 0x00);
	nwe080_send_cmd_data(ctx, 0x37, 0x00);
	nwe080_send_cmd_data(ctx, 0x38, 0x3C);
	nwe080_send_cmd_data(ctx, 0x39, 0x00);
	nwe080_send_cmd_data(ctx, 0x3A, 0x00);
	nwe080_send_cmd_data(ctx, 0x3B, 0x00);
	nwe080_send_cmd_data(ctx, 0x3C, 0x00);
	nwe080_send_cmd_data(ctx, 0x3D, 0x00);
	nwe080_send_cmd_data(ctx, 0x3E, 0x00);
	nwe080_send_cmd_data(ctx, 0x3F, 0x00);
	nwe080_send_cmd_data(ctx, 0x40, 0x00);
	nwe080_send_cmd_data(ctx, 0x41, 0x00);
	nwe080_send_cmd_data(ctx, 0x42, 0x00);
	nwe080_send_cmd_data(ctx, 0x43, 0x00);
	nwe080_send_cmd_data(ctx, 0x44, 0x00);

	nwe080_send_cmd_data(ctx, 0x50, 0x10);
	nwe080_send_cmd_data(ctx, 0x51, 0x32);
	nwe080_send_cmd_data(ctx, 0x52, 0x54);
	nwe080_send_cmd_data(ctx, 0x53, 0x76);
	nwe080_send_cmd_data(ctx, 0x54, 0x98);
	nwe080_send_cmd_data(ctx, 0x55, 0xba);
	nwe080_send_cmd_data(ctx, 0x56, 0x10);
	nwe080_send_cmd_data(ctx, 0x57, 0x32);
	nwe080_send_cmd_data(ctx, 0x58, 0x54);
	nwe080_send_cmd_data(ctx, 0x59, 0x76);
	nwe080_send_cmd_data(ctx, 0x5A, 0x98);
	nwe080_send_cmd_data(ctx, 0x5B, 0xba);
	nwe080_send_cmd_data(ctx, 0x5C, 0xdc);
	nwe080_send_cmd_data(ctx, 0x5D, 0xfe);

	//GIP_3
	nwe080_send_cmd_data(ctx, 0x5E, 0x00);
	nwe080_send_cmd_data(ctx, 0x5F, 0x01);
	nwe080_send_cmd_data(ctx, 0x60, 0x00);
	nwe080_send_cmd_data(ctx, 0x61, 0x15);
	nwe080_send_cmd_data(ctx, 0x62, 0x14);
	nwe080_send_cmd_data(ctx, 0x63, 0x0E);
	nwe080_send_cmd_data(ctx, 0x64, 0x0F);
	nwe080_send_cmd_data(ctx, 0x65, 0x0C);
	nwe080_send_cmd_data(ctx, 0x66, 0x0D);
	nwe080_send_cmd_data(ctx, 0x67, 0x06);
	nwe080_send_cmd_data(ctx, 0x68, 0x02);
	nwe080_send_cmd_data(ctx, 0x69, 0x02);
	nwe080_send_cmd_data(ctx, 0x6A, 0x02);
	nwe080_send_cmd_data(ctx, 0x6B, 0x02);
	nwe080_send_cmd_data(ctx, 0x6C, 0x02);
	nwe080_send_cmd_data(ctx, 0x6D, 0x02);
	nwe080_send_cmd_data(ctx, 0x6E, 0x07);
	nwe080_send_cmd_data(ctx, 0x6F, 0x02);
	nwe080_send_cmd_data(ctx, 0x70, 0x02);
	nwe080_send_cmd_data(ctx, 0x71, 0x02);
	nwe080_send_cmd_data(ctx, 0x72, 0x02);
	nwe080_send_cmd_data(ctx, 0x73, 0x02);
	nwe080_send_cmd_data(ctx, 0x74, 0x02);

	nwe080_send_cmd_data(ctx, 0x75, 0x01);
	nwe080_send_cmd_data(ctx, 0x76, 0x00);
	nwe080_send_cmd_data(ctx, 0x77, 0x14);
	nwe080_send_cmd_data(ctx, 0x78, 0x15);
	nwe080_send_cmd_data(ctx, 0x79, 0x0E);
	nwe080_send_cmd_data(ctx, 0x7A, 0x0F);
	nwe080_send_cmd_data(ctx, 0x7B, 0x0C);
	nwe080_send_cmd_data(ctx, 0x7C, 0x0D);
	nwe080_send_cmd_data(ctx, 0x7D, 0x06);
	nwe080_send_cmd_data(ctx, 0x7E, 0x02);
	nwe080_send_cmd_data(ctx, 0x7F, 0x02);
	nwe080_send_cmd_data(ctx, 0x80, 0x02);
	nwe080_send_cmd_data(ctx, 0x81, 0x02);
	nwe080_send_cmd_data(ctx, 0x82, 0x02);
	nwe080_send_cmd_data(ctx, 0x83, 0x02);
	nwe080_send_cmd_data(ctx, 0x84, 0x07);
	nwe080_send_cmd_data(ctx, 0x85, 0x02);
	nwe080_send_cmd_data(ctx, 0x86, 0x02);
	nwe080_send_cmd_data(ctx, 0x87, 0x02);
	nwe080_send_cmd_data(ctx, 0x88, 0x02);
	nwe080_send_cmd_data(ctx, 0x89, 0x02);
	nwe080_send_cmd_data(ctx, 0x8A, 0x02);

	nwe080_switch_page(ctx, 4);
	nwe080_send_cmd_data(ctx, 0x6C, 0x15);
	nwe080_send_cmd_data(ctx, 0x6E, 0x2A);

	//clamp 15V
	nwe080_send_cmd_data(ctx, 0x6F, 0x35);
	nwe080_send_cmd_data(ctx, 0x3A, 0x92);
	nwe080_send_cmd_data(ctx, 0x8D, 0x1F);
	nwe080_send_cmd_data(ctx, 0x87, 0xBA);
	nwe080_send_cmd_data(ctx, 0x26, 0x76);
	nwe080_send_cmd_data(ctx, 0xB2, 0xD1);
	nwe080_send_cmd_data(ctx, 0xB5, 0x27);
	nwe080_send_cmd_data(ctx, 0x31, 0x75);
	nwe080_send_cmd_data(ctx, 0x30, 0x03);
	nwe080_send_cmd_data(ctx, 0x3B, 0x98);
	nwe080_send_cmd_data(ctx, 0x35, 0x17);
	nwe080_send_cmd_data(ctx, 0x33, 0x14);
	nwe080_send_cmd_data(ctx, 0x38, 0x01);
	nwe080_send_cmd_data(ctx, 0x39, 0x00);

	nwe080_switch_page(ctx, 1);
	// direction rotate
	//nwe080_send_cmd_data(ctx, 0x22, 0x0B);
	nwe080_send_cmd_data(ctx, 0x22, 0x0A);
	//TBD >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	nwe080_send_cmd_data(ctx, 0x31, 0x00);		// nwe080_send_cmd_data(ctx, 0x31, 0x02);
	//DONE <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
	nwe080_send_cmd_data(ctx, 0x53, 0x63);
	nwe080_send_cmd_data(ctx, 0x55, 0x69);
	nwe080_send_cmd_data(ctx, 0x50, 0xC7);
	nwe080_send_cmd_data(ctx, 0x51, 0xC2);
	nwe080_send_cmd_data(ctx, 0x60, 0x26);

	nwe080_send_cmd_data(ctx, 0xA0, 0x08);
	nwe080_send_cmd_data(ctx, 0xA1, 0x0F);
	nwe080_send_cmd_data(ctx, 0xA2, 0x25);
	nwe080_send_cmd_data(ctx, 0xA3, 0x01);
	nwe080_send_cmd_data(ctx, 0xA4, 0x23);
	nwe080_send_cmd_data(ctx, 0xA5, 0x18);
	nwe080_send_cmd_data(ctx, 0xA6, 0x11);
	nwe080_send_cmd_data(ctx, 0xA7, 0x1A);
	nwe080_send_cmd_data(ctx, 0xA8, 0x81);
	nwe080_send_cmd_data(ctx, 0xA9, 0x19);
	nwe080_send_cmd_data(ctx, 0xAA, 0x26);
	nwe080_send_cmd_data(ctx, 0xAB, 0x7C);
	nwe080_send_cmd_data(ctx, 0xAC, 0x24);
	nwe080_send_cmd_data(ctx, 0xAD, 0x1E);
	nwe080_send_cmd_data(ctx, 0xAE, 0x5C);
	nwe080_send_cmd_data(ctx, 0xAF, 0x2A);
	nwe080_send_cmd_data(ctx, 0xB0, 0x2B);
	nwe080_send_cmd_data(ctx, 0xB1, 0x50);
	nwe080_send_cmd_data(ctx, 0xB2, 0x5C);
	nwe080_send_cmd_data(ctx, 0xB3, 0x39);

	nwe080_send_cmd_data(ctx, 0xC0, 0x08);
	nwe080_send_cmd_data(ctx, 0xC1, 0x1F);
	nwe080_send_cmd_data(ctx, 0xC2, 0x24);
	nwe080_send_cmd_data(ctx, 0xC3, 0x1D);
	nwe080_send_cmd_data(ctx, 0xC4, 0x04);
	nwe080_send_cmd_data(ctx, 0xC5, 0x32);
	nwe080_send_cmd_data(ctx, 0xC6, 0x24);
	nwe080_send_cmd_data(ctx, 0xC7, 0x1F);
	nwe080_send_cmd_data(ctx, 0xC8, 0x90);
	nwe080_send_cmd_data(ctx, 0xC9, 0x20);
	nwe080_send_cmd_data(ctx, 0xCA, 0x2C);
	nwe080_send_cmd_data(ctx, 0xCB, 0x82);
	nwe080_send_cmd_data(ctx, 0xCC, 0x19);
	nwe080_send_cmd_data(ctx, 0xCD, 0x22);
	nwe080_send_cmd_data(ctx, 0xCE, 0x4E);
	nwe080_send_cmd_data(ctx, 0xCF, 0x28);
	nwe080_send_cmd_data(ctx, 0xD0, 0x2D);
	nwe080_send_cmd_data(ctx, 0xD1, 0x51);
	nwe080_send_cmd_data(ctx, 0xD2, 0x5D);
	nwe080_send_cmd_data(ctx, 0xD3, 0x39);

 	///Bist Mode  Page4 0x2F 0x01
	// nwe080_switch_page(ctx, 4);
	// nwe080_send_cmd_data(ctx, 0x2F, 0x01);

	nwe080_switch_page(ctx, 0);
	//PWM
	nwe080_send_cmd_data(ctx, 0x51, 0x0F);
	nwe080_send_cmd_data(ctx, 0x52, 0xFF);
	nwe080_send_cmd_data(ctx, 0x53, 0x2C);


	nwe080_send_cmd_data(ctx, 0x11, 0x00);
	msleep(120);
	nwe080_send_cmd_data(ctx, 0x29, 0x00);
	msleep(20);
	nwe080_send_cmd_data(ctx, 0x35, 0x00);
}

static int nwe080_disable(struct drm_panel *panel)
{
	struct nwe080 *ctx = panel_to_nwe080(panel);

	if (!ctx->enabled)
		return 0;

	backlight_disable(ctx->backlight);

	ctx->enabled = false;

	return 0;
}

static int nwe080_unprepare(struct drm_panel *panel)
{
	struct nwe080 *ctx = panel_to_nwe080(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret)
		return ret;

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret)
		return ret;

	msleep(120);

	if (ctx->reset_gpio) {
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		msleep(20);
	}

	regulator_disable(ctx->supply);

	ctx->prepared = false;

	return 0;
}

static int nwe080_prepare(struct drm_panel *panel)
{
	struct nwe080 *ctx = panel_to_nwe080(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	int ret;

	if (ctx->prepared)
		return 0;

	ret = regulator_enable(ctx->supply);
	if (ret < 0)
		return ret;

	if (ctx->reset_gpio) {
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		msleep(20);
		gpiod_set_value_cansleep(ctx->reset_gpio, 0);
		msleep(100);
	}

	nwe080_init_sequence(ctx);

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

static int nwe080_enable(struct drm_panel *panel)
{
	struct nwe080 *ctx = panel_to_nwe080(panel);

	if (ctx->enabled)
		return 0;

	backlight_enable(ctx->backlight);

	ctx->enabled = true;

	return 0;
}

static int nwe080_get_modes(struct drm_panel *panel,
			struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		dev_err(panel->dev, "failed to add mode %ux%ux@%u\n",
			  default_mode.hdisplay, default_mode.vdisplay,
			  drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;

	return 1;
}

static const struct drm_panel_funcs nwe080_drm_funcs = {
	.disable = nwe080_disable,
	.unprepare = nwe080_unprepare,
	.prepare = nwe080_prepare,
	.enable = nwe080_enable,
	.get_modes = nwe080_get_modes,
};

static int nwe080_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct nwe080 *ctx;
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
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST | MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_EOT_PACKET | MIPI_DSI_MODE_LPM;

	drm_panel_init(&ctx->panel, &dsi->dev, &nwe080_drm_funcs, 
				DRM_MODE_CONNECTOR_DPI);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &nwe080_drm_funcs;

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "mipi_dsi_attach() failed: %d\n", ret);
		drm_panel_remove(&ctx->panel);
		return ret;
	}

	return 0;
}

static int nwe080_remove(struct mipi_dsi_device *dsi)
{
	struct nwe080 *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id nwe_nwe080_of_match[] = {
	{ .compatible = "nwe,nwe080" },
	{ }
};
MODULE_DEVICE_TABLE(of, nwe_nwe080_of_match);

static struct mipi_dsi_driver nwe_nwe080_driver = {
	.probe = nwe080_probe,
	.remove = nwe080_remove,
	.driver = {
		.name = "panel-nwe-nwe080",
		.of_match_table = nwe_nwe080_of_match,
	},
};
module_mipi_dsi_driver(nwe_nwe080_driver);

MODULE_AUTHOR("Philippe Cornu <philippe.cornu@st.com>");
MODULE_AUTHOR("Yannick Fertre <yannick.fertre@st.com>");
MODULE_AUTHOR("Henson Li <lidongsheng111@gmail.com>");
MODULE_DESCRIPTION("DRM Driver for nwe nwe080 MIPI DSI panel");
MODULE_LICENSE("GPL v2");
