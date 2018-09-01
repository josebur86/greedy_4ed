#include "4coder_default_include.cpp"

static bool global_normal_mode = true;

/* NOTE(joe): Available command maps
 * mapid_global
 * mapid_file
 * mapid_ui
 * mapid_nomap
 */

/* NOTE(joe): Key Modifier enums
 * MDFR_NONE
 * MDFR_CTRL
 * MDFR_ALT
 * MDFR_CMND
 * MDFR_SHIFT
 */

/* TODO(joe): VIM TODO
 *  - Mode indication (change cursor and top bar color?)
 *  - o O
 *  - y yy
 *  - p P
 *  - search
 *  - Movement Chord support (d, r, c)
 *  - Registers
 *  - visual mode
 *  - visual block mode
 *  - . "dot" support
 *  - Panel management (Ctrl-W Ctrl-V), (Ctrl-W Ctrl-H)
 *  - File commands (gf)
 *  - Brace matching %
 *  - Find/Replace
 *  - Find in Files
 *  - ctag support? does 4coder have something better?
 *  - ctrl-a addition, ctrl-x subtraction
 *  - highlight word under cursor
 *  - find corresponding file (h <-> cpp)
 *  - find corresponding file and display in other panel (h <-> cpp)
 *  - Macro support
 *  - show line numbers? (Not sure if 4coder supports this yet)
 */

START_HOOK_SIG(greedy_start)
{
    default_start(app, files, file_count, flags, flag_count);

    //set_fullscreen(app, true);

    Theme_Color colors[] = {
        {Stag_Back, 0xFF002B36},
        {Stag_Comment, 0xFF586E75},
        {Stag_Keyword, 0xFF519E18},
        {Stag_Preproc, 0xFFCB4B1B},
        {Stag_Include, 0xFFCB4B1B},
        {Stag_Highlight, 0xFFB58900},
        {Stag_Cursor, 0xFF839496},
    };
    set_theme_colors(app, colors, ArrayCount(colors));

    set_global_face_by_name(app, literal("SourceCodePro-Regular"), true);

    global_normal_mode = true;

    return 0;
}

//
// Mode Switching
//

CUSTOM_COMMAND_SIG(switch_to_insert_mode)
{
    global_normal_mode = false;
}

CUSTOM_COMMAND_SIG(switch_to_normal_mode)
{
    global_normal_mode = true;
}

//
// Editing
//
CUSTOM_COMMAND_SIG(vim_append)
{
    exec_command(app, switch_to_insert_mode);
    // TODO(joe): Position the cursor correctly.
}

//
// Moving
//
CUSTOM_COMMAND_SIG(vim_move_up)
{
    uint32_t access = AccessProtected;
    View_Summary view = get_active_view(app, access);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);

    int line = buffer_get_line_number(app, &buffer, view.cursor.pos);
    if (line > 1)
    {
        exec_command(app, move_up);
    }
}

CUSTOM_COMMAND_SIG(vim_move_down)
{
    uint32_t access = AccessProtected;
    View_Summary view = get_active_view(app, access);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);

    int line = buffer_get_line_number(app, &buffer, view.cursor.pos);
    if (line < buffer.line_count)
    {
        exec_command(app, move_down);
    }
}

static bool at_line_boundary(Application_Links *app, bool moving_left)
{
    uint32_t access = AccessProtected;
    View_Summary view = get_active_view(app, access);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);

    int boundary_pos = (moving_left) ? buffer_get_line_start(app, &buffer, view.cursor.line)
                                     : buffer_get_line_end(app, &buffer, view.cursor.line);
    return (view.cursor.pos == boundary_pos);
}


CUSTOM_COMMAND_SIG(vim_move_left)
{
    if (!at_line_boundary(app, true)) {
        exec_command(app, move_left);
    }
}

CUSTOM_COMMAND_SIG(vim_move_right)
{
    if (!at_line_boundary(app, false)) {
        exec_command(app, move_right);
    }
}

CUSTOM_COMMAND_SIG(vim_seek_white_or_token_left)
{
    if (!at_line_boundary(app, true)) {
        exec_command(app, seek_white_or_token_left);
    }
}

CUSTOM_COMMAND_SIG(vim_seek_white_or_token_right)
{
    if (!at_line_boundary(app, false)) {
        exec_command(app, seek_white_or_token_right);
    }
}

CUSTOM_COMMAND_SIG(vim_ex_command)
{
    char command[1024];

    Query_Bar bar;
    bar.prompt = make_lit_string(":");
    bar.string = make_fixed_width_string(command);

    if (query_user_string(app, &bar)) {
        if (match(bar.string, make_lit_string("w"))) {
            exec_command(app, save);
        } else if (match(bar.string, make_lit_string("wa"))) {
            exec_command(app, save_all_dirty_buffers);
        } else if (match(bar.string, make_lit_string("q"))) {
            exec_command(app, kill_buffer);
            exec_command(app, close_panel);
        }
    }

    end_query_bar(app, &bar, 0);
}

//
// Bindings
//

