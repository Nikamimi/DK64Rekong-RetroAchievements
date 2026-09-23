#include "ra_ui.h"
#include "ra_ui_api.h"

RECOMP_IMPORT(".", unsigned long dk64_ra_ui_revision(void));
RECOMP_IMPORT(".", unsigned long dk64_ra_ui_count(unsigned long filter));
RECOMP_IMPORT(".", unsigned long dk64_ra_ui_entry(unsigned long encoded, unsigned long field,
                                                   char* destination, unsigned long capacity));
RECOMP_IMPORT(".", unsigned long dk64_ra_ui_badge(unsigned long encoded,
                                                   unsigned char* destination, unsigned long capacity));
RECOMP_IMPORT(".", unsigned long dk64_ra_ui_summary(char* destination, unsigned long capacity));
RECOMP_IMPORT(".", unsigned long dk64_ra_ui_take_toast(char* destination, unsigned long capacity));
RECOMP_IMPORT(".", unsigned long dk64_ra_ui_toast_description(char* destination, unsigned long capacity));
RECOMP_IMPORT(".", unsigned long dk64_ra_ui_toast_badge(unsigned char* destination, unsigned long capacity));
RECOMP_IMPORT(".", unsigned long dk64_ra_ui_hotkey(void));
RECOMP_IMPORT(".", unsigned long dk64_ra_ui_reset_local(void));

/* The same retail input symbols used by Rekongpiled's optional warp menu.
 * R + D-pad Up opens the browser; the UI itself captures controller input. */
extern volatile unsigned short newly_pressed_input;
extern volatile unsigned short D_global_asm_807ECD58; /* OSContPad.button, first field */

enum { ROWS_PER_PAGE = 6, FILTER_ALL = 0, FILTER_LOCKED = 1, FILTER_UNLOCKED = 2 };
static const RaUiColor backdrop = { 0, 0, 0, 170 };
static const RaUiColor panel_color = { 16, 20, 31, 245 };
static const RaUiColor toast_color = { 24, 30, 43, 240 };
static const RaUiColor line_color = { 85, 109, 126, 180 };
static const RaUiColor white = { 246, 250, 251, 255 };
static const RaUiColor pale = { 187, 205, 213, 255 };
static const RaUiColor gold = { 255, 205, 98, 255 };

static RaUiContext browser_context, toast_context, reset_context;
static RaUiResource filter_radio, page_slider, summary_label, page_label, reset_button, reset_label;
static RaUiResource reset_choice;
static RaUiResource row[ROWS_PER_PAGE], title[ROWS_PER_PAGE];
static RaUiResource description[ROWS_PER_PAGE], status[ROWS_PER_PAGE];
static RaUiResource icon[ROWS_PER_PAGE];
static RaUiTexture placeholder_texture, row_texture[ROWS_PER_PAGE];
static RaUiTexture toast_texture;
static unsigned long row_key[ROWS_PER_PAGE];
static int row_badge_loaded[ROWS_PER_PAGE];
static RaUiResource toast_title, toast_description, toast_icon;
static unsigned long filter, page, last_revision, toast_frames;
static int browser_shown, toast_shown, toast_badge_loaded, initialized, reset_shown;
static char entry_buffer[256], summary_buffer[256], toast_buffer[256], toast_description_buffer[256], page_buffer[80];
static unsigned char icon_buffer[65536];
static const unsigned char placeholder_pixel[4] = { 49, 62, 74, 255 };
static const char* filter_options[] = { "All", "Locked", "Unlocked" };
/* A one-option radio has a click value we can poll: Rekongpiled 1.0.2 does
 * not dispatch mod button callbacks. Index 1 is the unselected idle state. */
static const char* reset_button_options[] = { "Reset local progress" };
static const char* reset_modal_options[] = { "Cancel", "Confirm reset" };

static void close_reset_modal(void) {
    recompui_hide_context(reset_context);
    reset_shown = 0;
    recompui_open_context(browser_context);
    recompui_set_input_value_u32(reset_button, 1);
    recompui_close_context(browser_context);
}

