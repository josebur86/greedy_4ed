#include "4coder_default_include.cpp"

#include <assert.h>

#define max(a, b) ((a)>(b)) ? a : b

//
// VIM Start
//

/* TODO(joe): VIM TODO
 *  -----In Progress------
 *  - Movement Chord support (v, d, r, c)
 *  -----Must have------
 *  - Shift-j collapse lines
 *  - visual block mode
 *  - search
 *  - . "dot" support
 *  - Find/Replace
 *  - Find in Files
 *  - find corresponding file (h <-> cpp)
 *  - find corresponding file and display in other panel (h <-> cpp)
 *  - indenting (=) (this is low priority because 4ed does so much to help already)
 *  - Mouse integration - select things with the mouse
 *  -----Nice to have------
 *  - Brace matching %
 *  - File commands (gf)
 *  - Completion
 *  - Panel management (Ctrl-W Ctrl-V), (Ctrl-W Ctrl-H)
 *  - ctag support? does 4coder have something better?
 *  - ctrl-a addition, ctrl-x subtraction
 *  - f/t - search on this line.
 *  - Go to line number
 *  - Macro support
 *  - Hide the mouse cursor unless it moves.
 *  - zz to move current line to the center of the view.
 *  -----Not Supported------
 *  - show line numbers? (Not sure if 4coder supports this yet)
 *  - highlight word under cursor (this ones won't work because the highlight and cursor are
 *    mutually exclusive in 4ed.)
 */

enum VimMode
{
    NORMAL = 0,
    INSERT,
    VISUAL,
    VISUAL_BLOCK,
};

enum VimKeyMaps
{
    mapid_normal = (1 << 16),
    mapid_insert,
    mapid_visual,
    mapid_visual_block,

    mapid_movement,
    mapid_delete,
};

enum CommandMode
{
    NONE = 0,
    DELETE,
    G_COMMANDS,
    Y_COMMANDS,
    Z_COMMANDS,
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

struct VimHighlight
{
    Full_Cursor cursor;
    int start;
    int end;
};
static void highlight_seek(Application_Links *app, VimHighlight *highlight, Buffer_Seek seek);

static VimMode global_mode = NORMAL;
static CommandMode global_command_mode = NONE;
static Register global_yank_register = {
    UNKNOWN, 0, 0, 0
};
static VimHighlight global_highlight = {0};

//
// Mode Helpers
//
static void set_current_map(Application_Links *app, int buffer_id, int mapid)
{
    unsigned int access = AccessAll;
    Buffer_Summary buffer = get_buffer(app, buffer_id, access);
    buffer_set_setting(app, &buffer, BufferSetting_MapID, mapid);
}

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

    if (global_mode == NORMAL) {
        set_theme_colors(app, normal_mode_colors, ArrayCount(normal_mode_colors));
    } else if (global_mode == INSERT) {
        set_theme_colors(app, insert_mode_colors, ArrayCount(insert_mode_colors));
    }
}

//
// Movement
// Everthing builds on movement
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

        if (global_mode == VISUAL) {
            Buffer_Seek seek = seek_line_char(global_highlight.cursor.line-1, global_highlight.cursor.character);
            highlight_seek(app, &global_highlight, seek);
        }
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

        if (global_mode == VISUAL) {
            Buffer_Seek seek = seek_line_char(global_highlight.cursor.line+1, global_highlight.cursor.character);
            highlight_seek(app, &global_highlight, seek);
        }
    }
}

static bool at_line_boundary(Application_Links *app, Full_Cursor cursor, bool moving_left)
{
    uint32_t access = AccessProtected;
    View_Summary view = get_active_view(app, access);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);

    int boundary_pos = (moving_left) ? buffer_get_line_start(app, &buffer, cursor.line)
                                     : buffer_get_line_end(app, &buffer, cursor.line);
    return (cursor.pos == boundary_pos);
}


static void vim_move_left(Application_Links *app)
{
    uint32_t access = AccessProtected;
    View_Summary view = get_active_view(app, access);

    if (global_mode == NORMAL && !at_line_boundary(app, view.cursor, true)) {
        exec_command(app, move_left);
    } else if (global_mode == VISUAL && !at_line_boundary(app, global_highlight.cursor, true)) {
        Buffer_Seek seek = seek_pos(global_highlight.cursor.pos-1);
        highlight_seek(app, &global_highlight, seek);
    }
}

