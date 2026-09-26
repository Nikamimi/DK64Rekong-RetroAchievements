#include "ra_ui.h"
#include "ra_ui_api.h"
#include "ra_browser.h"

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
RECOMP_IMPORT(".", unsigned long dk64_ra_ui_account(unsigned long field, char* destination, unsigned long capacity));
RECOMP_IMPORT(".", unsigned long dk64_ra_ui_account_action(unsigned long action));

RECOMP_IMPORT(".", unsigned long dk64_ra_ui_input(unsigned long command));

/* The browser owns input while shown. SDL is observed by the native companion;
 * Rekongpiled 1.0.2 does not pump registered mod hover/focus callbacks. */
extern volatile unsigned short newly_pressed_input;
extern volatile unsigned short D_global_asm_807ECD58;

enum { FILTER_ALL = 0, FILTER_LOCKED = 1, FILTER_UNLOCKED = 2 };
static const RaUiColor backdrop = { 0, 0, 0, 180 };
static const RaUiColor panel_color = { 30, 30, 32, 250 };
static const RaUiColor toast_color = { 45, 45, 48, 250 };
static const RaUiColor line_color = { 86, 87, 91, 255 };
static const RaUiColor white = { 246, 246, 246, 255 };
static const RaUiColor pale = { 187, 189, 195, 255 };
static const RaUiColor gold = { 255, 205, 98, 255 };
static const RaUiColor earned = { 102, 201, 132, 255 };

