#ifndef DK64_RA_UI_API_H
#define DK64_RA_UI_API_H

#include "modding.h"

// Only the pinned Rekongpiled 1.0.2 recompui exports used by this mod.
// Signatures/enum values match the upstream mod template's recompui.h.
typedef unsigned long RaUiContext;
typedef unsigned long RaUiResource;
typedef unsigned long RaUiTexture;
typedef struct { unsigned char r, g, b, a; } RaUiColor;
enum { RA_UI_LABEL_SMALL = 1, RA_UI_LABEL_NORMAL = 2, RA_UI_LABEL_LARGE = 3 };
enum { RA_UI_SLIDER_INTEGER = 2 };
enum { RA_UI_DISPLAY_NONE = 0, RA_UI_DISPLAY_FLEX = 5 };
enum { RA_UI_POSITION_ABSOLUTE = 0 };
enum { RA_UI_UNIT_DP = 1, RA_UI_UNIT_PERCENT = 2 };
enum { RA_UI_JUSTIFY_CENTER = 2 };
enum { RA_UI_FLEX_ROW = 0, RA_UI_FLEX_COLUMN = 1 };
enum { RA_UI_ALIGN_CENTER = 2 };

RECOMP_IMPORT("*", RaUiContext recompui_create_context(void));
RECOMP_IMPORT("*", void recompui_open_context(RaUiContext context));
RECOMP_IMPORT("*", void recompui_close_context(RaUiContext context));
RECOMP_IMPORT("*", RaUiResource recompui_context_root(RaUiContext context));
RECOMP_IMPORT("*", void recompui_show_context(RaUiContext context));
RECOMP_IMPORT("*", void recompui_hide_context(RaUiContext context));
RECOMP_IMPORT("*", void recompui_set_context_captures_input(RaUiContext context, int value));
RECOMP_IMPORT("*", void recompui_set_context_captures_mouse(RaUiContext context, int value));
RECOMP_IMPORT("*", RaUiResource recompui_create_element(RaUiContext context, RaUiResource parent));
RECOMP_IMPORT("*", RaUiResource recompui_create_label(RaUiContext context, RaUiResource parent, const char* text, int style));
RECOMP_IMPORT("*", RaUiResource recompui_create_imageview(RaUiContext context, RaUiResource parent, RaUiTexture texture));
RECOMP_IMPORT("*", RaUiTexture recompui_create_texture_rgba32(const void* data, unsigned long width, unsigned long height));
RECOMP_IMPORT("*", RaUiTexture recompui_create_texture_image_bytes(const void* data, unsigned long length));
RECOMP_IMPORT("*", void recompui_set_imageview_texture(RaUiResource resource, RaUiTexture texture));
RECOMP_IMPORT("*", void recompui_destroy_texture(RaUiTexture texture));
RECOMP_IMPORT("*", RaUiResource recompui_create_labelradio(RaUiContext context, RaUiResource parent,
                                                           const char** options, unsigned long count));
RECOMP_IMPORT("*", RaUiResource recompui_create_slider(RaUiContext context, RaUiResource parent,
                                                       int type, float min, float max, float step, float initial));
RECOMP_IMPORT("*", unsigned long recompui_get_input_value_u32(RaUiResource resource));
RECOMP_IMPORT("*", void recompui_set_input_value_u32(RaUiResource resource, unsigned long value));
RECOMP_IMPORT("*", void recompui_set_text(RaUiResource resource, const char* text));
RECOMP_IMPORT("*", void recompui_set_display(RaUiResource resource, int display));
RECOMP_IMPORT("*", void recompui_set_position(RaUiResource resource, int position));
RECOMP_IMPORT("*", void recompui_set_left(RaUiResource resource, float value, int unit));
RECOMP_IMPORT("*", void recompui_set_top(RaUiResource resource, float value, int unit));
RECOMP_IMPORT("*", void recompui_set_right(RaUiResource resource, float value, int unit));
RECOMP_IMPORT("*", void recompui_set_bottom(RaUiResource resource, float value, int unit));
RECOMP_IMPORT("*", void recompui_set_width(RaUiResource resource, float value, int unit));
RECOMP_IMPORT("*", void recompui_set_min_width(RaUiResource resource, float value, int unit));
RECOMP_IMPORT("*", void recompui_set_width_auto(RaUiResource resource));
RECOMP_IMPORT("*", void recompui_set_height(RaUiResource resource, float value, int unit));
RECOMP_IMPORT("*", void recompui_set_min_height(RaUiResource resource, float value, int unit));
RECOMP_IMPORT("*", void recompui_set_height_auto(RaUiResource resource));
RECOMP_IMPORT("*", void recompui_set_max_width(RaUiResource resource, float value, int unit));
RECOMP_IMPORT("*", void recompui_set_padding(RaUiResource resource, float value, int unit));
RECOMP_IMPORT("*", void recompui_set_gap(RaUiResource resource, float value, int unit));
RECOMP_IMPORT("*", void recompui_set_border_width(RaUiResource resource, float value, int unit));
RECOMP_IMPORT("*", void recompui_set_border_bottom_width(RaUiResource resource, float value, int unit));
RECOMP_IMPORT("*", void recompui_set_border_radius(RaUiResource resource, float value, int unit));
RECOMP_IMPORT("*", void recompui_set_background_color(RaUiResource resource, const RaUiColor* color));
RECOMP_IMPORT("*", void recompui_set_border_color(RaUiResource resource, const RaUiColor* color));
RECOMP_IMPORT("*", void recompui_set_color(RaUiResource resource, const RaUiColor* color));
RECOMP_IMPORT("*", void recompui_set_justify_content(RaUiResource resource, int value));
RECOMP_IMPORT("*", void recompui_set_flex_direction(RaUiResource resource, int value));
RECOMP_IMPORT("*", void recompui_set_align_items(RaUiResource resource, int value));

#endif
