#ifndef DK64_RA_BROWSER_H
#define DK64_RA_BROWSER_H

/* Shared DP geometry: recompui always renders a 1080 DP tall viewport. */
enum {
    RA_BROWSER_WIDTH = 1120, RA_BROWSER_HEIGHT = 1000, RA_BROWSER_TOP = 40,
    RA_GRID_X = 48, RA_GRID_Y = 194, RA_GRID_PITCH = 64, RA_BADGE_SIZE = 58,
    RA_GRID_COLUMNS = 16, RA_GRID_ROWS = 9,
    RA_GRID_SLOTS = RA_GRID_COLUMNS * RA_GRID_ROWS,
    RA_INPUT_UP = 1, RA_INPUT_DOWN = 2, RA_INPUT_LEFT = 4, RA_INPUT_RIGHT = 8,
    RA_INPUT_ACCEPT = 16, RA_INPUT_TAB = 32, RA_INPUT_HOME = 64, RA_INPUT_END = 128,
    RA_INPUT_POINTER = 256, RA_INPUT_CLICK = 512,
    RA_INPUT_SCROLL_UP = 1024, RA_INPUT_SCROLL_DOWN = 2048,
    RA_FOCUS_ALL = -1, RA_FOCUS_LOCKED = -2, RA_FOCUS_UNLOCKED = -3,
    RA_FOCUS_RESET = -4, RA_FOCUS_CLOSE = -5, RA_FOCUS_NONE = -6,
    RA_FOCUS_ACCOUNT = -7
};
enum { RA_ACCOUNT_WIDTH = 760, RA_ACCOUNT_HEIGHT = 356, RA_ACCOUNT_BUTTON_Y = 286,
       RA_ACCOUNT_X = (RA_BROWSER_WIDTH - RA_ACCOUNT_WIDTH) / 2,
       RA_ACCOUNT_Y = (RA_BROWSER_HEIGHT - RA_ACCOUNT_HEIGHT) / 2 };

static inline int ra_account_move(int focus, unsigned long input) {
    if (input & (RA_INPUT_TAB | RA_INPUT_LEFT | RA_INPUT_RIGHT | RA_INPUT_UP | RA_INPUT_DOWN)) return !focus;
    return focus;
}
static inline int ra_account_hit(int x, int y) {
    x -= RA_ACCOUNT_X; y -= RA_ACCOUNT_Y;
    if (y >= RA_ACCOUNT_BUTTON_Y && y < RA_ACCOUNT_BUTTON_Y + 36) {
        if (x >= 32 && x < 182) return 0;
        if (x >= 202 && x < 382) return 1;
    }
    return -1;
}

static inline unsigned long ra_browser_last_row(unsigned long total) {
    return total ? (total - 1) / RA_GRID_COLUMNS : 0;
}

static inline unsigned long ra_browser_scroll_limit(unsigned long total) {
    unsigned long rows = ra_browser_last_row(total) + 1;
    return rows > RA_GRID_ROWS ? rows - RA_GRID_ROWS : 0;
}

static inline unsigned long ra_browser_reveal(unsigned long first, unsigned long selected) {
    unsigned long row = selected / RA_GRID_COLUMNS;
    if (row < first) return row;
    if (row >= first + RA_GRID_ROWS) return row - RA_GRID_ROWS + 1;
    return first;
}

static inline long ra_browser_move(long focus, unsigned long total,
                                   unsigned long filter, unsigned long input) {
    if (input & RA_INPUT_TAB) {
        if (focus >= 0) return RA_FOCUS_RESET;
        if (focus == RA_FOCUS_RESET) return RA_FOCUS_CLOSE;
        if (focus == RA_FOCUS_CLOSE) return RA_FOCUS_ACCOUNT;
        if (focus == RA_FOCUS_ACCOUNT) return -(long)filter - 1;
        return total ? 0 : RA_FOCUS_RESET;
    }
    if (input & RA_INPUT_HOME) return total ? 0 : RA_FOCUS_ALL;
    if (input & RA_INPUT_END) return total ? (long)total - 1 : RA_FOCUS_ALL;
    if (focus >= 0) {
        if (!total) return RA_FOCUS_ALL;
        if (input & RA_INPUT_UP) return focus < RA_GRID_COLUMNS ?
            -(long)filter - 1 : focus - RA_GRID_COLUMNS;
        if (input & RA_INPUT_DOWN) {
            if ((unsigned long)focus / RA_GRID_COLUMNS == ra_browser_last_row(total))
                return RA_FOCUS_RESET;
            return (unsigned long)focus + RA_GRID_COLUMNS < total ?
                focus + RA_GRID_COLUMNS : (long)total - 1;
        }
        if ((input & RA_INPUT_LEFT) && focus % RA_GRID_COLUMNS) return focus - 1;
        if ((input & RA_INPUT_RIGHT) && focus % RA_GRID_COLUMNS != RA_GRID_COLUMNS - 1 &&
            (unsigned long)focus + 1 < total) return focus + 1;
    } else if (focus == RA_FOCUS_ACCOUNT) {
        if (input & RA_INPUT_UP) return RA_FOCUS_CLOSE;
        if (input & (RA_INPUT_DOWN | RA_INPUT_LEFT | RA_INPUT_RIGHT)) return -(long)filter - 1;
    } else if (focus >= RA_FOCUS_UNLOCKED) {
        if ((input & RA_INPUT_LEFT) && focus < RA_FOCUS_ALL) return focus + 1;
        if ((input & RA_INPUT_RIGHT) && focus > RA_FOCUS_UNLOCKED) return focus - 1;
        if (input & RA_INPUT_DOWN) return total ? 0 : RA_FOCUS_RESET;
        if (input & RA_INPUT_UP) return RA_FOCUS_ACCOUNT;
    } else {
        if (input & RA_INPUT_UP) return total ? (long)total - 1 : -(long)filter - 1;
        if (input & RA_INPUT_DOWN) return -(long)filter - 1;
        if (input & (RA_INPUT_LEFT | RA_INPUT_RIGHT))
            return focus == RA_FOCUS_RESET ? RA_FOCUS_CLOSE : RA_FOCUS_RESET;
    }
    return focus;
}

static inline long ra_browser_hit(int x, int y, unsigned long first, unsigned long total) {
    if (x >= 892 && x < 1072 && y >= 22 && y < 58) return RA_FOCUS_ACCOUNT;
    if (y >= 144 && y < 180 && x >= 48 && x < 48 + 3 * 136) {
        int offset = (x - 48) % 136;
        return offset < 128 ? -(long)((x - 48) / 136) - 1 : RA_FOCUS_NONE;
    }
    if (y >= 950 && y < 988) {
        if (x >= 48 && x < 294) return RA_FOCUS_RESET;
        if (x >= 918 && x < 1072) return RA_FOCUS_CLOSE;
    }
    x -= RA_GRID_X;
    y -= RA_GRID_Y;
    if (x >= 0 && x < RA_GRID_COLUMNS * RA_GRID_PITCH &&
        y >= 0 && y < RA_GRID_ROWS * RA_GRID_PITCH &&
        x % RA_GRID_PITCH < RA_BADGE_SIZE && y % RA_GRID_PITCH < RA_BADGE_SIZE) {
        unsigned long index = (first + y / RA_GRID_PITCH) * RA_GRID_COLUMNS + x / RA_GRID_PITCH;
        if (index < total) return (long)index;
    }
    return RA_FOCUS_NONE;
}
#endif