static void vim_move_right(Application_Links *app)
{
    uint32_t access = AccessProtected;
    View_Summary view = get_active_view(app, access);

    if (global_mode == NORMAL && !at_line_boundary(app, view.cursor, false)) {
        exec_command(app, move_right);
    } else if (global_mode == VISUAL && !at_line_boundary(app, global_highlight.cursor, false)) {
        Buffer_Seek seek = seek_pos(global_highlight.cursor.pos+1);
        highlight_seek(app, &global_highlight, seek);
    }
}

static void vim_move_back_word(Application_Links *app)
{
    uint32_t access = AccessProtected;
    View_Summary view = get_active_view(app, access);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);

    int pos = (global_mode == NORMAL) ? view.cursor.pos : global_highlight.cursor.pos;

    Cpp_Get_Token_Result get_result = {0};
    if (buffer_get_token_index(app, &buffer, pos, &get_result)) {
        int token_index = get_result.token_index;

        Cpp_Token chunk[2];
        Stream_Tokens stream = {0};
        if (init_stream_tokens(&stream, app, &buffer, token_index, chunk, 2)) {
            Cpp_Token *token = stream.tokens + token_index;
            if (pos == token->start) {
                token_index -= 1;
            }
            if (token_index < stream.start) {
                if (!backward_stream_tokens(&stream)) {
                    return;
                }
            }
            token = stream.tokens + token_index;

            if (global_mode == NORMAL) {
                view_set_cursor(app, &view, seek_pos(token->start), true);
            } else if (global_mode == VISUAL) {
                highlight_seek(app, &global_highlight, seek_pos(token->start));
            }
        }
    }
}

static void vim_move_forward_word(Application_Links *app)
{
    uint32_t access = AccessProtected;
    View_Summary view = get_active_view(app, access);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);

    int pos = (global_mode == NORMAL) ? view.cursor.pos : global_highlight.cursor.pos;

    Cpp_Get_Token_Result get_result = {0};
    if (buffer_get_token_index(app, &buffer, pos, &get_result)) {
        int token_index = get_result.token_index;

        Cpp_Token chunk[2];
        Stream_Tokens stream = {0};
        if (init_stream_tokens(&stream, app, &buffer, token_index, chunk, 2)){
            int target_token_index = token_index + 1;
            if (target_token_index < stream.token_count) {
                if (target_token_index == stream.end) {
                    if (!forward_stream_tokens(&stream)) {
                        return;
                    }
                }

                Cpp_Token *token = stream.tokens + target_token_index;

                if (global_mode == NORMAL) {
                    view_set_cursor(app, &view, seek_pos(token->start), true);
                } else if (global_mode == VISUAL) {
                    highlight_seek(app, &global_highlight, seek_pos(token->start));
                }
            }
        }
    }
}

static void vim_move_forward_word_end(Application_Links *app)
{
    uint32_t access = AccessProtected;
    View_Summary view = get_active_view(app, access);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);

    int pos = (global_mode == NORMAL) ? view.cursor.pos : global_highlight.cursor.pos;

    Cpp_Get_Token_Result get_result = {0};
    if (buffer_get_token_index(app, &buffer, pos, &get_result)) {
        int token_index = get_result.token_index;
        if (get_result.in_whitespace || pos == get_result.token_end-1) {
            token_index+=1;
        }

        Cpp_Token chunk[1];
        Stream_Tokens stream = {0};
        if (init_stream_tokens(&stream, app, &buffer, token_index, chunk, 1)){
            Cpp_Token *token = stream.tokens + token_index;

            if (global_mode == NORMAL) {
                view_set_cursor(app, &view, seek_pos(token->start+token->size-1), true);
            } else if (global_mode == VISUAL) {
                highlight_seek(app, &global_highlight, seek_pos(token->start+token->size-1));
            }
        }
    }
}

static void vim_move_to_file_end(Application_Links *app)
{
    uint32_t access = AccessProtected;
    View_Summary view = get_active_view(app, access);
    Buffer_Seek seek = seek_xy(0.0f, max_f32, 1, true);

    if (global_mode == NORMAL) {
        view_set_cursor(app, &view, seek, true);
    } else if (global_mode == VISUAL) {
        highlight_seek(app, &global_highlight, seek);
    }
}