static char* append_uint(char* dest, unsigned long number) {
    char digits[12];
    unsigned count = 0;
    do { digits[count++] = (char)('0' + number % 10); number /= 10; } while (number && count < 11);
    while (count) *dest++ = digits[--count];
    return dest;
}

static char* append_text(char* dest, const char* text) {
    while (*text) *dest++ = *text++;
    return dest;
}

static RaUiResource label(RaUiContext context, RaUiResource parent,
                          const char* text, int kind, const RaUiColor* color) {
    RaUiResource item = recompui_create_label(context, parent, text, kind);
    recompui_set_color(item, color);
    return item;
}

static void reset_badge(unsigned i, unsigned long key) {
    if (row_key[i] == key) return;
    recompui_set_imageview_texture(icon[i], placeholder_texture);
    if (row_badge_loaded[i]) recompui_destroy_texture(row_texture[i]);
    row_badge_loaded[i] = 0;
    row_key[i] = key;
}

static void render_page(void) {
    /* Caller must own browser_context. The frame hook opens it explicitly. */
    unsigned long total = dk64_ra_ui_count(filter);
    unsigned long pages = (total + ROWS_PER_PAGE - 1) / ROWS_PER_PAGE;
    unsigned i;
    char* cursor;
    if (pages == 0) pages = 1;
    if (page >= pages) page = pages - 1;
    if (dk64_ra_ui_summary(summary_buffer, sizeof(summary_buffer))) {
        recompui_set_text(summary_label, summary_buffer);
    }
    for (i = 0; i < ROWS_PER_PAGE; ++i) {
        unsigned long index = page * ROWS_PER_PAGE + i;
        unsigned long encoded = (filter << 24) | index;
        if (index >= total) {
            reset_badge(i, 0xFFFFFFFFUL);
            recompui_set_display(row[i], RA_UI_DISPLAY_NONE);
            continue;
        }
        recompui_set_display(row[i], RA_UI_DISPLAY_FLEX);
        recompui_set_display(icon[i], RA_UI_DISPLAY_FLEX);
        /* The locked/unlocked badge can change without changing the row index. */
        if (dk64_ra_ui_entry(encoded, 3, entry_buffer, sizeof(entry_buffer))) {
            reset_badge(i, encoded * 2 + (entry_buffer[0] == '1'));
        }
        if (!row_badge_loaded[i]) {
            unsigned long bytes = dk64_ra_ui_badge(encoded, icon_buffer, sizeof(icon_buffer));
            if (bytes) {
                row_texture[i] = recompui_create_texture_image_bytes(icon_buffer, bytes);
                recompui_set_imageview_texture(icon[i], row_texture[i]);
                row_badge_loaded[i] = 1;
            }
        }
        if (dk64_ra_ui_entry(encoded, 0, entry_buffer, sizeof(entry_buffer))) {
            recompui_set_text(title[i], entry_buffer);
        }
        if (dk64_ra_ui_entry(encoded, 1, entry_buffer, sizeof(entry_buffer))) {
            recompui_set_text(description[i], entry_buffer);
        }
        if (dk64_ra_ui_entry(encoded, 2, entry_buffer, sizeof(entry_buffer))) {
            recompui_set_text(status[i], entry_buffer);
        }
    }
    if (!total) {
        recompui_set_display(row[0], RA_UI_DISPLAY_FLEX);
        recompui_set_display(icon[0], RA_UI_DISPLAY_NONE);
        recompui_set_text(title[0], filter == FILTER_UNLOCKED ?
            "No account unlocks yet" : "No achievements loaded for this filter");
        recompui_set_text(description[0], filter == FILTER_UNLOCKED ?
            "Local triggers and RA unlocks appear here." : "Check connection status above.");
        recompui_set_text(status[0], "");
    }
    cursor = append_text(page_buffer, "Page ");
    cursor = append_uint(cursor, page + 1);
    cursor = append_text(cursor, " / ");
    cursor = append_uint(cursor, pages);
    cursor = append_text(cursor, "  |  ");
    cursor = append_uint(cursor, total);
    cursor = append_text(cursor, " entries");
    *cursor = 0;
    recompui_set_text(page_label, page_buffer);
}

