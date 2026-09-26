#include "browser_input.h"
#include <cstdio>

#define CHECK(test) do { if (!(test)) { std::fprintf(stderr, "Failed line %d: %s\n", __LINE__, #test); return 1; } } while (0)

int main() {
    using dk64_ra::BrowserInput;
    BrowserInput input;
    input.button(RA_INPUT_RIGHT, true);
    CHECK(input.poll(0) == 0); // gameplay input cannot leak into the browser
    input.set_active(true);
    input.button(RA_INPUT_RIGHT, true);
    input.button(RA_INPUT_RIGHT, false); // a tap shorter than a game frame
    CHECK(input.poll(1) == RA_INPUT_RIGHT);
    CHECK(input.poll(1000) == 0);
    input.button(RA_INPUT_DOWN, true);
    CHECK(input.poll(1001) == RA_INPUT_DOWN);
    CHECK(input.poll(1320) == 0);
    CHECK(input.poll(1321) == RA_INPUT_DOWN);
    CHECK(input.poll(1420) == 0);
    CHECK(input.poll(1421) == RA_INPUT_DOWN);
    input.clear(); // focus loss/controller removal cannot leave a held direction
    CHECK(input.poll(2000) == 0);
    input.axis(0, 12000);
    CHECK(input.poll(2001) == 0); // stick deadzone
    input.axis(0, -25000);
    CHECK(input.poll(2002) == RA_INPUT_LEFT);
    input.axis(0, 25000);
    CHECK(input.poll(2003) == RA_INPUT_RIGHT);
    input.axis(0, 0);
    CHECK(input.poll(3000) == 0);
    input.button(RA_INPUT_ACCEPT, true);
    CHECK(input.poll(3001) == RA_INPUT_ACCEPT);
    CHECK(input.poll(4000) == 0); // no repeat on reset/confirm
    input.pulse(RA_INPUT_POINTER | RA_INPUT_CLICK);
    input.set_active(false); // reset modal owns input
    input.set_active(true);
    CHECK(input.poll(5000) == 0);
    input.button(RA_INPUT_DOWN, true, 1);
    CHECK(input.poll(5001) == RA_INPUT_DOWN);
    input.axis(1, 0); // neutral stick events must not release D-pad navigation
    CHECK(input.poll(5321) == RA_INPUT_DOWN);
    input.clear();

    // Grid borders, gaps and the ragged last row must not select phantom badges.
    CHECK(ra_browser_hit(48, 194, 0, 128) == 0);
    CHECK(ra_browser_hit(48 + 58, 194, 0, 128) == RA_FOCUS_NONE);
    CHECK(ra_browser_hit(48 + 15 * 64, 194 + 7 * 64, 0, 128) == 127);
    CHECK(ra_browser_hit(48, 194 + 8 * 64, 0, 128) == RA_FOCUS_NONE);
    CHECK(ra_browser_hit(48, 194 + 8 * 64, 0, 129) == 128);
    CHECK(ra_browser_hit(48 + 64, 194 + 8 * 64, 0, 129) == RA_FOCUS_NONE);
    CHECK(ra_browser_hit(48, 194, 1, 200) == 16);
    CHECK(ra_browser_hit(48, 144, 0, 0) == RA_FOCUS_ALL);
    CHECK(ra_browser_hit(48 + 136 * 2, 144, 0, 0) == RA_FOCUS_UNLOCKED);
    CHECK(ra_browser_hit(49, 951, 0, 0) == RA_FOCUS_RESET);
    CHECK(ra_browser_hit(920, 951, 0, 0) == RA_FOCUS_CLOSE);
    CHECK(ra_browser_move(15, 129, 0, RA_INPUT_RIGHT) == 15);
    CHECK(ra_browser_move(16, 129, 0, RA_INPUT_LEFT) == 16);
    CHECK(ra_browser_move(127, 129, 0, RA_INPUT_DOWN) == 128);
    CHECK(ra_browser_move(128, 129, 0, RA_INPUT_UP) == 112);
    CHECK(ra_browser_move(128, 129, 0, RA_INPUT_DOWN) == RA_FOCUS_RESET);
    CHECK(ra_browser_move(0, 129, 2, RA_INPUT_UP) == RA_FOCUS_UNLOCKED);
    CHECK(ra_browser_move(RA_FOCUS_UNLOCKED, 0, 2, RA_INPUT_DOWN) == RA_FOCUS_RESET);
    CHECK(ra_browser_move(RA_FOCUS_RESET, 0, 2, RA_INPUT_UP) == RA_FOCUS_UNLOCKED);
    CHECK(ra_browser_move(0, 129, 0, RA_INPUT_END) == 128);
    CHECK(ra_browser_move(128, 129, 0, RA_INPUT_HOME) == 0);
    CHECK(ra_browser_move(127, 129, 0, RA_INPUT_TAB) == RA_FOCUS_RESET);
    CHECK(ra_browser_move(RA_FOCUS_RESET, 129, 0, RA_INPUT_TAB) == RA_FOCUS_CLOSE);
    CHECK(ra_browser_move(RA_FOCUS_CLOSE, 129, 0, RA_INPUT_TAB) == RA_FOCUS_ACCOUNT);
    CHECK(ra_browser_move(RA_FOCUS_ACCOUNT, 129, 0, RA_INPUT_TAB) == RA_FOCUS_ALL);
    CHECK(ra_browser_move(RA_FOCUS_UNLOCKED, 129, 2, RA_INPUT_UP) == RA_FOCUS_ACCOUNT);
    CHECK(ra_browser_move(RA_FOCUS_ACCOUNT, 129, 2, RA_INPUT_DOWN) == RA_FOCUS_UNLOCKED);
    CHECK(ra_browser_hit(900, 25, 0, 0) == RA_FOCUS_ACCOUNT);
    CHECK(ra_browser_hit(1072, 25, 0, 0) == RA_FOCUS_NONE);
    CHECK(ra_account_move(0, RA_INPUT_RIGHT) == 1);
    CHECK(ra_account_move(1, RA_INPUT_TAB) == 0);
    CHECK(ra_account_move(1, RA_INPUT_ACCEPT) == 1);
    CHECK(ra_account_hit(RA_ACCOUNT_X + 32, RA_ACCOUNT_Y + 286) == 0);
    CHECK(ra_account_hit(RA_ACCOUNT_X + 201, RA_ACCOUNT_Y + 286) == -1);
    CHECK(ra_account_hit(RA_ACCOUNT_X + 202, RA_ACCOUNT_Y + 286) == 1);
    CHECK(ra_account_hit(RA_ACCOUNT_X + 202, RA_ACCOUNT_Y + 322) == -1);
    CHECK(ra_browser_move(RA_FOCUS_ALL, 129, 0, RA_INPUT_TAB) == 0);
    CHECK(ra_browser_scroll_limit(129) == 0);
    CHECK(ra_browser_scroll_limit(145) == 1);
    CHECK(ra_browser_reveal(0, 144) == 1);
    CHECK(ra_browser_reveal(1, 0) == 0);
    std::puts("Browser: taps, repeat, deadzone, focus loss, modal isolation, grid navigation and hit testing passed");
}
