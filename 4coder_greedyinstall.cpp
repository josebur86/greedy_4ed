#include "4coder_default_include.cpp"

#define max(a, b) ((a)>(b)) ? a : b

//
// VIM Start
//

/* TODO(joe): VIM TODO
 *  - p P
 *  - y
 *  - Movement Chord support (d, r, c)
 *  - Registers
 *  - Mouse integration
 *  - search
 *  - visual mode (view_set_highlight)
 *  - visual block mode
 *  - . "dot" support
 *  - Completion
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
 *  - Animated scrolling @Fun
 *  - Hide the mouse cursor unless it moves.
 *  - Shift-j collapse lines
 */

enum CommandMode
{
    NONE = 0,
    G_COMMANDS,
    Y_COMMANDS,
};

enum RegisterType
{
    UNKNOWN = 0,
    PARTIAL_LINE,
    WHOLE_LINE,
    MULTIPLE_LINES,
};

struct Register
{
    RegisterType type;

    char *content;
    uint32_t size;
    uint32_t max_size;
};
static void register_ensure_storage_for_size(Register *r, uint32_t size)
{
    if (r->max_size < size) {
        uint32_t requested_size = max(r->max_size, 1024);
        r->content = (char *)realloc(r->content, requested_size);
        r->max_size = requested_size;
        r->size = 0;
    }
}
#define register_to_string(r) make_string(r.content, r.size)

static bool global_normal_mode = true;
static CommandMode global_command_mode = NONE;
static Register global_yank_register = {
    UNKNOWN, 0, 0, 0
};

// Helpers
static void sync_to_mode(Application_Links *app)
{
    unsigned int insert = 0xFF719E07;
    unsigned int normal = 0xFF839496;

    Theme_Color normal_mode_colors[] = {
        {Stag_Cursor, normal},
        {Stag_Margin_Active, normal},
    };

    Theme_Color insert_mode_colors[] = {
        {Stag_Cursor, insert},
        {Stag_Margin_Active, insert},
    };

    if (global_normal_mode) {
        set_theme_colors(app, normal_mode_colors, ArrayCount(normal_mode_colors));
    } else {
        set_theme_colors(app, insert_mode_colors, ArrayCount(insert_mode_colors));
    }
}

static void enter_g_command_mode()
{
    global_command_mode = G_COMMANDS;
}

static void enter_y_command_mode()
{
    global_command_mode = Y_COMMANDS;
}

static void enter_none_command_mode()
{
    global_command_mode = NONE;
}

// Mode Switching

// TODO(joe): Should these be commands or just functions that are called?
CUSTOM_COMMAND_SIG(switch_to_insert_mode)
{
    global_normal_mode = false;
    sync_to_mode(app);
}

CUSTOM_COMMAND_SIG(switch_to_normal_mode)
{
    global_normal_mode = true;
    sync_to_mode(app);
}

CUSTOM_COMMAND_SIG(handle_g_key)
{
    if (global_command_mode == NONE) {
        enter_g_command_mode();
    } else if (global_command_mode == G_COMMANDS) {
        // Go to the top of the file
        uint32_t access = AccessProtected;
        View_Summary view = get_active_view(app, access);
        Buffer_Seek seek = seek_pos(0);
        view_set_cursor(app, &view, seek, true);

        enter_none_command_mode();
    }
}

CUSTOM_COMMAND_SIG(handle_y_key)
{
    if (global_command_mode == NONE) {
        enter_y_command_mode();
    } else if (global_command_mode == Y_COMMANDS) {
        // Put the current line into the yank register
        uint32_t access = AccessProtected;
        View_Summary view = get_active_view(app, access);
        Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);
        int32_t start = buffer_get_line_start(app, &buffer, view.cursor.line);
        int32_t end = buffer_get_line_end(app, &buffer, view.cursor.line);

        uint32_t size = end-start;
        register_ensure_storage_for_size(&global_yank_register, size);
        bool32 success = buffer_read_range(app, &buffer, start, end, global_yank_register.content);
        global_yank_register.size = size;
        global_yank_register.type = WHOLE_LINE;

        if (success) {
            // TODO(joe): Should I add an string terminator?
        }

        enter_none_command_mode();
    }
}