static void render_page_from_frame(void) {
    recompui_open_context(browser_context);
    render_page();
    recompui_set_input_value_u32(page_slider, page + 1);
    recompui_close_context(browser_context);
}

void dk64_ra_ui_init(void) {
    RaUiResource root, panel, controls, reset_controls, rows, footer, toast_root, card, details, text, toast_details, toast_text;
    RaUiResource reset_root, reset_card;
    unsigned i;
    if (initialized) return;
    initialized = 1;
    browser_context = recompui_create_context();
    recompui_open_context(browser_context);
    root = recompui_context_root(browser_context);
    recompui_set_position(root, RA_UI_POSITION_ABSOLUTE);
    recompui_set_left(root, 0, RA_UI_UNIT_DP);
    recompui_set_top(root, 0, RA_UI_UNIT_DP);
    recompui_set_right(root, 0, RA_UI_UNIT_DP);
    recompui_set_bottom(root, 0, RA_UI_UNIT_DP);
    recompui_set_width_auto(root);
    recompui_set_height_auto(root);
    recompui_set_display(root, RA_UI_DISPLAY_FLEX);
    recompui_set_justify_content(root, RA_UI_JUSTIFY_CENTER);
    recompui_set_align_items(root, RA_UI_ALIGN_CENTER);
    recompui_set_padding(root, 48, RA_UI_UNIT_DP);
    recompui_set_background_color(root, &backdrop);

    panel = recompui_create_element(browser_context, root);
    recompui_set_display(panel, RA_UI_DISPLAY_FLEX);
    recompui_set_flex_direction(panel, RA_UI_FLEX_COLUMN);
    recompui_set_width(panel, 100, RA_UI_UNIT_PERCENT);
    recompui_set_max_width(panel, 980, RA_UI_UNIT_DP);
    recompui_set_height(panel, 940, RA_UI_UNIT_DP);
    recompui_set_padding(panel, 28, RA_UI_UNIT_DP);
    recompui_set_gap(panel, 10, RA_UI_UNIT_DP);
    recompui_set_background_color(panel, &panel_color);
    recompui_set_border_width(panel, 2, RA_UI_UNIT_DP);
    recompui_set_border_color(panel, &line_color);
    recompui_set_border_radius(panel, 18, RA_UI_UNIT_DP);
    label(browser_context, panel, "DK64 RetroAchievements | PRIVATE BETA", RA_UI_LABEL_LARGE, &gold);
    summary_label = label(browser_context, panel, "Preparing achievement tracking", RA_UI_LABEL_SMALL, &pale);
    label(browser_context, panel, "Only 'Unlocked on RA' is a confirmed award; local triggers are saved separately.", RA_UI_LABEL_SMALL, &white);
    label(browser_context, panel, "F8 or R + D-pad Up: open  |  F8, Esc, or controller B: close", RA_UI_LABEL_SMALL, &pale);
    label(browser_context, panel, "Choose a filter and adjust the page slider", RA_UI_LABEL_SMALL, &pale);

    controls = recompui_create_element(browser_context, panel);
    recompui_set_display(controls, RA_UI_DISPLAY_FLEX);
    recompui_set_flex_direction(controls, RA_UI_FLEX_ROW);
    recompui_set_gap(controls, 12, RA_UI_UNIT_DP);
    filter_radio = recompui_create_labelradio(browser_context, controls, filter_options, 3);
    reset_controls = recompui_create_element(browser_context, panel);
    recompui_set_display(reset_controls, RA_UI_DISPLAY_FLEX);
    recompui_set_flex_direction(reset_controls, RA_UI_FLEX_ROW);
    recompui_set_gap(reset_controls, 12, RA_UI_UNIT_DP);
    reset_button = recompui_create_labelradio(browser_context, reset_controls, reset_button_options, 1);
    recompui_set_input_value_u32(reset_button, 1);
    recompui_set_padding(reset_button, 8, RA_UI_UNIT_DP);
    recompui_set_background_color(reset_button, &toast_color);
    recompui_set_border_width(reset_button, 1, RA_UI_UNIT_DP);
    recompui_set_border_color(reset_button, &line_color);
    recompui_set_border_radius(reset_button, 7, RA_UI_UNIT_DP);
    reset_label = label(browser_context, reset_controls, "Local history only; RA awards and DK64 saves stay intact.", RA_UI_LABEL_SMALL, &pale);

    rows = recompui_create_element(browser_context, panel);
    recompui_set_display(rows, RA_UI_DISPLAY_FLEX);
    recompui_set_flex_direction(rows, RA_UI_FLEX_COLUMN);
    placeholder_texture = recompui_create_texture_rgba32(placeholder_pixel, 1, 1);
    for (i = 0; i < ROWS_PER_PAGE; ++i) {
        row_key[i] = 0xFFFFFFFFUL;
        row[i] = recompui_create_element(browser_context, rows);
        recompui_set_display(row[i], RA_UI_DISPLAY_NONE);
        recompui_set_height(row[i], 100, RA_UI_UNIT_DP);
        recompui_set_border_bottom_width(row[i], 1, RA_UI_UNIT_DP);
        recompui_set_border_color(row[i], &line_color);
        details = recompui_create_element(browser_context, row[i]);
        recompui_set_display(details, RA_UI_DISPLAY_FLEX);
        recompui_set_flex_direction(details, RA_UI_FLEX_ROW);
        recompui_set_align_items(details, RA_UI_ALIGN_CENTER);
        recompui_set_gap(details, 12, RA_UI_UNIT_DP);
        icon[i] = recompui_create_imageview(browser_context, details, placeholder_texture);
        recompui_set_width(icon[i], 70, RA_UI_UNIT_DP);
        recompui_set_height(icon[i], 70, RA_UI_UNIT_DP);
        recompui_set_min_width(icon[i], 70, RA_UI_UNIT_DP);
        recompui_set_min_height(icon[i], 70, RA_UI_UNIT_DP);
        recompui_set_border_radius(icon[i], 6, RA_UI_UNIT_DP);
        text = recompui_create_element(browser_context, details);
        recompui_set_display(text, RA_UI_DISPLAY_FLEX);
        recompui_set_flex_direction(text, RA_UI_FLEX_COLUMN);
        title[i] = label(browser_context, text, "", RA_UI_LABEL_NORMAL, &white);
        description[i] = label(browser_context, text, "", RA_UI_LABEL_SMALL, &pale);
        status[i] = label(browser_context, text, "", RA_UI_LABEL_SMALL, &gold);
    }
    footer = recompui_create_element(browser_context, panel);
    recompui_set_display(footer, RA_UI_DISPLAY_FLEX);
    recompui_set_flex_direction(footer, RA_UI_FLEX_ROW);
    recompui_set_align_items(footer, RA_UI_ALIGN_CENTER);
    recompui_set_gap(footer, 16, RA_UI_UNIT_DP);
    label(browser_context, footer, "Page:", RA_UI_LABEL_NORMAL, &white);
    page_slider = recompui_create_slider(browser_context, footer, RA_UI_SLIDER_INTEGER, 1.0f, 28.0f, 1.0f, 1.0f);
    recompui_set_width(page_slider, 400, RA_UI_UNIT_DP);
    page_label = label(browser_context, footer, "Page 1 / 1", RA_UI_LABEL_NORMAL, &white);
    recompui_close_context(browser_context);

    reset_context = recompui_create_context();
    recompui_open_context(reset_context);
    reset_root = recompui_context_root(reset_context);
    recompui_set_position(reset_root, RA_UI_POSITION_ABSOLUTE);
    recompui_set_left(reset_root, 0, RA_UI_UNIT_DP);
    recompui_set_top(reset_root, 0, RA_UI_UNIT_DP);
    recompui_set_right(reset_root, 0, RA_UI_UNIT_DP);
    recompui_set_bottom(reset_root, 0, RA_UI_UNIT_DP);
    recompui_set_width_auto(reset_root);
    recompui_set_height_auto(reset_root);
    recompui_set_display(reset_root, RA_UI_DISPLAY_FLEX);
    recompui_set_justify_content(reset_root, RA_UI_JUSTIFY_CENTER);
    recompui_set_align_items(reset_root, RA_UI_ALIGN_CENTER);
    recompui_set_background_color(reset_root, &backdrop);
    reset_card = recompui_create_element(reset_context, reset_root);
    recompui_set_display(reset_card, RA_UI_DISPLAY_FLEX);
    recompui_set_flex_direction(reset_card, RA_UI_FLEX_COLUMN);
    recompui_set_width(reset_card, 670, RA_UI_UNIT_DP);
    recompui_set_padding(reset_card, 32, RA_UI_UNIT_DP);
    recompui_set_gap(reset_card, 22, RA_UI_UNIT_DP);
    recompui_set_background_color(reset_card, &panel_color);
    recompui_set_border_width(reset_card, 2, RA_UI_UNIT_DP);
    recompui_set_border_color(reset_card, &gold);
    recompui_set_border_radius(reset_card, 18, RA_UI_UNIT_DP);
    label(reset_context, reset_card, "Reset local achievement progress?", RA_UI_LABEL_LARGE, &gold);
    label(reset_context, reset_card,
          "This clears locally tracked triggers for the current profile and DK64 ROM. It does not reset your RA points or game saves.",
          RA_UI_LABEL_NORMAL, &white);
    reset_choice = recompui_create_labelradio(reset_context, reset_card, reset_modal_options, 2);
    recompui_set_input_value_u32(reset_choice, 2); /* no action preselected */
    recompui_close_context(reset_context);

    toast_context = recompui_create_context();
    recompui_set_context_captures_input(toast_context, 0);
    recompui_set_context_captures_mouse(toast_context, 0);
    recompui_open_context(toast_context);
    toast_root = recompui_context_root(toast_context);
    recompui_set_position(toast_root, RA_UI_POSITION_ABSOLUTE);
    recompui_set_left(toast_root, 0, RA_UI_UNIT_DP);
    recompui_set_top(toast_root, 0, RA_UI_UNIT_DP);
    recompui_set_right(toast_root, 0, RA_UI_UNIT_DP);
    recompui_set_bottom(toast_root, 0, RA_UI_UNIT_DP);
    card = recompui_create_element(toast_context, toast_root);
    recompui_set_position(card, RA_UI_POSITION_ABSOLUTE);
    recompui_set_right(card, 28, RA_UI_UNIT_DP);
    recompui_set_top(card, 28, RA_UI_UNIT_DP);
    recompui_set_width(card, 540, RA_UI_UNIT_DP);
    recompui_set_padding(card, 20, RA_UI_UNIT_DP);
    recompui_set_display(card, RA_UI_DISPLAY_FLEX);
    recompui_set_flex_direction(card, RA_UI_FLEX_COLUMN);
    recompui_set_background_color(card, &toast_color);
    recompui_set_border_width(card, 2, RA_UI_UNIT_DP);
    recompui_set_border_color(card, &gold);
    recompui_set_border_radius(card, 14, RA_UI_UNIT_DP);
    toast_details = recompui_create_element(toast_context, card);
    recompui_set_display(toast_details, RA_UI_DISPLAY_FLEX);
    recompui_set_flex_direction(toast_details, RA_UI_FLEX_ROW);
    recompui_set_align_items(toast_details, RA_UI_ALIGN_CENTER);
    recompui_set_gap(toast_details, 14, RA_UI_UNIT_DP);
    toast_icon = recompui_create_imageview(toast_context, toast_details, placeholder_texture);
    recompui_set_width(toast_icon, 80, RA_UI_UNIT_DP);
    recompui_set_height(toast_icon, 80, RA_UI_UNIT_DP);
    recompui_set_min_width(toast_icon, 80, RA_UI_UNIT_DP);
    recompui_set_min_height(toast_icon, 80, RA_UI_UNIT_DP);
    recompui_set_border_radius(toast_icon, 7, RA_UI_UNIT_DP);
    toast_text = recompui_create_element(toast_context, toast_details);
    recompui_set_display(toast_text, RA_UI_DISPLAY_FLEX);
    recompui_set_flex_direction(toast_text, RA_UI_FLEX_COLUMN);
    recompui_set_gap(toast_text, 5, RA_UI_UNIT_DP);
    toast_title = label(toast_context, toast_text, "", RA_UI_LABEL_NORMAL, &white);
    toast_description = label(toast_context, toast_text, "", RA_UI_LABEL_SMALL, &pale);
    recompui_close_context(toast_context);
    filter = FILTER_ALL;
    page = 0;
    last_revision = dk64_ra_ui_revision();
}

