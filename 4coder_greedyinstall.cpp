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

START_HOOK_SIG(greedy_start)
{
    default_start(app, files, file_count, flags, flag_count);

    set_fullscreen(app, true);

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

CUSTOM_COMMAND_SIG(vim_move_left)
{
    uint32_t access = AccessProtected;
    View_Summary view = get_active_view(app, access);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);

    int line_start = buffer_get_line_start(app, &buffer, view.cursor.line);
    if (view.cursor.pos != line_start)
    {
        exec_command(app, move_left);
    }
}

CUSTOM_COMMAND_SIG(vim_move_right)
{
    uint32_t access = AccessProtected;
    View_Summary view = get_active_view(app, access);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);

    int line_end = buffer_get_line_end(app, &buffer, view.cursor.line);
    if (view.cursor.pos != line_end)
    {
        exec_command(app, move_right);
    }
}

//
// Bindings
//

void vim_handle_key_normal(Application_Links *app, Key_Code code)
{
    switch(code)
    {
        case 'a': exec_command(app, vim_append); break;
        case 'h': exec_command(app, vim_move_left); break;
        case 'i': exec_command(app, switch_to_insert_mode); break;
        case 'j': exec_command(app, vim_move_down); break;
        case 'k': exec_command(app, vim_move_up); break;
        case 'l': exec_command(app, vim_move_right); break;
    }
}

void vim_handle_unmodified_key(Application_Links *app, Key_Code code)
{
    if (global_normal_mode) {
        vim_handle_key_normal(app, code);
    } else {
        exec_command(app, write_character);
    }
}

CUSTOM_COMMAND_SIG(vim_a) { vim_handle_unmodified_key(app, 'a'); }
CUSTOM_COMMAND_SIG(vim_b) { vim_handle_unmodified_key(app, 'b'); }
CUSTOM_COMMAND_SIG(vim_c) { vim_handle_unmodified_key(app, 'c'); }
CUSTOM_COMMAND_SIG(vim_d) { vim_handle_unmodified_key(app, 'd'); }
CUSTOM_COMMAND_SIG(vim_e) { vim_handle_unmodified_key(app, 'e'); }
CUSTOM_COMMAND_SIG(vim_f) { vim_handle_unmodified_key(app, 'f'); }
CUSTOM_COMMAND_SIG(vim_g) { vim_handle_unmodified_key(app, 'g'); }
CUSTOM_COMMAND_SIG(vim_h) { vim_handle_unmodified_key(app, 'h'); }
CUSTOM_COMMAND_SIG(vim_i) { vim_handle_unmodified_key(app, 'i'); }
CUSTOM_COMMAND_SIG(vim_j) { vim_handle_unmodified_key(app, 'j'); }
CUSTOM_COMMAND_SIG(vim_k) { vim_handle_unmodified_key(app, 'k'); }
CUSTOM_COMMAND_SIG(vim_l) { vim_handle_unmodified_key(app, 'l'); }
CUSTOM_COMMAND_SIG(vim_m) { vim_handle_unmodified_key(app, 'm'); }
CUSTOM_COMMAND_SIG(vim_n) { vim_handle_unmodified_key(app, 'n'); }
CUSTOM_COMMAND_SIG(vim_o) { vim_handle_unmodified_key(app, 'o'); }
CUSTOM_COMMAND_SIG(vim_p) { vim_handle_unmodified_key(app, 'p'); }
CUSTOM_COMMAND_SIG(vim_q) { vim_handle_unmodified_key(app, 'q'); }
CUSTOM_COMMAND_SIG(vim_r) { vim_handle_unmodified_key(app, 'r'); }
CUSTOM_COMMAND_SIG(vim_s) { vim_handle_unmodified_key(app, 's'); }
CUSTOM_COMMAND_SIG(vim_t) { vim_handle_unmodified_key(app, 't'); }
CUSTOM_COMMAND_SIG(vim_u) { vim_handle_unmodified_key(app, 'u'); }
CUSTOM_COMMAND_SIG(vim_v) { vim_handle_unmodified_key(app, 'v'); }
CUSTOM_COMMAND_SIG(vim_w) { vim_handle_unmodified_key(app, 'w'); }
CUSTOM_COMMAND_SIG(vim_x) { vim_handle_unmodified_key(app, 'x'); }
CUSTOM_COMMAND_SIG(vim_y) { vim_handle_unmodified_key(app, 'y'); }
CUSTOM_COMMAND_SIG(vim_z) { vim_handle_unmodified_key(app, 'z'); }

extern "C" GET_BINDING_DATA(get_bindings)
{
    Bind_Helper context_actual = begin_bind_helper(data, size);
    Bind_Helper *context = &context_actual;

    set_start_hook(context, greedy_start);
    set_command_caller(context, default_command_caller);
    set_open_file_hook(context, default_file_settings);
    //set_scroll_rule(context, casey_smooth_scroll_rule);
    set_end_file_hook(context, default_end_file);

    begin_map(context, mapid_file);
    {
        bind_vanilla_keys(context, write_character);

        // Mode switching
        bind(context, key_esc, MDFR_NONE, switch_to_normal_mode);

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

        bind(context, key_f4, MDFR_ALT, exit_4coder);
    }
    end_map(context);

    end_bind_helper(context);
    return context->write_total;
}