static void vim_move_beginning_of_line(Application_Links *app)
{
    if (global_mode == NORMAL) {
        exec_command(app, seek_beginning_of_line);
    } else if (global_mode == VISUAL) {
        uint32_t access = AccessProtected;
        View_Summary view = get_active_view(app, access);
        Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);

        int pos = buffer_get_line_start(app, &buffer, global_highlight.cursor.line);
        Buffer_Seek seek = seek_pos(pos);
        highlight_seek(app, &global_highlight, seek);
    }
}

static void vim_move_end_of_line(Application_Links *app)
{
    if (global_mode == NORMAL) {
        exec_command(app, seek_end_of_line);
    } else if (global_mode == VISUAL) {
        uint32_t access = AccessProtected;
        View_Summary view = get_active_view(app, access);
        Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);

        int pos = buffer_get_line_end(app, &buffer, global_highlight.cursor.line);
        Buffer_Seek seek = seek_pos(pos);
        highlight_seek(app, &global_highlight, seek);
    }
}

static void vim_page_up(Application_Links *app)
{
    // TODO(joe): Any active mode should be affected by this movement.
    exec_command(app, page_up);
}

static void vim_page_down(Application_Links *app)
{
    // TODO(joe): Any active mode should be affected by this movement.
    exec_command(app, page_down);
}

//
// Vim Yank
//
static void yank_range(Application_Links *app, int start, int end)
{
    uint32_t access = AccessProtected;
    View_Summary view = get_active_view(app, access);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);

    uint32_t size = end-start+1;
    register_ensure_storage_for_size(&global_yank_register, size);
    if (buffer_read_range(app, &buffer, start, end+1, global_yank_register.content)) {
        global_yank_register.size = size;

        int start_line = buffer_get_line_number(app, &buffer, start);
        int end_line = buffer_get_line_number(app, &buffer, end);
        if (start_line == end_line) {
            int line_start_pos = buffer_get_line_start(app, &buffer, start_line);
            int line_end_pos = buffer_get_line_end(app, &buffer, start_line);
            if (start == line_start_pos && end == line_end_pos) {
                global_yank_register.type = WHOLE_LINE;
            } else {
                global_yank_register.type = PARTIAL_LINE;
            }
        } else {
            global_yank_register.type = MULTIPLE_LINES;
        }
    }
}

static void yank_current_line(Application_Links *app)
{
	uint32_t access = AccessProtected;
    View_Summary view = get_active_view(app, access);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);
    int32_t start = buffer_get_line_start(app, &buffer, view.cursor.line);
    int32_t end = buffer_get_line_end(app, &buffer, view.cursor.line);

    yank_range(app, start, end);
}
//
// Vim Highlight
//

static VimHighlight highlight_start(Application_Links *app)
{
    VimHighlight highlight = {0};

    uint32_t access = AccessProtected;
    View_Summary view = get_active_view(app, access);

    highlight.cursor = view.cursor;
    highlight.start = view.cursor.pos;
    highlight.end = view.cursor.pos;

    view_set_highlight(app, &view, highlight.start, highlight.end+1, true);

    return highlight;
}

static void highlight_end(Application_Links *app, VimHighlight *highlight, bool update_cursor)
{
    uint32_t access = AccessProtected;
    View_Summary view = get_active_view(app, access);

    view_set_highlight(app, &view, highlight->start, highlight->end+1, false);

    if (update_cursor) {
        view_set_cursor(app, &view, seek_pos(highlight->cursor.pos), true);
    }
}

static void enter_visual_mode(Application_Links *app)
{
    global_mode = VISUAL;
    sync_to_mode(app);

    int access = AccessAll;
    View_Summary view = get_active_view(app, access);
    set_current_map(app, view.buffer_id, mapid_visual);

    global_highlight = highlight_start(app);
}

static void exit_visual_mode(Application_Links *app, bool update_cursor)
{
    global_mode = NORMAL;
    sync_to_mode(app);

    int access = AccessAll;
    View_Summary view = get_active_view(app, access);
    set_current_map(app, view.buffer_id, mapid_normal);

    highlight_end(app, &global_highlight, update_cursor);
}

CUSTOM_COMMAND_SIG(toggle_visual_mode)
{
    if (global_mode != VISUAL) {
        enter_visual_mode(app);
    } else {
        exit_visual_mode(app, true);
    }
}