//
// Editing
//
static void vim_append(Application_Links *app)
{
    exec_command(app, switch_to_insert_mode);
    // TODO(joe): Position the cursor correctly.
}

static void vim_newline_below_then_insert(Application_Links *app)
{
    exec_command(app, seek_end_of_line);
    write_string(app, make_lit_string("\n"));
    exec_command(app, auto_tab_line_at_cursor);
    exec_command(app, switch_to_insert_mode);
}

static void vim_newline_above_then_insert(Application_Links *app)
{
    exec_command(app, seek_beginning_of_line);
    write_string(app, make_lit_string("\n"));
    exec_command(app, move_up);
    exec_command(app, auto_tab_line_at_cursor);
    exec_command(app, switch_to_insert_mode);
}

static void vim_paste_after(Application_Links *app)
{
    if (global_yank_register.size == 0) return;

    // TODO(joe): Cursor placement:
    //  - has_newline == true  -> first character
    //  - has_newline == false -> last character
    if (global_yank_register.type == WHOLE_LINE) {
        exec_command(app, seek_end_of_line);
        write_string(app, make_lit_string("\n"));
    }

    write_string(app, register_to_string(global_yank_register));
}

//
// Moving
//
static void vim_move_up(Application_Links *app)
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

static void vim_move_down(Application_Links *app)
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


static void vim_move_left(Application_Links *app)
{
    if (!at_line_boundary(app, true)) {
        exec_command(app, move_left);
    }
}

static void vim_move_right(Application_Links *app)
{
    if (!at_line_boundary(app, false)) {
        exec_command(app, move_right);
    }
}

static void vim_seek_white_or_token_left(Application_Links *app)
{
    if (!at_line_boundary(app, true)) {
        exec_command(app, seek_white_or_token_left);
    }
}

static void vim_seek_white_or_token_right(Application_Links *app)
{
    if (!at_line_boundary(app, false)) {
        exec_command(app, seek_white_or_token_right);
    }
}

static void vim_seek_to_file_end(Application_Links *app)
{
    uint32_t access = AccessProtected;
    View_Summary view = get_active_view(app, access);
    Buffer_Seek seek = seek_xy(0.0f, max_f32, 1, true);
    view_set_cursor(app, &view, seek, true);
}

static void vim_ex_command(Application_Links *app)
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

static void vim_handle_key_normal(Application_Links *app, Key_Code code, Key_Modifier_Flag modifier)
{
    if (modifier == MDFR_NONE || modifier == MDFR_SHIFT)
    {
        switch(code)
        {
            // TODO(joe): w and e aren't properly emulated with seek_token_right.
            // Might need to use the streaming interface to determine words, characters, etc
            case 'a': vim_append(app); break;
            case 'b': vim_seek_white_or_token_left(app); break;
            case 'e': vim_seek_white_or_token_right(app); break;
            case 'g': handle_g_key(app); break;
            case 'h': vim_move_left(app); break;
            case 'i': switch_to_insert_mode(app); break;
            case 'j': vim_move_down(app); break;
            case 'k': vim_move_up(app); break;
            case 'l': vim_move_right(app); break;
            case 'o': vim_newline_below_then_insert(app); break;
            case 'p': vim_paste_after(app); break;
            case 'u': exec_command(app, undo); break;
            case 'w': vim_seek_white_or_token_right(app); break;
            case 'x': exec_command(app, delete_char); break;
            case 'y': handle_y_key(app); break;

            case 'G': vim_seek_to_file_end(app); break;
            case 'O': vim_newline_above_then_insert(app); break;

            case key_back: vim_move_left(app); break;
            case '$': exec_command(app, seek_end_of_line); break;
            case '^': exec_command(app, seek_beginning_of_line); break;
            case ':': vim_ex_command(app); break;
            case '/': exec_command(app, search); break;
        }
    }
    else if (modifier == MDFR_CTRL)
    {
        switch(code)
        {
            case 'b': exec_command(app, page_up); break;
            case 'f': exec_command(app, page_down); break;
            case 'h': exec_command(app, change_active_panel_backwards); break;
            case 'j': exec_command(app, change_active_panel); break;
            case 'k': exec_command(app, change_active_panel_backwards); break;
            case 'l': exec_command(app, change_active_panel); break;
            case 'r': exec_command(app, redo); break;
            case 'p': exec_command(app, interactive_open_or_new); break;
        }
    }
}

