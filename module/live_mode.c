#include "live_mode.h"

#include <string.h>

// this
#include "flash.h"
#include "gitversion.h"
#include "globals.h"
#include "keyboard_helper.h"
#include "line_editor.h"

// teletype
#include "teletype_io.h"

// libavr32
#include "font.h"
#include "region.h"
#include "util.h"

// asf
#include "conf_usb_host.h"  // needed in order to include "usb_protocol_hid.h"
#include "usb_protocol_hid.h"

#define MAX_HISTORY_SIZE 16
static tele_command_t history[MAX_HISTORY_SIZE];  // newest entry in index 0
static int8_t history_line;                       // -1 for not selected
static int8_t history_top;                        // -1 when empty

static line_editor_t le;
static process_result_t output;
static error_t status;
static char error_msg[TELE_ERROR_MSG_LENGTH];
static bool show_welcome_message;

static const uint8_t D_INPUT = 1 << 0;
static const uint8_t D_LIST = 1 << 1;
static const uint8_t D_MESSAGE = 1 << 2;
static const uint8_t D_VARS = 1 << 3;
static const uint8_t D_ALL = 0xFF;
static uint8_t dirty;

static const uint8_t A_METRO = 1 << 0;
static const uint8_t A_SLEW = 1 << 1;
static const uint8_t A_DELAY = 1 << 2;
static const uint8_t A_STACK = 1 << 3;
static const uint8_t A_MUTES = 1 << 4;
static uint8_t activity_prev;
static uint8_t activity;
static bool show_vars = false;
static int16_t vars_prev[8];
char var_names[] = { 'A', 0, 'X', 0, 'B', 0, 'Y', 0,
                     'C', 0, 'Z', 0, 'D', 0, 'T', 0 };

// teletype_io.h
void tele_has_delays(bool has_delays) {
    if (has_delays)
        activity |= A_DELAY;
    else
        activity &= ~A_DELAY;
}

void tele_has_stack(bool has_stack) {
    if (has_stack)
        activity |= A_STACK;
    else
        activity &= ~A_STACK;
}

void tele_mute() {
    activity |= A_MUTES;
}

// set icons
void set_slew_icon(bool display) {
    if (display)
        activity |= A_SLEW;
    else
        activity &= ~A_SLEW;
}

void set_metro_icon(bool display) {
    if (display)
        activity |= A_METRO;
    else
        activity &= ~A_METRO;
}

void set_vars_updated() {
    dirty |= D_VARS;
}

// main mode functions
void init_live_mode() {
    status = E_OK;
    show_welcome_message = true;
    dirty = D_ALL;
    activity_prev = 0xFF;
    history_top = -1;
    history_line = -1;
    show_vars = false;
}

void set_live_mode() {
    line_editor_set(&le, "");
    history_line = -1;
    dirty = D_ALL;
    activity_prev = 0xFF;
}