static void highlight_seek(Application_Links *app, VimHighlight *highlight, Buffer_Seek seek)
{
    uint32_t access = AccessProtected;
    View_Summary view = get_active_view(app, access);

    int old_pos = highlight->cursor.pos;
    if (view_compute_cursor(app, &view, seek, &highlight->cursor)) {
        if (old_pos == highlight->start) {
            highlight->start = highlight->cursor.pos;
        } else if (old_pos == highlight->end) {
            highlight->end = highlight->cursor.pos;
        } else {
            assert(!"Unexpected highlight position");
        }

        if (highlight->end < highlight->start) {
            int temp = highlight->start;
            highlight->start = highlight->end;
            highlight->end = temp;
        }

        view_set_highlight(app, &view, highlight->start, highlight->end+1, true);
    }
}

//
// Vim Delete
//

void toggle_visual_mode(Application_Links *app);

CUSTOM_COMMAND_SIG(vim_enter_delete_command_mode)
{
    global_command_mode = DELETE;

    uint32_t access = AccessAll;
    View_Summary view = get_active_view(app, access);
    set_current_map(app, view.buffer_id, mapid_delete);
}

static void vim_delete_range(Application_Links *app, int start, int end, bool save_to_yank_register)
{
    View_Summary view = get_active_view(app, AccessOpen);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessOpen);

    view_set_cursor(app, &view, seek_pos(start), true);

    if (save_to_yank_register) {
        yank_range(app, start, end);
    }
    buffer_replace_range(app, &buffer, start, end+1, 0, 0);
}

static void vim_delete(Application_Links *app)
{
    if (global_mode == VISUAL) {
        vim_delete_range(app, global_highlight.start, global_highlight.end, true);
        exit_visual_mode(app, false);
    } else if (global_command_mode == DELETE) {
        yank_current_line(app);
        exec_command(app, delete_line);
        global_command_mode = NONE;
    } else {
        vim_enter_delete_command_mode(app);
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

static void enter_z_command_mode()
{
    global_command_mode = Z_COMMANDS;
}

// Mode Switching

CUSTOM_COMMAND_SIG(switch_to_insert_mode)
{
    global_mode = INSERT;
    sync_to_mode(app);

    int access = AccessAll;
    View_Summary view = get_active_view(app, access);
    set_current_map(app, view.buffer_id, mapid_insert);
}

CUSTOM_COMMAND_SIG(switch_to_normal_mode)
{
    VimMode prev_mode = global_mode;
    global_mode = NORMAL;
    sync_to_mode(app);

    if (prev_mode == VISUAL) {
        highlight_end(app, &global_highlight, true);
    }

    int access = AccessAll;
    View_Summary view = get_active_view(app, access);
    set_current_map(app, view.buffer_id, mapid_normal);
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

        if (global_mode == NORMAL) {
            view_set_cursor(app, &view, seek, true);
        } else {
            highlight_seek(app, &global_highlight, seek);
        }
    }
}

CUSTOM_COMMAND_SIG(handle_y_key)
{
    if (global_mode == NORMAL) {
        if (global_command_mode == NONE) {
            enter_y_command_mode();
        } else if (global_command_mode == Y_COMMANDS) {
            yank_current_line(app);
        }
    } else if (global_mode == VISUAL) {
        uint32_t access = AccessProtected;
        View_Summary view = get_active_view(app, access);
        Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);

        uint32_t size = global_highlight.end-global_highlight.start+1;
        register_ensure_storage_for_size(&global_yank_register, size);
        if (buffer_read_range(app, &buffer, global_highlight.start, global_highlight.end+1, global_yank_register.content)) {
            global_yank_register.size = size;
            if (buffer_get_line_number(app, &buffer, global_highlight.start) ==
                buffer_get_line_number(app, &buffer, global_highlight.end)) {
                global_yank_register.type = PARTIAL_LINE;
            } else {
                global_yank_register.type = MULTIPLE_LINES;
            }
        }
        exec_command(app, toggle_visual_mode);
    }
}

CUSTOM_COMMAND_SIG(handle_z_key)
{
    if (global_command_mode == NONE) {
        enter_z_command_mode();
    } else {
        // Center the view on the cursor.
        exec_command(app, center_view);
    }
}

