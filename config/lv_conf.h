/**
 * @file lv_conf.h
 * @brief LVGL 9.1 configuration used by this repository's host builds.
 *
 * Copyright (c) 2026 boa-z
 * SPDX-License-Identifier: MIT
 *
 * Only the options that matter for headless host builds, CI and the example
 * program are pinned here; everything else falls back to the LVGL 9.1
 * defaults (see lv_conf_internal.h). This file is NOT the product
 * configuration of an embedded target: a real target provides its own
 * lv_conf.h (display, input, filesystem, fonts, memory) and only needs the
 * options that the widget relies on:
 *
 *   - LV_USE_DRAW_SW 1 (line drawing through lv_draw_line)
 *   - LV_USE_OS: any (the widget itself is OS agnostic)
 *   - LV_COLOR_DEPTH: 16 or 32 (styles are depth agnostic)
 */
#ifndef LV_CONF_H
#define LV_CONF_H

/* --- Colour and geometry --------------------------------------------------- */
#define LV_COLOR_DEPTH 16

/* --- OS / standard library ------------------------------------------------- */
#define LV_USE_OS LV_OS_NONE
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

/* --- Rendering ------------------------------------------------------------- */
#define LV_DEF_REFR_PERIOD 16
#define LV_USE_DRAW_SW 1
#define LV_USE_DRAW_SW_ASM LV_DRAW_SW_ASM_NONE
#define LV_USE_VECTOR_GRAPHIC 0
#define LV_USE_THORVG_INTERNAL 0

/* --- Diagnostics ----------------------------------------------------------- */
#define LV_USE_LOG 0
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_ASSERT_STYLE 0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ 0
#define LV_USE_SYSMON 0
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0

/* --- Misc ------------------------------------------------------------------ */
#define LV_USE_OBJ_PROPERTY 0
#define LV_USE_FLOAT 0
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14
#define LV_USE_THEME_DEFAULT 1

#endif /* LV_CONF_H */