static RaUiContext browser_context, toast_context, reset_context, account_context;
static RaUiResource account_button, account_name, account_status, account_notice, account_back, account_action;
static RaUiResource summary_label, count_label, empty_label, reset_button, close_button, reset_label;
static RaUiResource filter_button[3], reset_choice;
static RaUiResource icon[RA_GRID_SLOTS], detail_icon, detail_title, detail_description, detail_status;
static RaUiTexture placeholder_texture, row_texture[RA_GRID_SLOTS], toast_texture;
static unsigned long row_key[RA_GRID_SLOTS];
static int row_badge_loaded[RA_GRID_SLOTS], row_unlocked[RA_GRID_SLOTS];
static RaUiResource toast_title, toast_description, toast_icon;
static unsigned long filter, first_row, total, detail_index, last_revision, toast_frames, badge_cursor;
static long focus = 0;
static int browser_shown, toast_shown, toast_badge_loaded, initialized, reset_shown, account_shown, account_signed_in, account_focus;
static char entry_buffer[1024], summary_buffer[512], toast_buffer[256], toast_description_buffer[256], count_buffer[80];
static unsigned char icon_buffer[65536];
static const unsigned char placeholder_pixel[4] = { 65, 66, 69, 255 };
static const char* filter_options[] = { "All", "Locked", "Unlocked" };
static const char* reset_modal_options[] = { "Cancel", "Confirm reset" };

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
static unsigned long read_uint(const char* text) {
    unsigned long value = 0;
    while (*text >= '0' && *text <= '9') value = value * 10 + *text++ - '0';
    return value;
}
static RaUiResource label(RaUiContext context, RaUiResource parent,
                          const char* text, int kind, const RaUiColor* color) {
    RaUiResource item = recompui_create_label(context, parent, text, kind);
    recompui_set_color(item, color);
    return item;
}
static void place(RaUiResource item, int x, int y, int w, int h) {
    recompui_set_position(item, RA_UI_POSITION_ABSOLUTE);
    recompui_set_left(item, (float)x, RA_UI_UNIT_DP);
    recompui_set_top(item, (float)y, RA_UI_UNIT_DP);
    recompui_set_width(item, (float)w, RA_UI_UNIT_DP);
    recompui_set_height(item, (float)h, RA_UI_UNIT_DP);
}
static RaUiResource positioned_label(RaUiContext context, RaUiResource parent, const char* text,
                                  int x, int y, int w, int h, int font, const RaUiColor* color) {
    RaUiResource item = label(context, parent, text, RA_UI_LABEL_SMALL, color);
    place(item, x, y, w, h);
    recompui_set_font_size(item, (float)font, RA_UI_UNIT_DP);
    return item;
}
static RaUiResource browser_label(RaUiResource parent, const char* text,
                                  int x, int y, int w, int h, int font, const RaUiColor* color) {
    return positioned_label(browser_context, parent, text, x, y, w, h, font, color);
}
static RaUiResource positioned_button(RaUiContext context, RaUiResource parent, const char* text, int x, int y, int w) {
    RaUiResource item = positioned_label(context, parent, text, x, y, w, 36, 18, &white);
    recompui_set_padding(item, 6, RA_UI_UNIT_DP);
    recompui_set_border_width(item, 2, RA_UI_UNIT_DP);
    recompui_set_border_radius(item, 5, RA_UI_UNIT_DP);
    return item;
}
static RaUiResource browser_button(RaUiResource parent, const char* text, int x, int y, int w) {
    return positioned_button(browser_context, parent, text, x, y, w);
}
static void render_focus(void) {
    unsigned i;
    for (i = 0; i < RA_GRID_SLOTS; ++i) {
        long index = (long)(first_row * RA_GRID_COLUMNS + i);
        recompui_set_border_color(icon[i], focus == index ? &gold : row_unlocked[i] ? &earned : &line_color);
    }
    for (i = 0; i < 3; ++i) {
        recompui_set_border_color(filter_button[i], focus == -(long)i - 1 ? &gold : &line_color);
        recompui_set_background_color(filter_button[i], filter == i ? &line_color : &toast_color);
        recompui_set_color(filter_button[i], filter == i ? &gold : &white);
    }
    recompui_set_border_color(reset_button, focus == RA_FOCUS_RESET ? &gold : &line_color);
    recompui_set_border_color(close_button, focus == RA_FOCUS_CLOSE ? &gold : &line_color);
    recompui_set_border_color(account_button, focus == RA_FOCUS_ACCOUNT ? &gold : &line_color);
}
static void render_detail(void) {
    unsigned long encoded, slot;
    if (!total) {
        recompui_set_display(detail_icon, RA_UI_DISPLAY_NONE);
        recompui_set_text(detail_title, "No achievements in this filter");
        recompui_set_text(detail_description, filter == FILTER_UNLOCKED ?
            "Your RA unlocks and locally tracked achievements will appear here." : "Check the connection status above or choose another filter.");
        recompui_set_text(detail_status, "");
        return;
    }
    if (detail_index >= total) detail_index = total - 1;
    encoded = (filter << 24) | detail_index;
    recompui_set_display(detail_icon, RA_UI_DISPLAY_FLEX);
    slot = detail_index - first_row * RA_GRID_COLUMNS;
    recompui_set_imageview_texture(detail_icon, slot < RA_GRID_SLOTS && row_badge_loaded[slot] ?
                                  row_texture[slot] : placeholder_texture);
    if (dk64_ra_ui_entry(encoded, 0, entry_buffer, sizeof(entry_buffer))) recompui_set_text(detail_title, entry_buffer);
    if (dk64_ra_ui_entry(encoded, 1, entry_buffer, sizeof(entry_buffer))) recompui_set_text(detail_description, entry_buffer);
    if (dk64_ra_ui_entry(encoded, 2, entry_buffer, sizeof(entry_buffer))) recompui_set_text(detail_status, entry_buffer);
}
static void render_grid(void) {
    unsigned i;
    char* cursor;
    total = dk64_ra_ui_count(filter);
    if (first_row > ra_browser_scroll_limit(total)) first_row = ra_browser_scroll_limit(total);
    if (focus >= 0 && (unsigned long)focus >= total) focus = total ? (long)total - 1 : RA_FOCUS_ALL;
    if (detail_index >= total) detail_index = total ? total - 1 : 0;
    if (dk64_ra_ui_summary(summary_buffer, sizeof(summary_buffer))) recompui_set_text(summary_label, summary_buffer);
    /* Detach the shared detail image before replacing any slot texture. */
    recompui_set_imageview_texture(detail_icon, placeholder_texture);
    for (i = 0; i < RA_GRID_SLOTS; ++i) {
        unsigned long index = first_row * RA_GRID_COLUMNS + i;
        unsigned long encoded = (filter << 24) | index;
        unsigned long key = 0xFFFFFFFFUL;
        if (index < total) {
            dk64_ra_ui_entry(encoded, 3, entry_buffer, sizeof(entry_buffer));
            row_unlocked[i] = entry_buffer[0] == '1';
            /* Identity, not filtered position: unlocks can reorder a filter. */
            if (dk64_ra_ui_entry(encoded, 4, entry_buffer, sizeof(entry_buffer)))
                key = read_uint(entry_buffer) * 2 + row_unlocked[i];
        }
        if (key != row_key[i]) {
            recompui_set_imageview_texture(icon[i], placeholder_texture);
            if (row_badge_loaded[i]) recompui_destroy_texture(row_texture[i]);
            row_badge_loaded[i] = 0;
            row_key[i] = key;
        }
        recompui_set_display(icon[i], index < total ? RA_UI_DISPLAY_FLEX : RA_UI_DISPLAY_NONE);
    }
    recompui_set_display(empty_label, total ? RA_UI_DISPLAY_NONE : RA_UI_DISPLAY_FLEX);
    cursor = append_uint(count_buffer, total);
    cursor = append_text(cursor, total == 1 ? " achievement" : " achievements");
    if (total > RA_GRID_SLOTS) cursor = append_text(cursor, "  |  Scroll or use arrows for more");
    *cursor = 0;
    recompui_set_text(count_label, count_buffer);
    render_detail();
    render_focus();
}
static void load_badges(void) {
    unsigned attempted, loaded = 0;
    /* Keep decoding bounded while downloading the whole visible grid in the
     * existing background cache. Never decode 144 PNGs in a single game frame. */
    for (attempted = 0; attempted < RA_GRID_SLOTS && loaded < 4; ++attempted) {
        unsigned i = (unsigned)(badge_cursor++ % RA_GRID_SLOTS);
        unsigned long index = first_row * RA_GRID_COLUMNS + i;
        if (index < total && !row_badge_loaded[i]) {
            unsigned long bytes = dk64_ra_ui_badge((filter << 24) | index, icon_buffer, sizeof(icon_buffer));
            if (bytes) {
                row_texture[i] = recompui_create_texture_image_bytes(icon_buffer, bytes);
                recompui_set_imageview_texture(icon[i], row_texture[i]);
                row_badge_loaded[i] = 1;
                if (index == detail_index) recompui_set_imageview_texture(detail_icon, row_texture[i]);
                ++loaded;
            }
        }
    }
}
static void close_browser(void) {
    dk64_ra_ui_input(0);
    recompui_hide_context(browser_context);
    browser_shown = 0;
}
static void close_reset_modal(void) {
    recompui_hide_context(reset_context);
    reset_shown = 0;
    dk64_ra_ui_input(3);
}
static void render_account_focus(void) {
    recompui_set_border_color(account_back, account_focus == 0 ? &gold : &line_color);
    recompui_set_border_color(account_action, account_focus == 1 ? &gold : &line_color);
}
static void render_account(void) {
    if (dk64_ra_ui_account(0, entry_buffer, sizeof(entry_buffer))) recompui_set_text(account_name, entry_buffer);
    if (dk64_ra_ui_account(1, entry_buffer, sizeof(entry_buffer))) recompui_set_text(account_status, entry_buffer);
    if (dk64_ra_ui_account(2, entry_buffer, sizeof(entry_buffer))) recompui_set_text(account_notice, entry_buffer);
    account_signed_in = dk64_ra_ui_account(3, entry_buffer, sizeof(entry_buffer)) && entry_buffer[0] == '1';
    recompui_set_text(account_action, account_signed_in ? "Log Out" : "Sign In");
    account_focus = 0;
    render_account_focus();
}
static void close_account(void) {
    recompui_hide_context(account_context);
    account_shown = 0;
    dk64_ra_ui_input(3);
}
static void select_focus(long next) {
    unsigned long previous_row = first_row;
    if (next == RA_FOCUS_NONE || next == focus) return;
    focus = next;
    if (focus >= 0) {
        detail_index = (unsigned long)focus;
        first_row = ra_browser_reveal(first_row, detail_index);
    }
    if (previous_row != first_row) render_grid();
    else { render_detail(); render_focus(); }
}