void vim_handle_key_normal(Application_Links *app, Key_Code code, Key_Modifier_Flag modifier)
{
    if (modifier == MDFR_NONE)
    {
        switch(code)
        {
            // TODO(joe): w and e aren't properly emulated with seek_token_right.
            case 'a': exec_command(app, vim_append); break;
            case 'b': exec_command(app, vim_seek_white_or_token_left); break;
            case 'e': exec_command(app, vim_seek_white_or_token_right); break;
            case 'h': exec_command(app, vim_move_left); break;
            case 'i': exec_command(app, switch_to_insert_mode); break;
            case 'j': exec_command(app, vim_move_down); break;
            case 'k': exec_command(app, vim_move_up); break;
            case 'l': exec_command(app, vim_move_right); break;
            case 'u': exec_command(app, undo); break;
            case 'w': exec_command(app, vim_seek_white_or_token_right); break;
            case 'x': exec_command(app, delete_char); break;

            case key_back: exec_command(app, vim_move_left); break;
            case '$': exec_command(app, seek_end_of_line); break;
            case '^': exec_command(app, seek_beginning_of_line); break;
            case ':': exec_command(app, vim_ex_command); break;
        }
    } else if (modifier == MDFR_CTRL) {
        switch(code)
        {
            case 'b': exec_command(app, page_up); break;
            case 'h': exec_command(app, change_active_panel_backwards); break;
            case 'j': exec_command(app, change_active_panel); break;
            case 'k': exec_command(app, change_active_panel_backwards); break;
            case 'l': exec_command(app, change_active_panel); break;
            case 'f': exec_command(app, page_down); break;
            case 'r': exec_command(app, redo); break;
        }
    }
}

void vim_handle_key_insert(Application_Links *app, Key_Code code, Key_Modifier_Flag modifier)
{
    switch(code)
    {
        case key_back: exec_command(app, backspace_char); break;
        default: exec_command(app, write_character);
    }
}

void vim_handle_key(Application_Links *app, Key_Code code, Key_Modifier_Flag modifier)
{
    if (global_normal_mode) {
        vim_handle_key_normal(app, code, modifier);
    } else {
        vim_handle_key_insert(app, code, modifier);
    }
}

CUSTOM_COMMAND_SIG(vim_a) { vim_handle_key(app, 'a', MDFR_NONE); }
CUSTOM_COMMAND_SIG(vim_b) { vim_handle_key(app, 'b', MDFR_NONE); }
CUSTOM_COMMAND_SIG(vim_c) { vim_handle_key(app, 'c', MDFR_NONE); }
CUSTOM_COMMAND_SIG(vim_d) { vim_handle_key(app, 'd', MDFR_NONE); }
CUSTOM_COMMAND_SIG(vim_e) { vim_handle_key(app, 'e', MDFR_NONE); }
CUSTOM_COMMAND_SIG(vim_f) { vim_handle_key(app, 'f', MDFR_NONE); }
CUSTOM_COMMAND_SIG(vim_g) { vim_handle_key(app, 'g', MDFR_NONE); }
CUSTOM_COMMAND_SIG(vim_h) { vim_handle_key(app, 'h', MDFR_NONE); }
CUSTOM_COMMAND_SIG(vim_i) { vim_handle_key(app, 'i', MDFR_NONE); }
CUSTOM_COMMAND_SIG(vim_j) { vim_handle_key(app, 'j', MDFR_NONE); }
CUSTOM_COMMAND_SIG(vim_k) { vim_handle_key(app, 'k', MDFR_NONE); }
CUSTOM_COMMAND_SIG(vim_l) { vim_handle_key(app, 'l', MDFR_NONE); }
CUSTOM_COMMAND_SIG(vim_m) { vim_handle_key(app, 'm', MDFR_NONE); }
CUSTOM_COMMAND_SIG(vim_n) { vim_handle_key(app, 'n', MDFR_NONE); }
CUSTOM_COMMAND_SIG(vim_o) { vim_handle_key(app, 'o', MDFR_NONE); }
CUSTOM_COMMAND_SIG(vim_p) { vim_handle_key(app, 'p', MDFR_NONE); }
CUSTOM_COMMAND_SIG(vim_q) { vim_handle_key(app, 'q', MDFR_NONE); }
CUSTOM_COMMAND_SIG(vim_r) { vim_handle_key(app, 'r', MDFR_NONE); }
CUSTOM_COMMAND_SIG(vim_s) { vim_handle_key(app, 's', MDFR_NONE); }
CUSTOM_COMMAND_SIG(vim_t) { vim_handle_key(app, 't', MDFR_NONE); }
CUSTOM_COMMAND_SIG(vim_u) { vim_handle_key(app, 'u', MDFR_NONE); }
CUSTOM_COMMAND_SIG(vim_v) { vim_handle_key(app, 'v', MDFR_NONE); }
CUSTOM_COMMAND_SIG(vim_w) { vim_handle_key(app, 'w', MDFR_NONE); }
CUSTOM_COMMAND_SIG(vim_x) { vim_handle_key(app, 'x', MDFR_NONE); }
CUSTOM_COMMAND_SIG(vim_y) { vim_handle_key(app, 'y', MDFR_NONE); }
CUSTOM_COMMAND_SIG(vim_z) { vim_handle_key(app, 'z', MDFR_NONE); }

