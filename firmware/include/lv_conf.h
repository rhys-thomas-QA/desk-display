#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

#if 1

/*====================
   COLOR SETTINGS
 *====================*/
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 1  /* Required for most SPI displays */

/*====================
   MEMORY SETTINGS
 *====================*/
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (48 * 1024U)  /* 48KB for LVGL heap */
#define LV_MEM_POOL_INCLUDE <stdlib.h>
#define LV_MEM_POOL_ALLOC malloc
#define LV_MEM_POOL_FREE  free

/*====================
   HAL SETTINGS
 *====================*/
#define LV_TICK_CUSTOM 0
#define LV_DPI_DEF 130

/*====================
   DRAWING
 *====================*/
#define LV_DRAW_COMPLEX 1
#define LV_SHADOW_CACHE_SIZE 0
#define LV_CIRCLE_CACHE_SIZE 4
#define LV_IMG_CACHE_DEF_SIZE 1
#define LV_GRADIENT_MAX_STOPS 2
#define LV_GRAD_CACHE_DEF_SIZE 0
#define LV_DITHER_GRADIENT 0
#define LV_DISP_ROT_MAX_BUF (10*1024)

/*====================
   LOGGING
 *====================*/
#define LV_USE_LOG 0

/*====================
   ASSERT
 *====================*/
#define LV_USE_ASSERT_NULL    0
#define LV_USE_ASSERT_MALLOC  0
#define LV_USE_ASSERT_STYLE   0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ     0

/*====================
   DEBUG
 *====================*/
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR  0
#define LV_USE_REFR_DEBUG   0
#define LV_USE_LAYER_DEBUG  0

/*====================
   STDLIB WRAPPER
 *====================*/
#define LV_USE_BUILTIN_MALLOC 1
#define LV_USE_BUILTIN_MEMCPY 1

/*====================
   SPRINTF
 *====================*/
#define LV_SPRINTF_CUSTOM 0
#define LV_SPRINTF_USE_FLOAT 0

/*====================
   GPU (none)
 *====================*/
#define LV_USE_GPU_STM32_DMA2D 0
#define LV_USE_GPU_SWM341_DBOX 0
#define LV_USE_GPU_NXP_PXP     0
#define LV_USE_GPU_NXP_VG_LITE 0
#define LV_USE_GPU_SDL         0

/*====================
   WIDGETS
 *====================*/
#define LV_USE_ARC        0
#define LV_USE_BAR        1
#define LV_USE_BTN        0
#define LV_USE_BTNMATRIX  0
#define LV_USE_CANVAS     0
#define LV_USE_CHECKBOX   0
#define LV_USE_DROPDOWN   0
#define LV_USE_IMG        0
#define LV_USE_LABEL      1
#  define LV_LABEL_TEXT_SELECTION 0
#  define LV_LABEL_LONG_TXT_HINT 0
#define LV_USE_LINE       0
#define LV_USE_ROLLER     0
#define LV_USE_SLIDER     0
#define LV_USE_SWITCH     0
#define LV_USE_TEXTAREA   0
#define LV_USE_TABLE      0

/*====================
   EXTRA WIDGETS (all off)
 *====================*/
#define LV_USE_ANIMIMG    0
#define LV_USE_CALENDAR   0
#define LV_USE_COLORWHEEL 0
#define LV_USE_IMGBTN     0
#define LV_USE_KEYBOARD   0
#define LV_USE_LED        0
#define LV_USE_LIST       0
#define LV_USE_MENU       0
#define LV_USE_METER      0
#define LV_USE_MSGBOX     0
#define LV_USE_SPAN       0
#define LV_USE_SPINBOX    0
#define LV_USE_SPINNER    0
#  define LV_SPINNER_DEF_ARC_LENGTH 60
#  define LV_SPINNER_DEF_SPEED      1000
#define LV_USE_TABVIEW    0
#define LV_USE_TILEVIEW   0
#define LV_USE_WIN        0
#define LV_USE_CHART      0

/*====================
   THEMES
 *====================*/
#define LV_USE_THEME_DEFAULT 0
#  define LV_THEME_DEFAULT_DARK          1
#  define LV_THEME_DEFAULT_GROW          0
#  define LV_THEME_DEFAULT_TRANSITION_TIME 80
#define LV_USE_THEME_BASIC 0
#define LV_USE_THEME_MONO  0

/*====================
   LAYOUTS
 *====================*/
#define LV_USE_FLEX 0
#define LV_USE_GRID 0

/*====================
   FONTS
 *====================*/
#define LV_FONT_MONTSERRAT_8   0
#define LV_FONT_MONTSERRAT_10  0
#define LV_FONT_MONTSERRAT_12  0
#define LV_FONT_MONTSERRAT_14  0
#define LV_FONT_MONTSERRAT_16  0
#define LV_FONT_MONTSERRAT_18  0
#define LV_FONT_MONTSERRAT_20  0
#define LV_FONT_MONTSERRAT_22  0
#define LV_FONT_MONTSERRAT_24  0
#define LV_FONT_MONTSERRAT_26  0
#define LV_FONT_MONTSERRAT_28  0
#define LV_FONT_MONTSERRAT_30  0
#define LV_FONT_MONTSERRAT_32  0
#define LV_FONT_MONTSERRAT_34  0
#define LV_FONT_MONTSERRAT_36  0
#define LV_FONT_MONTSERRAT_38  0
#define LV_FONT_MONTSERRAT_40  0
#define LV_FONT_MONTSERRAT_42  0
#define LV_FONT_MONTSERRAT_44  0
#define LV_FONT_MONTSERRAT_46  0
#define LV_FONT_MONTSERRAT_48  0

#define LV_FONT_CUSTOM_DECLARE LV_FONT_DECLARE(poppins_12) LV_FONT_DECLARE(poppins_16) LV_FONT_DECLARE(poppins_20) LV_FONT_DECLARE(lora_24)
#define LV_FONT_DEFAULT  &poppins_16
#define LV_FONT_UNSCII_8  0
#define LV_FONT_UNSCII_16 0
#define LV_USE_FONT_COMPRESSED 1

/*====================
   TEXT
 *====================*/
#define LV_TXT_ENC LV_TXT_ENC_UTF8
#define LV_TXT_BREAK_CHARS " ,.;:-_"
#define LV_TXT_LINE_BREAK_LONG_LEN      0
#define LV_TXT_LINE_BREAK_LONG_PRE_MIN_LEN  3
#define LV_TXT_LINE_BREAK_LONG_POST_MIN_LEN 3
#define LV_TXT_COLOR_CMD "#"
#define LV_USE_BIDI                   0
#define LV_USE_ARABIC_PERSIAN_CHARS   0

/*====================
   FILE SYSTEM (none)
 *====================*/
#define LV_USE_FS_STDIO    0
#define LV_USE_FS_POSIX    0
#define LV_USE_FS_WIN32    0
#define LV_USE_FS_LITTLEFS 0

/*====================
   MISC
 *====================*/
#define LV_USE_SNAPSHOT  0
#define LV_USE_MONKEY    0
#define LV_USE_GRIDNAV   0
#define LV_USE_FRAGMENT  0
#define LV_USE_IMGFONT   0
#define LV_USE_MSG       0
#define LV_USE_IME_PINYIN 0

/*====================
   DEMOS (off)
 *====================*/
#define LV_USE_DEMO_UNITY     0
#define LV_USE_DEMO_BENCHMARK 0
#define LV_USE_DEMO_STRESS    0
#define LV_USE_DEMO_MUSIC     0

#endif /* End of "Content enable" */
#endif /* LV_CONF_H */