//
// Editing
//
static void vim_append(Application_Links *app)
{
    uint32_t access = AccessOpen;
    View_Summary view = get_active_view(app, access);
    view_set_cursor(app, &view, seek_pos(view.cursor.pos+1), true);
    exec_command(app, switch_to_insert_mode);
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

// TODO(joe): vim_paste_before/after() can be compressed better.
static void vim_paste_before(Application_Links *app)
{
    Register *r = &global_yank_register;
    if (r->size == 0) return;

    uint32_t access = AccessOpen;
    View_Summary view = get_active_view(app, access);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);

    Partition *part = &global_part;
    Temp_Memory temp = begin_temp_memory(part);

    if (r->type == WHOLE_LINE) {
        // NOTE(joe): If the register is a whole line, then we will paste it on the previous line
        // regardless of where the cursor currently is. After pasting, the cursor will be at the
        // beginning of the pasted line.
        uint32_t edit_len = r->size+1;
        char *edit_str = push_array(part, char, edit_len);
        copy_fast_unsafe(edit_str, r->content);
        edit_str[edit_len-1] = '\n';

        int32_t insert_pos = buffer_get_line_start(app, &buffer, view.cursor.line);

        Buffer_Edit edit = {0};
        edit.str_start = 0;
        edit.len = edit_len;
        edit.start = insert_pos;
        edit.end = insert_pos;

        buffer_batch_edit(app, &buffer, edit_str, edit_len, &edit, 1, BatchEdit_Normal);

        Buffer_Seek seek = seek_pos(insert_pos);
        view_set_cursor(app, &view, seek, true);

        Theme_Color paste;
        paste.tag = Stag_Paste;
        get_theme_colors(app, &paste, 1);
        view_post_fade(app, &view, 0.667f, insert_pos, insert_pos + edit_len, paste.color);
    } else if (r->type == PARTIAL_LINE) {
        // NOTE(joe): A partial line register will paste its contents where the cursor is. After the
        // paste, the cursor will be at the end of the pasted register contents.
        uint32_t edit_len = r->size;
        char *edit_str = push_array(part, char, edit_len);
        copy_fast_unsafe(edit_str, r->content);

        int32_t insert_pos = view.cursor.pos;

        Buffer_Edit edit = {0};
        edit.str_start = 0;
        edit.len = edit_len;
        edit.start = insert_pos;
        edit.end = insert_pos;

        buffer_batch_edit(app, &buffer, edit_str, edit_len, &edit, 1, BatchEdit_Normal);

        Buffer_Seek seek = seek_pos(insert_pos+edit_len-1);
        view_set_cursor(app, &view, seek, true);

        Theme_Color paste;
        paste.tag = Stag_Paste;
        get_theme_colors(app, &paste, 1);
        view_post_fade(app, &view, 0.667f, insert_pos, insert_pos + edit_len, paste.color);
    } else if (r->type == MULTIPLE_LINES) {
        // NOTE(joe): A Multiple lines register will paste its contents where the cursor is. After the
        // paste, the cursor will be at the beginning of the pasted register contents.
        uint32_t edit_len = r->size;
        char *edit_str = push_array(part, char, edit_len);
        copy_fast_unsafe(edit_str, r->content);

        int32_t insert_pos = view.cursor.pos;

        Buffer_Edit edit = {0};
        edit.str_start = 0;
        edit.len = edit_len;
        edit.start = insert_pos;
        edit.end = insert_pos;

        buffer_batch_edit(app, &buffer, edit_str, edit_len, &edit, 1, BatchEdit_Normal);

        Buffer_Seek seek = seek_pos(insert_pos);
        view_set_cursor(app, &view, seek, true);

        Theme_Color paste;
        paste.tag = Stag_Paste;
        get_theme_colors(app, &paste, 1);
        view_post_fade(app, &view, 0.667f, insert_pos, insert_pos + edit_len, paste.color);
    }

    end_temp_memory(temp);
}