CUSTOM_COMMAND_SIG(vim_backspace) { vim_handle_key(app, key_back, MDFR_NONE); }
CUSTOM_COMMAND_SIG(vim_dollar) { vim_handle_key(app, '$', MDFR_NONE); }
CUSTOM_COMMAND_SIG(vim_hat) { vim_handle_key(app, '^', MDFR_NONE); }

CUSTOM_COMMAND_SIG(vim_b_ctrl) { vim_handle_key(app, 'b', MDFR_CTRL); }
CUSTOM_COMMAND_SIG(vim_h_ctrl) { vim_handle_key(app, 'h', MDFR_CTRL); }
CUSTOM_COMMAND_SIG(vim_j_ctrl) { vim_handle_key(app, 'j', MDFR_CTRL); }
CUSTOM_COMMAND_SIG(vim_k_ctrl) { vim_handle_key(app, 'k', MDFR_CTRL); }
CUSTOM_COMMAND_SIG(vim_l_ctrl) { vim_handle_key(app, 'l', MDFR_CTRL); }
CUSTOM_COMMAND_SIG(vim_f_ctrl) { vim_handle_key(app, 'f', MDFR_CTRL); }
CUSTOM_COMMAND_SIG(vim_r_ctrl) { vim_handle_key(app, 'r', MDFR_CTRL); }

CUSTOM_COMMAND_SIG(vim_colon) { vim_handle_key(app, ':', MDFR_NONE); }

extern "C" GET_BINDING_DATA(get_bindings)
{
    Bind_Helper context_actual = begin_bind_helper(data, size);
    Bind_Helper *context = &context_actual;

    set_start_hook(context, greedy_start);
    set_command_caller(context, default_command_caller);
    set_open_file_hook(context, default_file_settings);
    //set_scroll_rule(context, casey_smooth_scroll_rule);
    set_end_file_hook(context, default_end_file);

    begin_map(context, mapid_global);
    {
        // TODO(joe): Reroute this thru the vim stuff to revert to normal mode?
        bind(context, 'p', MDFR_CTRL, interactive_open_or_new);
    }
    end_map(context);

    begin_map(context, mapid_file);
    {
        bind_vanilla_keys(context, write_character);

        // Mode switching
        bind(context, key_esc, MDFR_NONE, switch_to_normal_mode);
        // TODO(joe): Incremental search
        // Commands
        bind(context, ':', MDFR_NONE, vim_colon);

        // Character Keys
        bind(context, 'a', MDFR_NONE, vim_a);
        bind(context, 'b', MDFR_NONE, vim_b);
        bind(context, 'c', MDFR_NONE, vim_c);
        bind(context, 'd', MDFR_NONE, vim_d);
        bind(context, 'e', MDFR_NONE, vim_e);
        bind(context, 'f', MDFR_NONE, vim_f);
        bind(context, 'g', MDFR_NONE, vim_g);
        bind(context, 'h', MDFR_NONE, vim_h);
        bind(context, 'i', MDFR_NONE, vim_i);
        bind(context, 'j', MDFR_NONE, vim_j);
        bind(context, 'k', MDFR_NONE, vim_k);
        bind(context, 'l', MDFR_NONE, vim_l);
        bind(context, 'm', MDFR_NONE, vim_m);
        bind(context, 'n', MDFR_NONE, vim_n);
        bind(context, 'o', MDFR_NONE, vim_o);
        bind(context, 'p', MDFR_NONE, vim_p);
        bind(context, 'q', MDFR_NONE, vim_q);
        bind(context, 'r', MDFR_NONE, vim_r);
        bind(context, 's', MDFR_NONE, vim_s);
        bind(context, 't', MDFR_NONE, vim_t);
        bind(context, 'u', MDFR_NONE, vim_u);
        bind(context, 'v', MDFR_NONE, vim_v);
        bind(context, 'w', MDFR_NONE, vim_w);
        bind(context, 'x', MDFR_NONE, vim_x);
        bind(context, 'y', MDFR_NONE, vim_y);
        bind(context, 'z', MDFR_NONE, vim_z);

        bind(context, key_back, MDFR_NONE, vim_backspace);
        bind(context, '$', MDFR_NONE, vim_dollar);
        bind(context, '^', MDFR_NONE, vim_hat);

        bind(context, 'b', MDFR_CTRL, vim_b_ctrl);
        bind(context, 'h', MDFR_CTRL, vim_h_ctrl);
        bind(context, 'j', MDFR_CTRL, vim_j_ctrl);
        bind(context, 'k', MDFR_CTRL, vim_k_ctrl);
        bind(context, 'l', MDFR_CTRL, vim_l_ctrl);
        bind(context, 'f', MDFR_CTRL, vim_f_ctrl);
        bind(context, 'r', MDFR_CTRL, vim_r_ctrl);

        bind(context, key_f4, MDFR_ALT, exit_4coder);
    }
    end_map(context);

    end_bind_helper(context);
    return context->write_total;
}