void dk64_ra_ui_init(void) {
    RaUiResource root, panel, detail_card, toast_root, card, toast_details, toast_text;
    RaUiResource reset_root, reset_card, account_root, account_card;
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
    recompui_set_background_color(root, &backdrop);
    panel = recompui_create_element(browser_context, root);
    recompui_set_position(panel, 1); /* relative: anchors absolute children */
    recompui_set_width(panel, RA_BROWSER_WIDTH, RA_UI_UNIT_DP);
    recompui_set_height(panel, RA_BROWSER_HEIGHT, RA_UI_UNIT_DP);
    recompui_set_background_color(panel, &panel_color);
    recompui_set_border_radius(panel, 14, RA_UI_UNIT_DP);
    browser_label(panel, "Donkey Kong 64", 48, 18, 850, 40, 32, &gold);
    account_button = browser_button(panel, "Account", 892, 22, 180);
    summary_label = browser_label(panel, "Preparing achievement tracking", 48, 64, 1024, 48, 18, &white);
    browser_label(panel, "Hover a badge or use arrows / D-pad / stick  |  Enter / A: select  |  Tab: next section  |  F8 / Esc / B: close",
                  48, 115, 1024, 24, 16, &pale);
    for (i = 0; i < 3; ++i) filter_button[i] = browser_button(panel, filter_options[i], 48 + i * 136, 144, 128);
    count_label = browser_label(panel, "", 490, 151, 580, 30, 16, &pale);
    placeholder_texture = recompui_create_texture_rgba32(placeholder_pixel, 1, 1);
    for (i = 0; i < RA_GRID_SLOTS; ++i) {
        row_key[i] = 0xFFFFFFFFUL;
        icon[i] = recompui_create_imageview(browser_context, panel, placeholder_texture);
        place(icon[i], RA_GRID_X + (i % RA_GRID_COLUMNS) * RA_GRID_PITCH,
              RA_GRID_Y + (i / RA_GRID_COLUMNS) * RA_GRID_PITCH, RA_BADGE_SIZE, RA_BADGE_SIZE);
        recompui_set_border_width(icon[i], 2, RA_UI_UNIT_DP);
        recompui_set_display(icon[i], RA_UI_DISPLAY_NONE);
    }
    empty_label = browser_label(panel, "No badges to display. Choose a filter or check the connection status.", 64, 380, 1000, 80, 24, &pale);
    detail_card = recompui_create_element(browser_context, panel);
    place(detail_card, 48, 782, 1024, 158);
    recompui_set_background_color(detail_card, &toast_color);
    recompui_set_border_radius(detail_card, 8, RA_UI_UNIT_DP);
    detail_icon = recompui_create_imageview(browser_context, detail_card, placeholder_texture);
    place(detail_icon, 16, 20, 110, 110);
    detail_title = browser_label(detail_card, "", 146, 10, 856, 36, 24, &white);
    detail_description = browser_label(detail_card, "", 146, 48, 856, 72, 18, &pale);
    detail_status = browser_label(detail_card, "", 146, 126, 856, 26, 17, &gold);
    reset_button = browser_button(panel, "Reset local progress", 48, 950, 246);
    close_button = browser_button(panel, "Close", 918, 950, 154);
    reset_label = browser_label(panel, "Only 'Unlocked on RA' confirms an RA award.", 314, 959, 590, 28, 15, &pale);
    render_grid();
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

    account_context = recompui_create_context();
    recompui_open_context(account_context);
    account_root = recompui_context_root(account_context);
    recompui_set_position(account_root, RA_UI_POSITION_ABSOLUTE);
    recompui_set_left(account_root, 0, RA_UI_UNIT_DP);
    recompui_set_top(account_root, 0, RA_UI_UNIT_DP);
    recompui_set_right(account_root, 0, RA_UI_UNIT_DP);
    recompui_set_bottom(account_root, 0, RA_UI_UNIT_DP);
    recompui_set_width_auto(account_root);
    recompui_set_height_auto(account_root);
    recompui_set_display(account_root, RA_UI_DISPLAY_FLEX);
    recompui_set_justify_content(account_root, RA_UI_JUSTIFY_CENTER);
    recompui_set_align_items(account_root, RA_UI_ALIGN_CENTER);
    recompui_set_background_color(account_root, &backdrop);
    account_card = recompui_create_element(account_context, account_root);
    recompui_set_position(account_card, 1);
    recompui_set_width(account_card, RA_ACCOUNT_WIDTH, RA_UI_UNIT_DP);
    recompui_set_height(account_card, RA_ACCOUNT_HEIGHT, RA_UI_UNIT_DP);
    recompui_set_background_color(account_card, &panel_color);
    recompui_set_border_width(account_card, 2, RA_UI_UNIT_DP);
    recompui_set_border_color(account_card, &gold);
    recompui_set_border_radius(account_card, 18, RA_UI_UNIT_DP);
    positioned_label(account_context, account_card, "RetroAchievements Account", 32, 26, 696, 44, 32, &gold);
    account_name = positioned_label(account_context, account_card, "", 32, 84, 696, 42, 28, &white);
    account_status = positioned_label(account_context, account_card, "", 32, 138, 696, 56, 22, &white);
    account_notice = positioned_label(account_context, account_card, "", 32, 202, 696, 72, 20, &gold);
    account_back = positioned_button(account_context, account_card, "Back", 32, RA_ACCOUNT_BUTTON_Y, 150);
    account_action = positioned_button(account_context, account_card, "Sign In", 202, RA_ACCOUNT_BUTTON_Y, 180);
    render_account();
    recompui_close_context(account_context);

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
    last_revision = dk64_ra_ui_revision();
}