static void vim_paste_after(Application_Links *app)
{
    Register *r = &global_yank_register;
    if (r->size == 0) return;

    uint32_t access = AccessOpen;
    View_Summary view = get_active_view(app, access);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);

    Partition *part = &global_part;
    Temp_Memory temp = begin_temp_memory(part);

    if (r->type == WHOLE_LINE) {
        // NOTE(joe): If the register is a whole line, then we will paste it on the next line
        // regardless of where the cursor currently is. After pasting, the cursor will be at the
        // beginning of the pasted line.
        uint32_t edit_len = r->size+1;
        char *edit_str = push_array(part, char, edit_len);
        edit_str[0] = '\n';
        copy_fast_unsafe(edit_str+1, r->content);

        int32_t insert_pos = buffer_get_line_end(app, &buffer, view.cursor.line);

        Buffer_Edit edit = {0};
        edit.str_start = 0;
        edit.len = edit_len;
        edit.start = insert_pos;
        edit.end = insert_pos;

        buffer_batch_edit(app, &buffer, edit_str, edit_len, &edit, 1, BatchEdit_Normal);

        Buffer_Seek seek = seek_line_char(view.cursor.line+1, 0);
        view_set_cursor(app, &view, seek, true);

        Theme_Color paste;
        paste.tag = Stag_Paste;
        get_theme_colors(app, &paste, 1);
        view_post_fade(app, &view, 0.667f, insert_pos + 1, insert_pos + 1 + edit_len, paste.color);
    } else if (r->type == PARTIAL_LINE) {
        // NOTE(joe): A partial line register will paste its contents where the cursor is. After the
        // paste, the cursor will be at the end of the pasted register contents.
        uint32_t edit_len = r->size;
        char *edit_str = push_array(part, char, edit_len);
        copy_fast_unsafe(edit_str, r->content);

        int32_t insert_pos = view.cursor.pos+1;

        Buffer_Edit edit = {0};
        edit.str_start = 0;
        edit.len = edit_len;
        edit.start = insert_pos;
        edit.end = insert_pos;

        buffer_batch_edit(app, &buffer, edit_str, edit_len, &edit, 1, BatchEdit_Normal);

        Buffer_Seek seek = seek_pos(insert_pos+edit_len-1);
        view_set_cursor(app, &view, seek, true);

        Theme_Color paste;
        paste.tag = Stag_Paste;
        get_theme_colors(app, &paste, 1);
        view_post_fade(app, &view, 0.667f, insert_pos, insert_pos + edit_len, paste.color);
    } else if (r->type == MULTIPLE_LINES) {
        // NOTE(joe): A Multiple lines register will paste its contents where the cursor is. After the
        // paste, the cursor will be at the beginning of the pasted register contents.
        uint32_t edit_len = r->size;
        char *edit_str = push_array(part, char, edit_len);
        copy_fast_unsafe(edit_str, r->content);

        int32_t insert_pos = view.cursor.pos+1;

        Buffer_Edit edit = {0};
        edit.str_start = 0;
        edit.len = edit_len;
        edit.start = insert_pos;
        edit.end = insert_pos;

        buffer_batch_edit(app, &buffer, edit_str, edit_len, &edit, 1, BatchEdit_Normal);

        Buffer_Seek seek = seek_pos(insert_pos);
        view_set_cursor(app, &view, seek, true);

        Theme_Color paste;
        paste.tag = Stag_Paste;
        get_theme_colors(app, &paste, 1);
        view_post_fade(app, &view, 0.667f, insert_pos, insert_pos + edit_len, paste.color);
    }


    end_temp_memory(temp);
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
        {Stag_At_Highlight, 0xFF000000},
        {Stag_Margin_Active, 0xFF719E07},
    };
    set_theme_colors(app, colors, ArrayCount(colors));

    set_global_face_by_name(app, literal("SourceCodePro-Regular"), true);

    global_mode = NORMAL;
    sync_to_mode(app);

    return 0;
}