void process_live_keys(uint8_t k, uint8_t m, bool is_held_key) {
    // <down> or C-n: history next
    if (match_no_mod(m, k, HID_DOWN) || match_ctrl(m, k, HID_N)) {
        if (history_line > 0) {
            history_line--;
            line_editor_set_command(&le, &history[history_line]);
        }
        else {
            history_line = -1;
            line_editor_set(&le, "");
        }
        dirty |= D_INPUT;
    }
    // <up> or C-p: history previous
    else if (match_no_mod(m, k, HID_UP) || match_ctrl(m, k, HID_P)) {
        if (history_line < history_top) {
            history_line++;
            line_editor_set_command(&le, &history[history_line]);
            dirty |= D_INPUT;
        }
    }
    // <enter>: execute command
    else if (match_no_mod(m, k, HID_ENTER)) {
        dirty |= D_MESSAGE;  // something will definitely happen
        dirty |= D_INPUT;

        tele_command_t command;

        status = parse(line_editor_get(&le), &command, error_msg);
        if (status != E_OK)
            return;  // quit, screen_refresh_live will display the error message

        status = validate(&command, error_msg);
        if (status != E_OK)
            return;  // quit, screen_refresh_live will display the error message

        if (command.length) {
            // increase history_size up to a maximum
            history_top++;
            if (history_top >= MAX_HISTORY_SIZE)
                history_top = MAX_HISTORY_SIZE - 1;

            // shuffle the history up
            // should really use some sort of ring buffer
            for (size_t i = history_top; i > 0; i--) {
                memcpy(&history[i], &history[i - 1], sizeof(command));
            }
            memcpy(&history[0], &command, sizeof(command));

            ss_clear_script(&scene_state, TEMP_SCRIPT);
            ss_overwrite_script_command(&scene_state, TEMP_SCRIPT, 0, &command);
            exec_state_t es;
            es_init(&es);
            es_push(&es);
            es_variables(&es)->script_number = TEMP_SCRIPT;

            output = run_script_with_exec_state(&scene_state, &es, TEMP_SCRIPT);
        }

        history_line = -1;
        line_editor_set(&le, "");
    }
    // [ or ]: switch to edit mode
    else if (match_no_mod(m, k, HID_OPEN_BRACKET) ||
             match_no_mod(m, k, HID_CLOSE_BRACKET)) {
        set_mode(M_EDIT);
    }
    // tilde: show the variables
    else if (match_no_mod(m, k, HID_TILDE)) {
        show_vars = !show_vars;
        if (show_vars) dirty |= D_VARS;  // combined with this...
        dirty |= D_LIST;  // cheap flag to indicate mode just switched
    }
    else {  // pass the key though to the line editor
        bool processed = line_editor_process_keys(&le, k, m, is_held_key);
        if (processed) dirty |= D_INPUT;
    }
}