void dk64_ra_ui_frame(void) {
    unsigned long keyboard_open;
    if (!initialized) return;
    keyboard_open = dk64_ra_ui_hotkey();
    if (account_shown) {
        unsigned long input = dk64_ra_ui_input(1);
        int activate = (input & RA_INPUT_ACCEPT) != 0;
        account_focus = ra_account_move(account_focus, input);
        if (input & (RA_INPUT_POINTER | RA_INPUT_CLICK)) {
            unsigned long pointer = dk64_ra_ui_input(2);
            int hit = pointer == 0xFFFFFFFFUL ? -1 : ra_account_hit((int)(pointer & 65535), (int)(pointer >> 16));
            if (hit >= 0) account_focus = hit;
            if ((input & RA_INPUT_CLICK) && hit >= 0) activate = 1;
        }
        recompui_open_context(account_context);
        render_account_focus();
        recompui_close_context(account_context);
        if (keyboard_open || (activate && account_focus == 0)) close_account();
        else if (activate && account_focus == 1) {
            dk64_ra_ui_input(0);
            dk64_ra_ui_account_action(account_signed_in ? 0 : 1);
            recompui_open_context(account_context);
            render_account();
            recompui_close_context(account_context);
            dk64_ra_ui_input(3);
        }
    } else if (reset_shown) {
        unsigned long decision;
        recompui_open_context(reset_context);
        decision = recompui_get_input_value_u32(reset_choice);
        recompui_close_context(reset_context);
        if (keyboard_open || decision == 0 || decision == 1) {
            if (decision == 1) {
                recompui_open_context(browser_context);
                recompui_set_text(reset_label, dk64_ra_ui_reset_local() ?
                    "Local history cleared. Restart for fresh evaluation." : "Reset unavailable: check local storage.");
                recompui_close_context(browser_context);
            }
            close_reset_modal();
        }
    } else if (keyboard_open && browser_shown) {
        close_browser();
    } else if (!browser_shown && ((keyboard_open & 1U) ||
        ((newly_pressed_input & 0x0800U) && (D_global_asm_807ECD58 & 0x0010U)))) {
        recompui_open_context(browser_context);
        render_grid();
        recompui_close_context(browser_context);
        last_revision = dk64_ra_ui_revision();
        recompui_show_context(browser_context);
        browser_shown = 1;
        dk64_ra_ui_input(3);
    }
    if (browser_shown && !reset_shown && !account_shown) {
        unsigned long input = dk64_ra_ui_input(1);
        int activate = (input & RA_INPUT_ACCEPT) != 0;
        recompui_open_context(browser_context);
        if (dk64_ra_ui_revision() != last_revision) {
            render_grid();
            last_revision = dk64_ra_ui_revision();
        }
        if (input & (RA_INPUT_POINTER | RA_INPUT_CLICK)) {
            unsigned long pointer = dk64_ra_ui_input(2);
            long hit = pointer == 0xFFFFFFFFUL ? RA_FOCUS_NONE :
                ra_browser_hit((int)(pointer & 65535), (int)(pointer >> 16), first_row, total);
            select_focus(hit);
            if ((input & RA_INPUT_CLICK) && hit != RA_FOCUS_NONE) activate = 1;
        }
        if (input & (RA_INPUT_UP | RA_INPUT_DOWN | RA_INPUT_LEFT | RA_INPUT_RIGHT |
                     RA_INPUT_TAB | RA_INPUT_HOME | RA_INPUT_END))
            select_focus(ra_browser_move(focus, total, filter, input));
        if (input & (RA_INPUT_SCROLL_UP | RA_INPUT_SCROLL_DOWN)) {
            unsigned long previous = first_row;
            if ((input & RA_INPUT_SCROLL_UP) && first_row) --first_row;
            if ((input & RA_INPUT_SCROLL_DOWN) && first_row < ra_browser_scroll_limit(total)) ++first_row;
            if (previous != first_row) {
                if (focus >= 0) {
                    if ((unsigned long)focus < first_row * RA_GRID_COLUMNS) focus += RA_GRID_COLUMNS;
                    if ((unsigned long)focus >= (first_row + RA_GRID_ROWS) * RA_GRID_COLUMNS) focus -= RA_GRID_COLUMNS;
                    detail_index = (unsigned long)focus;
                }
                render_grid();
            }
        }
        if (activate && focus <= RA_FOCUS_ALL && focus >= RA_FOCUS_UNLOCKED) {
            filter = (unsigned long)(-focus - 1);
            first_row = 0;
            detail_index = 0;
            render_grid();
        }
        load_badges();
        recompui_close_context(browser_context);
        if (activate && focus == RA_FOCUS_CLOSE) close_browser();
        if (activate && focus == RA_FOCUS_RESET) {
            dk64_ra_ui_input(0);
            recompui_open_context(reset_context);
            recompui_set_input_value_u32(reset_choice, 2);
            recompui_close_context(reset_context);
            recompui_show_context(reset_context);
            reset_shown = 1;
        }
        if (activate && focus == RA_FOCUS_ACCOUNT) {
            dk64_ra_ui_input(3); /* clear the opening press; Account now owns input */
            recompui_open_context(account_context);
            render_account();
            recompui_close_context(account_context);
            recompui_show_context(account_context);
            account_shown = 1;
        }
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