OPEN_FILE_HOOK_SIG(greedy_file_settings)
{
#if 0
    unsigned int access = AccessAll;
    Buffer_Summary buffer = get_buffer(app, buffer_id, access);

    buffer_set_setting(app, &buffer, BufferSetting_Lex, 1);
    buffer_set_setting(app, &buffer, BufferSetting_VirtualWhitespace, 0);
    buffer_set_setting(app, &buffer, BufferSetting_Eol, 0); // unix endings
    buffer_set_setting(app, &buffer, BufferSetting_MapID, mapid_file);
#endif

    default_file_settings(app, buffer_id);
    set_current_map(app, buffer_id, mapid_normal);

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
    set_open_file_hook(context, greedy_file_settings);
    set_new_file_hook(context, greedy_file_settings);
    set_scroll_rule(context, smooth_scroll_rule);
    set_end_file_hook(context, default_end_file);

    //
    // 4coder specific
    //
    begin_map(context, mapid_global);
    {
        bind(context, key_f4, MDFR_ALT, exit_4coder);
        bind(context, '\n', MDFR_ALT, toggle_fullscreen);
        bind(context, key_insert, MDFR_SHIFT, paste_and_indent);
    }
    end_map(context);

    //
    // VIM Movement Map
    //
    begin_map(context, mapid_movement);
    {
        inherit_map(context, mapid_global);

        bind(context, 'b', MDFR_NONE, vim_move_back_word);
        bind(context, 'e', MDFR_NONE, vim_move_forward_word_end);
        bind(context, 'h', MDFR_NONE, vim_move_left);
        bind(context, 'j', MDFR_NONE, vim_move_down);
        bind(context, 'k', MDFR_NONE, vim_move_up);
        bind(context, 'l', MDFR_NONE, vim_move_right);
        bind(context, 'w', MDFR_NONE, vim_move_forward_word);
        bind(context, '$', MDFR_NONE, vim_move_end_of_line);
        bind(context, '^', MDFR_NONE, vim_move_beginning_of_line);

        bind(context, 'G', MDFR_SHIFT, vim_move_to_file_end);

        bind(context, 'b', MDFR_CTRL, vim_page_up);
        bind(context, 'f', MDFR_CTRL, vim_page_down);
    }
    end_map(context);

    //
    // VIM Normal Mode
    //
    begin_map(context, mapid_normal);
    {
        inherit_map(context, mapid_movement);

        bind(context, 'a', MDFR_NONE, vim_append);
        bind(context, 'd', MDFR_NONE, vim_enter_delete_command_mode);
        bind(context, 'g', MDFR_NONE, handle_g_key);
        bind(context, 'i', MDFR_NONE, switch_to_insert_mode);
        bind(context, 'o', MDFR_NONE, vim_newline_below_then_insert);
        bind(context, 'p', MDFR_NONE, vim_paste_after);
        bind(context, 'u', MDFR_NONE, undo);
        bind(context, 'v', MDFR_NONE, toggle_visual_mode);
        bind(context, 'x', MDFR_NONE, delete_char);
        bind(context, 'y', MDFR_NONE, handle_y_key);
        bind(context, 'z', MDFR_NONE, handle_z_key);

        bind(context, 'O', MDFR_SHIFT, vim_newline_above_then_insert);
        bind(context, 'P', MDFR_SHIFT, vim_paste_before);

        bind(context, key_back, MDFR_NONE, vim_move_left);
        bind(context, key_esc,  MDFR_NONE, switch_to_normal_mode);
        bind(context, '*',      MDFR_NONE, search_identifier);
        bind(context, ':',      MDFR_NONE, vim_ex_command);
        bind(context, '/',      MDFR_NONE, search);

        bind(context, 'h', MDFR_CTRL, change_active_panel_backwards);;
        bind(context, 'j', MDFR_CTRL, change_active_panel);;
        bind(context, 'k', MDFR_CTRL, change_active_panel_backwards);;
        bind(context, 'l', MDFR_CTRL, change_active_panel);;
        bind(context, 'r', MDFR_CTRL, redo);;
        bind(context, 'p', MDFR_CTRL, interactive_open_or_new);;
    }
    end_map(context);

    //
    // VIM Insert Mode
    //
    begin_map(context, mapid_insert);
    {
        inherit_map(context, mapid_global);
        bind_vanilla_keys(context, write_character);

        bind(context, key_back, MDFR_NONE, backspace_char);
        bind(context, key_esc,  MDFR_NONE, switch_to_normal_mode);
    }
    end_map(context);

    //
    // VIM Visual Mode
    //
    begin_map(context, mapid_visual);
    {
        inherit_map(context, mapid_movement);

        bind(context, 'v',     MDFR_NONE, toggle_visual_mode);
        bind(context, 'd',     MDFR_NONE, vim_delete);
        bind(context, 'x',     MDFR_NONE, vim_delete);
        bind(context, key_esc, MDFR_NONE, switch_to_normal_mode);
    }
    end_map(context);

    //
    // VIM Delete chord
    //
    begin_map(context, mapid_delete);
    {
        inherit_map(context, mapid_movement);

        bind(context, key_esc,  MDFR_NONE, switch_to_normal_mode);
        bind(context, 'd',      MDFR_NONE, vim_delete);
    }
    end_map(context);


    end_bind_helper(context);
    return context->write_total;
}