uint8_t screen_refresh_live() {
    uint8_t screen_dirty = 0;

    if (dirty & D_INPUT) {
        line_editor_draw(&le, '>', &line[7]);
        screen_dirty |= (1 << 7);
        dirty &= ~D_INPUT;
    }

    if (dirty & D_MESSAGE) {
        char s[36];
        if (status != E_OK) {
            strcpy(s, tele_error(status));
            if (error_msg[0]) {
                size_t len = strlen(s);
                strcat(s, ": ");
                strncat(s, error_msg, 32 - len - 3);
                error_msg[0] = 0;
            }
            status = E_OK;
        }
        else if (output.has_value) {
            itoa(output.value, s, 10);
            output.has_value = false;
        }
        else if (show_welcome_message) {
            strcpy(s, "TELETYPE: ");
            strncat(s, git_version, 35 - strlen(s));
            show_welcome_message = false;
        }
        else {
            s[0] = 0;
        }

        region_fill(&line[6], 0);
        font_string_region_clip(&line[6], s, 0, 0, 0x4, 0);

        screen_dirty |= (1 << 6);
        dirty &= ~D_MESSAGE;
    }

    if (show_vars && ((dirty & D_VARS) || (dirty & D_LIST))) {
        int16_t* vp =
            &scene_state.variables
                 .a;  // 8 int16_t all in a row, point at the first one
                      // relies on variable ordering. see: src/state.h
        bool changed = dirty & D_LIST;
        char s[8];

        if (changed) {
            region_fill(&line[1], 0);
            screen_dirty |= (1 << 1);
        }

        for (size_t i = 0; i < 8; i += 2)
            if (changed || (vp[i] != vars_prev[i]) ||
                (vp[i + 1] != vars_prev[i + 1])) {
                region_fill(&line[i / 2 + 2], 0);
                vars_prev[i] = vp[i];
                vars_prev[i + 1] = vp[i + 1];
                itoa(vp[i], s, 10);
                font_string_region_clip_right(&line[i / 2 + 2], s, 11 * 4, 0,
                                              0xf, 0);
                font_string_region_clip_right(
                    &line[i / 2 + 2], var_names + (i * 2), 14 * 4, 0, 0x1, 0);
                itoa(vp[i + 1], s, 10);
                font_string_region_clip_right(&line[i / 2 + 2], s, 25 * 4, 0,
                                              0xf, 0);
                font_string_region_clip_right(&line[i / 2 + 2],
                                              var_names + ((i + 1) * 2), 28 * 4,
                                              0, 0x1, 0);
                screen_dirty |= (1 << (i / 2 + 2));
                for (int row = 1; row < 9; row += 2) {
                    line[i / 2 + 2].data[row * 128 + 12 * 4 - 1] = 0x1;
                    line[i / 2 + 2].data[row * 128 + 26 * 4 - 1] = 0x1;
                }
            }
        dirty &= ~D_VARS;
        dirty &= ~D_LIST;
    }

    if (dirty & D_LIST) {
        for (int i = 1; i < 6; i++) region_fill(&line[i], 0);

        screen_dirty |= 0x3E;
        dirty &= ~D_LIST;
    }

    if ((activity != activity_prev)) {
        region_fill(&line[0], 0);

        // slew icon
        uint8_t slew_fg = activity & A_SLEW ? 15 : 1;
        line[0].data[98 + 0 + 512] = slew_fg;
        line[0].data[98 + 1 + 384] = slew_fg;
        line[0].data[98 + 2 + 256] = slew_fg;
        line[0].data[98 + 3 + 128] = slew_fg;
        line[0].data[98 + 4 + 0] = slew_fg;

        // delay icon
        uint8_t delay_fg = activity & A_DELAY ? 15 : 1;
        line[0].data[106 + 0 + 0] = delay_fg;
        line[0].data[106 + 1 + 0] = delay_fg;
        line[0].data[106 + 2 + 0] = delay_fg;
        line[0].data[106 + 3 + 0] = delay_fg;
        line[0].data[106 + 4 + 0] = delay_fg;
        line[0].data[106 + 0 + 128] = delay_fg;
        line[0].data[106 + 0 + 256] = delay_fg;
        line[0].data[106 + 0 + 384] = delay_fg;
        line[0].data[106 + 0 + 512] = delay_fg;
        line[0].data[106 + 4 + 128] = delay_fg;
        line[0].data[106 + 4 + 256] = delay_fg;
        line[0].data[106 + 4 + 384] = delay_fg;
        line[0].data[106 + 4 + 512] = delay_fg;

        // queue icon
        uint8_t stack_fg = activity & A_STACK ? 15 : 1;
        line[0].data[114 + 0 + 0] = stack_fg;
        line[0].data[114 + 1 + 0] = stack_fg;
        line[0].data[114 + 2 + 0] = stack_fg;
        line[0].data[114 + 3 + 0] = stack_fg;
        line[0].data[114 + 4 + 0] = stack_fg;
        line[0].data[114 + 0 + 256] = stack_fg;
        line[0].data[114 + 1 + 256] = stack_fg;
        line[0].data[114 + 2 + 256] = stack_fg;
        line[0].data[114 + 3 + 256] = stack_fg;
        line[0].data[114 + 4 + 256] = stack_fg;
        line[0].data[114 + 0 + 512] = stack_fg;
        line[0].data[114 + 1 + 512] = stack_fg;
        line[0].data[114 + 2 + 512] = stack_fg;
        line[0].data[114 + 3 + 512] = stack_fg;
        line[0].data[114 + 4 + 512] = stack_fg;

        // metro icon
        uint8_t metro_fg = activity & A_METRO ? 15 : 1;
        line[0].data[122 + 0 + 0] = metro_fg;
        line[0].data[122 + 0 + 128] = metro_fg;
        line[0].data[122 + 0 + 256] = metro_fg;
        line[0].data[122 + 0 + 384] = metro_fg;
        line[0].data[122 + 0 + 512] = metro_fg;
        line[0].data[122 + 1 + 128] = metro_fg;
        line[0].data[122 + 2 + 256] = metro_fg;
        line[0].data[122 + 3 + 128] = metro_fg;
        line[0].data[122 + 4 + 0] = metro_fg;
        line[0].data[122 + 4 + 128] = metro_fg;
        line[0].data[122 + 4 + 256] = metro_fg;
        line[0].data[122 + 4 + 384] = metro_fg;
        line[0].data[122 + 4 + 512] = metro_fg;

        // mutes
        for (size_t i = 0; i < 8; i++) {
            // make it staggered to match how the device looks
            size_t stagger = i % 2 ? 384 : 128;
            uint8_t mute_fg = ss_get_mute(&scene_state, i) ? 15 : 1;
            line[0].data[87 + i + stagger] = mute_fg;
        }

        activity_prev = activity;
        screen_dirty |= 0x1;
        activity &= ~A_MUTES;
    }

    return screen_dirty;
}
