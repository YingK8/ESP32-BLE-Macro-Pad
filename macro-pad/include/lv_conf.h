#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 1  // ST7789 expects big-endian RGB565; swap bytes in buffer before flush

#define LV_TICK_CUSTOM 0

#define LV_USE_LOG 0
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0
#define LV_USE_FREETYPE 0       
#define LV_USE_FS_STDIO 0       
#define LV_USE_FS_LITTLEFS 0    
#define LV_USE_FS_POSIX 0       
#define LV_USE_THEME_BASIC 0    
#define LV_USE_THEME_MONO 0     
#define LV_USE_EXTRA 0  

#define LV_USE_LABEL 1

#define LV_FONT_MONTSERRAT_14 0  // disabled — project uses departure_mono_upper_28 exclusively
#define LV_FONT_MONTSERRAT_20 0
#define LV_FONT_MONTSERRAT_28 0
// unscii_8 is a tiny (~1 KB) built-in bitmap font used only as LV_FONT_DEFAULT placeholder.
// Our UI never hits the default (createUI sets departure_mono_upper_28 explicitly).
#define LV_FONT_UNSCII_8 1
#define LV_FONT_DEFAULT  &lv_font_unscii_8

#endif