void dk64_ra_ui_frame(void) {
    unsigned long keyboard_open;
    if (!initialized) return;
    keyboard_open = dk64_ra_ui_hotkey();
    if (reset_shown) {
        unsigned long decision;
        recompui_open_context(reset_context);
        decision = recompui_get_input_value_u32(reset_choice);
        recompui_close_context(reset_context);
        if (keyboard_open || decision == 0 || decision == 1) {
            if (decision == 1) {
                recompui_open_context(browser_context);
                recompui_set_text(reset_label, dk64_ra_ui_reset_local() ?
                    "Local history cleared. Restart for fresh condition evaluation." :
                    "Reset unavailable: sign in or check local storage.");
                recompui_close_context(browser_context);
            }
            close_reset_modal();
        }
    } else if (keyboard_open && browser_shown) {
        recompui_hide_context(browser_context);
        browser_shown = 0;
    } else if (!browser_shown && ((keyboard_open & 1U) ||
        ((newly_pressed_input & 0x0800U) && (D_global_asm_807ECD58 & 0x0010U)))) {
        render_page_from_frame();
        last_revision = dk64_ra_ui_revision();
        recompui_show_context(browser_context);
        browser_shown = 1;
    }
    if (browser_shown && !reset_shown) {
        unsigned long selected, selected_page, reset_selection;
        recompui_open_context(browser_context);
        selected = recompui_get_input_value_u32(filter_radio);
        selected_page = recompui_get_input_value_u32(page_slider);
        reset_selection = recompui_get_input_value_u32(reset_button);
        if (selected > FILTER_UNLOCKED) selected = FILTER_ALL;
        if (selected != filter) {
            filter = selected;
            page = 0;
            recompui_set_input_value_u32(page_slider, 1);
            render_page();
        } else if (selected_page != page + 1) {
            page = selected_page > 0 ? selected_page - 1 : 0;
            render_page();
            if (page + 1 != selected_page) recompui_set_input_value_u32(page_slider, page + 1);
        }
        recompui_close_context(browser_context);
        if (reset_selection == 0) {
            recompui_open_context(reset_context);
            recompui_set_input_value_u32(reset_choice, 2);
            recompui_close_context(reset_context);
            recompui_show_context(reset_context);
            reset_shown = 1;
        }
    }
    if (browser_shown && dk64_ra_ui_revision() != last_revision) {
        render_page_from_frame();
        last_revision = dk64_ra_ui_revision();
    }
    if (!toast_shown && dk64_ra_ui_take_toast(toast_buffer, sizeof(toast_buffer))) {
        recompui_open_context(toast_context);
        recompui_set_imageview_texture(toast_icon, placeholder_texture);
        if (toast_badge_loaded) recompui_destroy_texture(toast_texture);
        toast_badge_loaded = 0;
        recompui_set_text(toast_title, toast_buffer);
        if (dk64_ra_ui_toast_description(toast_description_buffer, sizeof(toast_description_buffer)))
            recompui_set_text(toast_description, toast_description_buffer);
        else
            recompui_set_text(toast_description, "");
        recompui_close_context(toast_context);
        recompui_show_context(toast_context);
        toast_shown = 1;
        toast_frames = 240; /* ~8 seconds at the game's 30 Hz simulation */
    } else if (toast_shown && toast_frames && --toast_frames == 0) {
        recompui_hide_context(toast_context);
        toast_shown = 0;
    }
    if (toast_shown && !toast_badge_loaded) {
        unsigned long bytes = dk64_ra_ui_toast_badge(icon_buffer, sizeof(icon_buffer));
        if (bytes) {
            recompui_open_context(toast_context);
            toast_texture = recompui_create_texture_image_bytes(icon_buffer, bytes);
            recompui_set_imageview_texture(toast_icon, toast_texture);
            recompui_close_context(toast_context);
            toast_badge_loaded = 1;
        }
    }
}