void vim_handle_key_insert(Application_Links *app, Key_Code code, Key_Modifier_Flag modifier)
{
    switch(code)
    {
        case key_back: exec_command(app, backspace_char); break;
        case key_esc: exec_command(app, switch_to_normal_mode); break;
        default: exec_command(app, write_character);
    }
}

CUSTOM_COMMAND_SIG(vim_handle_key)
{
    User_Input input = get_command_input(app);
    if (input.abort) return;

    if (input.type == UserInputKey) {
        Key_Modifier_Flag modifier = MDFR_NONE;
        if (input.key.modifiers[MDFR_SHIFT_INDEX])   { modifier |= MDFR_SHIFT; }
        if (input.key.modifiers[MDFR_CONTROL_INDEX]) { modifier |= MDFR_CTRL; }
        if (input.key.modifiers[MDFR_ALT_INDEX])     { modifier |= MDFR_ALT; }
        if (input.key.modifiers[MDFR_COMMAND_INDEX]) { modifier |= MDFR_CMND; }

        if (global_normal_mode) {
            vim_handle_key_normal(app, input.key.keycode, modifier);
        } else {
            vim_handle_key_insert(app, input.key.keycode, modifier);
        }
    }
}

//
//
//

START_HOOK_SIG(greedy_start)
{
    default_start(app, files, file_count, flags, flag_count);

    //set_fullscreen(app, true);

    Theme_Color colors[] = {
        {Stag_Back, 0xFF002B36},
        {Stag_Bar, 0xFF839496},
        {Stag_Comment, 0xFF586E75},
        {Stag_Keyword, 0xFF519E18},
        {Stag_Preproc, 0xFFCB4B1B},
        {Stag_Include, 0xFFCB4B1B},
        {Stag_Highlight, 0xFFB58900},
        {Stag_Margin_Active, 0xFF719E07},
    };
    set_theme_colors(app, colors, ArrayCount(colors));

    set_global_face_by_name(app, literal("SourceCodePro-Regular"), true);

    global_normal_mode = true;
    sync_to_mode(app);

    return 0;
}

//
//
//

extern "C" GET_BINDING_DATA(get_bindings)
{
    Bind_Helper context_actual = begin_bind_helper(data, size);
    Bind_Helper *context = &context_actual;

    set_start_hook(context, greedy_start);
    set_command_caller(context, default_command_caller);
    set_open_file_hook(context, default_file_settings);
    set_scroll_rule(context, smooth_scroll_rule);
    //set_scroll_rule(context, casey_smooth_scroll_rule);
    set_end_file_hook(context, default_end_file);

    // TODO(joe): Is it possibel to define my own mapid? Why should I?
    begin_map(context, mapid_file);
    {
        bind_vanilla_keys(context, write_character);

        //
        // 4coder specific
        //
        bind(context, key_f4, MDFR_ALT, exit_4coder);
        bind(context, '\n', MDFR_ALT, toggle_fullscreen);

        //
        // VIM
        //
        bind(context, key_esc, MDFR_NONE, vim_handle_key);
        bind(context, key_back, MDFR_NONE, vim_handle_key);

        for (Key_Code code = '!'; code <= '~'; ++code) {
            bind(context, code, MDFR_NONE, vim_handle_key);
            bind(context, code, MDFR_CTRL, vim_handle_key);
        }
    }
    end_map(context);

    end_bind_helper(context);
    return context->write_total;
}
