/*
 * ISF(Input Service Framework)
 *
 * ISF is based on SCIM 1.4.7 and extended for supporting more mobile fitable.
 * Copyright (c) 2012-2013 Samsung Electronics Co., Ltd.
 *
 * Contact: Jihoon Kim <jihoon48.kim@samsung.com>, Haifeng Deng <haifeng.deng@samsung.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#define Uses_SCIM_BACKEND
#define Uses_SCIM_IMENGINE_MODULE
#define Uses_SCIM_HELPER_MODULE
#define Uses_SCIM_HOTKEY
#define Uses_SCIM_PANEL_CLIENT

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/times.h>
#include <pthread.h>
#include <langinfo.h>
#include <unistd.h>

#include <Evas.h>
#include <Ecore_Evas.h>
#include <Ecore_X.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <glib.h>
#include <utilX.h>
#include <vconf.h>
#include <vconf-keys.h>
#include <notification.h>

#include "scim_private.h"
#include "scim.h"
#include "isf_imf_context.h"
#include "isf_imf_control_ui.h"

#ifndef CODESET
# define CODESET "INVALID"
#endif

#define ENABLE_BACKKEY 1

using namespace scim;

struct _EcoreIMFContextISFImpl {
    EcoreIMFContextISF      *parent;
    IMEngineInstancePointer  si;
    Ecore_X_Window           client_window;
    Evas                    *client_canvas;
    Ecore_IMF_Input_Mode     input_mode;
    WideString               preedit_string;
    AttributeList            preedit_attrlist;
    Ecore_IMF_Autocapital_Type autocapital_type;
    void                    *imdata;
    int                      imdata_size;
    int                      preedit_caret;
    int                      cursor_x;
    int                      cursor_y;
    int                      cursor_top_y;
    int                      cursor_pos;
    bool                     use_preedit;
    bool                     is_on;
    bool                     shared_si;
    bool                     preedit_started;
    bool                     preedit_updating;
    bool                     need_commit_preedit;
    bool                     prediction_allow;
    int                      next_shift_status;
    int                      shift_mode_enabled;

    EcoreIMFContextISFImpl  *next;
};

/* Input Context handling functions. */
static EcoreIMFContextISFImpl *new_ic_impl              (EcoreIMFContextISF     *parent);
static void                    delete_ic_impl           (EcoreIMFContextISFImpl *impl);
static void                    delete_all_ic_impl       (void);

static EcoreIMFContextISF     *find_ic                  (int                     id);


/* private functions */
static void     panel_slot_reload_config                (int                     context);
static void     panel_slot_exit                         (int                     context);
static void     panel_slot_update_candidate_item_layout (int                     context,
                                                         const std::vector<uint32> &row_items);
static void     panel_slot_update_lookup_table_page_size(int                     context,
                                                         int                     page_size);
static void     panel_slot_lookup_table_page_up         (int                     context);
static void     panel_slot_lookup_table_page_down       (int                     context);
static void     panel_slot_trigger_property             (int                     context,
                                                         const String           &property);
static void     panel_slot_process_helper_event         (int                     context,
                                                         const String           &target_uuid,
                                                         const String           &helper_uuid,
                                                         const Transaction      &trans);
static void     panel_slot_move_preedit_caret           (int                     context,
                                                         int                     caret_pos);
static void     panel_slot_update_preedit_caret         (int                     context,
                                                         int                     caret);
static void     panel_slot_select_aux                   (int                     context,
                                                         int                     aux_index);
static void     panel_slot_select_candidate             (int                     context,
                                                         int                     cand_index);
static void     panel_slot_process_key_event            (int                     context,
                                                         const KeyEvent         &key);
static void     panel_slot_commit_string                (int                     context,
                                                         const WideString       &wstr);
static void     panel_slot_forward_key_event            (int                     context,
                                                         const KeyEvent         &key);
static void     panel_slot_request_help                 (int                     context);
static void     panel_slot_request_factory_menu         (int                     context);
static void     panel_slot_change_factory               (int                     context,
                                                         const String           &uuid);
static void     panel_slot_reset_keyboard_ise           (int                     context);
static void     panel_slot_update_keyboard_ise          (int                     context);
static void     panel_slot_show_preedit_string          (int                     context);
static void     panel_slot_hide_preedit_string          (int                     context);
static void     panel_slot_update_preedit_string        (int                     context,
                                                         const WideString       &str,
                                                         const AttributeList    &attrs);
static void     panel_slot_get_surrounding_text         (int                     context,
                                                         int                     maxlen_before,
                                                         int                     maxlen_after);
static void     panel_slot_delete_surrounding_text      (int                     context,
                                                         int                     offset,
                                                         int                     len);

static void     panel_req_focus_in                      (EcoreIMFContextISF     *ic);
static void     panel_req_update_factory_info           (EcoreIMFContextISF     *ic);
static void     panel_req_update_spot_location          (EcoreIMFContextISF     *ic);
static void     panel_req_update_cursor_position        (EcoreIMFContextISF     *ic, int cursor_pos);
static void     panel_req_show_help                     (EcoreIMFContextISF     *ic);
static void     panel_req_show_factory_menu             (EcoreIMFContextISF     *ic);

/* Panel iochannel handler*/
static bool     panel_initialize                        (void);
static void     panel_finalize                          (void);
static Eina_Bool panel_iochannel_handler                (void                   *data,
                                                         Ecore_Fd_Handler       *fd_handler);

/* utility functions */
static bool     filter_hotkeys                          (EcoreIMFContextISF     *ic,
                                                         const KeyEvent         &key);
static void     turn_on_ic                              (EcoreIMFContextISF     *ic);
static void     turn_off_ic                             (EcoreIMFContextISF     *ic);
static void     set_ic_capabilities                     (EcoreIMFContextISF     *ic);

static void     initialize                              (void);
static void     finalize                                (void);

static void     open_next_factory                       (EcoreIMFContextISF     *ic);
static void     open_previous_factory                   (EcoreIMFContextISF     *ic);
static void     open_specific_factory                   (EcoreIMFContextISF     *ic,
                                                         const String           &uuid);
static void     initialize_modifier_bits                (Display *display);
static unsigned int scim_x11_keymask_scim_to_x11        (Display *display, uint16 scimkeymask);
static XKeyEvent createKeyEvent                         (bool press, int keycode, int modifiers, bool fake);
static void     send_x_key_event                        (const KeyEvent &key, bool fake);

static void     attach_instance                         (const IMEngineInstancePointer &si);

/* slot functions */
static void     slot_show_preedit_string                (IMEngineInstanceBase   *si);
static void     slot_show_aux_string                    (IMEngineInstanceBase   *si);
static void     slot_show_lookup_table                  (IMEngineInstanceBase   *si);

static void     slot_hide_preedit_string                (IMEngineInstanceBase   *si);
static void     slot_hide_aux_string                    (IMEngineInstanceBase   *si);
static void     slot_hide_lookup_table                  (IMEngineInstanceBase   *si);

static void     slot_update_preedit_caret               (IMEngineInstanceBase   *si,
                                                         int                     caret);
static void     slot_update_preedit_string              (IMEngineInstanceBase   *si,
                                                         const WideString       &str,
                                                         const AttributeList    &attrs);
static void     slot_update_aux_string                  (IMEngineInstanceBase   *si,
                                                         const WideString       &str,
                                                         const AttributeList    &attrs);
static void     slot_commit_string                      (IMEngineInstanceBase   *si,
                                                         const WideString       &str);
static void     slot_forward_key_event                  (IMEngineInstanceBase   *si,
                                                         const KeyEvent         &key);
static void     slot_update_lookup_table                (IMEngineInstanceBase   *si,
                                                         const LookupTable      &table);

static void     slot_register_properties                (IMEngineInstanceBase   *si,
                                                         const PropertyList     &properties);
static void     slot_update_property                    (IMEngineInstanceBase   *si,
                                                         const Property         &property);
static void     slot_beep                               (IMEngineInstanceBase   *si);
static void     slot_start_helper                       (IMEngineInstanceBase   *si,
                                                         const String           &helper_uuid);
static void     slot_stop_helper                        (IMEngineInstanceBase   *si,
                                                         const String           &helper_uuid);
static void     slot_send_helper_event                  (IMEngineInstanceBase   *si,
                                                         const String           &helper_uuid,
                                                         const Transaction      &trans);
static bool     slot_get_surrounding_text               (IMEngineInstanceBase   *si,
                                                         WideString             &text,
                                                         int                    &cursor,
                                                         int                     maxlen_before,
                                                         int                     maxlen_after);
static bool     slot_delete_surrounding_text            (IMEngineInstanceBase   *si,
                                                         int                     offset,
                                                         int                     len);

static void     slot_expand_candidate                   (IMEngineInstanceBase   *si);
static void     slot_contract_candidate                 (IMEngineInstanceBase   *si);

static void     slot_set_candidate_style                (IMEngineInstanceBase   *si,
                                                         ISF_CANDIDATE_PORTRAIT_LINE_T portrait_line,
                                                         ISF_CANDIDATE_MODE_T    mode);

static void     reload_config_callback                  (const ConfigPointer    &config);

static void     fallback_commit_string_cb               (IMEngineInstanceBase   *si,
                                                         const WideString       &str);
static void     _display_input_language                 (EcoreIMFContextISF *ic);

/* Local variables declaration */
static String                                           _language;
static EcoreIMFContextISFImpl                          *_used_ic_impl_list          = 0;
static EcoreIMFContextISFImpl                          *_free_ic_impl_list          = 0;
static EcoreIMFContextISF                              *_ic_list                    = 0;

static KeyboardLayout                                   _keyboard_layout            = SCIM_KEYBOARD_Default;
static int                                              _valid_key_mask             = SCIM_KEY_AllMasks;

static FrontEndHotkeyMatcher                            _frontend_hotkey_matcher;
static IMEngineHotkeyMatcher                            _imengine_hotkey_matcher;

static IMEngineInstancePointer                          _default_instance;

static ConfigModule                                    *_config_module              = 0;
static ConfigPointer                                    _config;
static BackEndPointer                                   _backend;

static EcoreIMFContextISF                              *_focused_ic                 = 0;

static bool                                             _scim_initialized           = false;

static int                                              _instance_count             = 0;
static int                                              _context_count              = 0;

static IMEngineFactoryPointer                           _fallback_factory;
static IMEngineInstancePointer                          _fallback_instance;
static PanelClient                                      _panel_client;
static int                                              _panel_client_id            = 0;

static Ecore_Fd_Handler                                *_panel_iochannel_read_handler = 0;
static Ecore_Fd_Handler                                *_panel_iochannel_err_handler  = 0;

static Ecore_X_Window                                   _client_window              = 0;
static Ecore_Event_Handler                             *_key_down_handler           = 0;
static Ecore_Event_Handler                             *_key_up_handler             = 0;

static bool                                             _on_the_spot                = true;
static bool                                             _shared_input_method        = false;
static double                                           space_key_time              = 0.0;

static Eina_Bool                                        autoperiod_allow            = EINA_FALSE;
static Eina_Bool                                        autocap_allow               = EINA_FALSE;
static Eina_Bool                                        desktop_mode                = EINA_FALSE;

static bool                                             _x_key_event_is_valid       = false;

static Display *__current_display      = 0;
static int      __current_alt_mask     = Mod1Mask;
static int      __current_meta_mask    = 0;
static int      __current_super_mask   = 0;
static int      __current_hyper_mask   = 0;
static int      __current_numlock_mask = Mod2Mask;

#define SHIFT_MODE_OFF  0xffe1
#define SHIFT_MODE_ON   0xffe2
#define SHIFT_MODE_LOCK 0xffe6
#define SHIFT_MODE_ENABLE 0x9fe7
#define SHIFT_MODE_DISABLE 0x9fe8

extern Ecore_IMF_Context *input_panel_ctx;

// A hack to shutdown the immodule cleanly even if im_module_exit () is not called when exiting.
class FinalizeHandler
{
public:
    FinalizeHandler () {
        SCIM_DEBUG_FRONTEND(1) << "FinalizeHandler::FinalizeHandler ()\n";
    }
    ~FinalizeHandler () {
        SCIM_DEBUG_FRONTEND(1) << "FinalizeHandler::~FinalizeHandler ()\n";
        finalize ();
    }
};

static FinalizeHandler                                  _finalize_handler;

EAPI ConfigPointer isf_imf_context_get_config (void)
{
    return _config;
}

EAPI EcoreIMFContextISF *
get_focused_ic ()
{
    return _focused_ic;
}

EAPI int
get_panel_client_id (void)
{
    return _panel_client_id;
}

EAPI Eina_Bool
get_desktop_mode ()
{
    return desktop_mode;
}

static unsigned int
get_time (void)
{
    unsigned int tint;
    struct timeval tv;
    struct timezone tz;           /* is not used since ages */
    gettimeofday (&tv, &tz);
    tint = tv.tv_sec * 1000;
    tint = tint / 1000 * 1000;
    tint = tint + tv.tv_usec / 1000;
    return tint;
}

/* Function Implementations */
static EcoreIMFContextISFImpl *
new_ic_impl (EcoreIMFContextISF *parent)
{
    EcoreIMFContextISFImpl *impl = NULL;

    if (_free_ic_impl_list != NULL) {
        impl = _free_ic_impl_list;
        _free_ic_impl_list = _free_ic_impl_list->next;
    } else {
        impl = new EcoreIMFContextISFImpl;
        if (impl == NULL)
            return NULL;
    }

    impl->autocapital_type = ECORE_IMF_AUTOCAPITAL_TYPE_NONE;
    impl->next_shift_status = 0;
    impl->shift_mode_enabled = 1;
    impl->next = _used_ic_impl_list;
    _used_ic_impl_list = impl;

    impl->parent = parent;
    impl->imdata = NULL;
    impl->imdata_size = 0;

    return impl;
}

static void
delete_ic_impl (EcoreIMFContextISFImpl *impl)
{
    EcoreIMFContextISFImpl *rec = _used_ic_impl_list, *last = 0;

    for (; rec != 0; last = rec, rec = rec->next) {
        if (rec == impl) {
            if (last != 0)
                last->next = rec->next;
            else
                _used_ic_impl_list = rec->next;

            rec->next = _free_ic_impl_list;
            _free_ic_impl_list = rec;

            if (rec->imdata) {
                free (rec->imdata);
                rec->imdata = NULL;
            }

            rec->imdata_size = 0;
            rec->parent = 0;
            rec->si.reset ();
            rec->client_window = 0;
            rec->preedit_string = WideString ();
            rec->preedit_attrlist.clear ();

            return;
        }
    }
}

static void
delete_all_ic_impl (void)
{
    EcoreIMFContextISFImpl *it = _used_ic_impl_list;

    while (it != 0) {
        _used_ic_impl_list = it->next;
        delete it;
        it = _used_ic_impl_list;
    }

    it = _free_ic_impl_list;
    while (it != 0) {
        _free_ic_impl_list = it->next;
        delete it;
        it = _free_ic_impl_list;
    }
}

static EcoreIMFContextISF *
find_ic (int id)
{
    EcoreIMFContextISFImpl *rec = _used_ic_impl_list;

    while (rec != 0) {
        if (rec->parent && rec->parent->id == id)
            return rec->parent;
        rec = rec->next;
    }

    return 0;
}

static Eina_Bool
_key_down_cb (void *data, int type, void *event)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    Evas_Event_Key_Down *ev = (Evas_Event_Key_Down *)event;
    if (!ev || !_focused_ic || !_focused_ic->ctx) return ECORE_CALLBACK_RENEW;

    if (!strcmp (ev->keyname, KEY_END) &&
        ecore_imf_context_input_panel_state_get (_focused_ic->ctx) != ECORE_IMF_INPUT_PANEL_STATE_HIDE) {
        LOGD ("END key is pressed\n");
        return ECORE_CALLBACK_CANCEL;
    }

    return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_key_up_cb (void *data, int type, void *event)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    Evas_Event_Key_Down *ev = (Evas_Event_Key_Down *)event;
    if (!ev || !_focused_ic || !_focused_ic->ctx) return ECORE_CALLBACK_RENEW;

    if (!strcmp (ev->keyname, KEY_END) &&
        ecore_imf_context_input_panel_state_get (_focused_ic->ctx) != ECORE_IMF_INPUT_PANEL_STATE_HIDE) {
        LOGD ("END key is released\n");
        isf_imf_context_input_panel_instant_hide (_focused_ic->ctx);
        return ECORE_CALLBACK_CANCEL;
    }

    return ECORE_CALLBACK_RENEW;
}

static void
_check_desktop_mode (Ecore_X_Window win)
{
    char *profile = ecore_x_e_window_profile_get (win);
    if (profile && (strcmp (profile, "desktop") == 0)) {
        desktop_mode = EINA_TRUE;
    } else {
        desktop_mode = EINA_FALSE;
    }

    if (profile)
        free (profile);
}

static Eina_Bool
_x_prop_change (void *data, int type, void *event)
{
    Ecore_X_Event_Window_Property *e = (Ecore_X_Event_Window_Property *)event;
    Ecore_X_Window xwin = (Ecore_X_Window)data;

    if (e->win != xwin) return ECORE_CALLBACK_PASS_ON;

    if (e->atom == ECORE_X_ATOM_E_PROFILE) {
        _check_desktop_mode (e->win);
    }

    return ECORE_CALLBACK_PASS_ON;
}

EAPI int
register_key_handler ()
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

#ifdef ENABLE_BACKKEY
    if (!_key_down_handler)
        _key_down_handler = ecore_event_handler_add (ECORE_EVENT_KEY_DOWN, _key_down_cb, NULL);

    if (!_key_up_handler)
        _key_up_handler = ecore_event_handler_add (ECORE_EVENT_KEY_UP, _key_up_cb, NULL);
#endif

    return EXIT_SUCCESS;
}

EAPI int
unregister_key_handler ()
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    if (_key_down_handler) {
        ecore_event_handler_del (_key_down_handler);
        _key_down_handler = NULL;
    }

    if (_key_up_handler) {
        ecore_event_handler_del (_key_up_handler);
        _key_up_handler = NULL;
    }

    return EXIT_SUCCESS;
}

static void
set_prediction_allow (Ecore_IMF_Context *ctx, bool prediction)
{
    EcoreIMFContextISF *context_scim = (EcoreIMFContextISF *)ecore_imf_context_data_get (ctx);

    if (context_scim && context_scim->impl) {
        _panel_client.prepare (context_scim->id);
        context_scim->impl->si->set_prediction_allow (prediction);
        _panel_client.send ();
    }
}

static void
autoperiod_insert (Ecore_IMF_Context *ctx)
{
    char *plain_str = NULL;
    char *markup_str = NULL;
    int cursor_pos = 0;
    Eina_Unicode *ustr = NULL;
    Ecore_IMF_Event_Delete_Surrounding ev;

    if (!ctx) return;

    Ecore_IMF_Input_Panel_Layout layout = ecore_imf_context_input_panel_layout_get (ctx);
    if (layout != ECORE_IMF_INPUT_PANEL_LAYOUT_NORMAL)
        return;

    if (autoperiod_allow == EINA_FALSE)
        return;

    if ((ecore_time_get () - space_key_time) > DOUBLE_SPACE_INTERVAL)
        goto done;

    ecore_imf_context_surrounding_get (ctx, &markup_str, &cursor_pos);
    if (!markup_str) goto done;

    // Convert into plain string
    plain_str = evas_textblock_text_markup_to_utf8 (NULL, markup_str);
    if (!plain_str) goto done;

    // Convert string from UTF-8 to unicode
    ustr = eina_unicode_utf8_to_unicode (plain_str, NULL);
    if (!ustr) goto done;

    if (cursor_pos < 2) goto done;

    if (((ustr[cursor_pos-2] != ':') && (ustr[cursor_pos-2] != ';') &&
        (ustr[cursor_pos-2] != '.') && (ustr[cursor_pos-2] != ',') &&
        (ustr[cursor_pos-2] != '?') && (ustr[cursor_pos-2] != '!') &&
        (ustr[cursor_pos-2] != ' ')) && ((ustr[cursor_pos-1] == ' ') || (ustr[cursor_pos-1] == '\240'))) {
        ev.ctx = ctx;
        ev.n_chars = 1;
        ev.offset = -1;
        ecore_imf_context_delete_surrounding_event_add (ctx, -1, 1);
        ecore_imf_context_event_callback_call (ctx, ECORE_IMF_CALLBACK_DELETE_SURROUNDING, &ev);

        ecore_imf_context_commit_event_add (ctx, ".");
        ecore_imf_context_event_callback_call (ctx, ECORE_IMF_CALLBACK_COMMIT, (void *)".");
     }

done:
    if (markup_str) free (markup_str);
    if (plain_str) free (plain_str);
    if (ustr) free (ustr);
    space_key_time = ecore_time_get ();
}

static Eina_Bool
analyze_surrounding_text (Ecore_IMF_Context *ctx)
{
    char *plain_str = NULL;
    char *markup_str = NULL;
    Eina_Unicode puncs[] = {'.', '!', '?', 0x00BF /* ¿ */, 0x00A1 /* ¡ */};
    Eina_Unicode *ustr = NULL;
    Eina_Bool ret = EINA_FALSE;
    Eina_Bool detect_space = EINA_FALSE;
    int cursor_pos = 0;
    int i = 0, j = 0;
    const int punc_num = sizeof (puncs) / sizeof (puncs[0]);
    EcoreIMFContextISF *context_scim;

    if (!ctx) return EINA_FALSE;
    context_scim = (EcoreIMFContextISF *)ecore_imf_context_data_get (ctx);
    if (!context_scim || !context_scim->impl) return EINA_FALSE;

    switch (context_scim->impl->autocapital_type) {
        case ECORE_IMF_AUTOCAPITAL_TYPE_NONE:
            return EINA_FALSE;
        case ECORE_IMF_AUTOCAPITAL_TYPE_ALLCHARACTER:
            return EINA_TRUE;
        default:
            break;
    }

    if (context_scim->impl->cursor_pos == 0)
        return EINA_TRUE;

    ecore_imf_context_surrounding_get (ctx, &markup_str, &cursor_pos);
    if (!markup_str) goto done;

    if (cursor_pos == 0) {
        ret = EINA_TRUE;
        goto done;
    }

    // Convert into plain string
    plain_str = evas_textblock_text_markup_to_utf8 (NULL, markup_str);
    if (!plain_str) goto done;

    // Convert string from UTF-8 to unicode
    ustr = eina_unicode_utf8_to_unicode (plain_str, NULL);
    if (!ustr) goto done;

    if (eina_unicode_strlen (ustr) < (size_t)cursor_pos) goto done;

    if (cursor_pos >= 1) {
        if (context_scim->impl->autocapital_type == ECORE_IMF_AUTOCAPITAL_TYPE_WORD) {
            // Check space or no-break space
            if (ustr[cursor_pos-1] == ' ' || ustr[cursor_pos-1] == 0x00A0) {
                ret = EINA_TRUE;
                goto done;
            }
        }

        // Check paragraph separator <PS> or carriage return  <br>
        if ((ustr[cursor_pos-1] == 0x2029) || (ustr[cursor_pos-1] == '\n')) {
            ret = EINA_TRUE;
            goto done;
        }

        for (i = cursor_pos; i > 0; i--) {
            // Check space or no-break space
            if (ustr[i-1] == ' ' || ustr[i-1] == 0x00A0) {
                detect_space = EINA_TRUE;
                continue;
            }

            for (j = 0; j < punc_num; j++) {
                // Check punctuation and following the continuous space(s)
                if ((ustr[i-1] == puncs[j]) && (detect_space == EINA_TRUE)) {
                    ret = EINA_TRUE;
                    goto done;
                }
            }

            if (j == punc_num) {
                // other character
                break;
            }
        }

        if ((i == 0) && (detect_space == EINA_TRUE)) {
            // continuous space(s) without any character
            ret = EINA_TRUE;
            goto done;
        }
    }

done:
    if (ustr) free (ustr);
    if (markup_str) free (markup_str);
    if (plain_str) free (plain_str);

    return ret;
}

EAPI Eina_Bool
caps_mode_check (Ecore_IMF_Context *ctx, Eina_Bool force, Eina_Bool noti)
{
    Eina_Bool uppercase;
    EcoreIMFContextISF *context_scim;

    if (!ctx) return EINA_FALSE;
    context_scim = (EcoreIMFContextISF *)ecore_imf_context_data_get (ctx);

    if (!context_scim || !context_scim->impl)
        return EINA_FALSE;

    if (context_scim->impl->next_shift_status == SHIFT_MODE_LOCK) return EINA_TRUE;

    Ecore_IMF_Input_Panel_Layout layout = ecore_imf_context_input_panel_layout_get (ctx);
    if (layout != ECORE_IMF_INPUT_PANEL_LAYOUT_NORMAL)
        return EINA_FALSE;

    // Check autocapital type
    if (ecore_imf_context_input_panel_caps_lock_mode_get (ctx)) {
        uppercase = EINA_TRUE;
    } else {
        if (autocap_allow == EINA_FALSE)
            return EINA_FALSE;

        if (analyze_surrounding_text (ctx)) {
            uppercase = EINA_TRUE;
        } else {
            uppercase = EINA_FALSE;
        }
    }

    if (force) {
        context_scim->impl->next_shift_status = uppercase ? SHIFT_MODE_ON : SHIFT_MODE_OFF;
        if (noti)
            isf_imf_context_input_panel_caps_mode_set (ctx, uppercase);
    } else {
        if (context_scim->impl->next_shift_status != (uppercase ? SHIFT_MODE_ON : SHIFT_MODE_OFF)) {
            context_scim->impl->next_shift_status = uppercase ? SHIFT_MODE_ON : SHIFT_MODE_OFF;
            if (noti)
                isf_imf_context_input_panel_caps_mode_set (ctx, uppercase);
        }
    }

    return uppercase;
}

static void
window_to_screen_geometry_get (Ecore_X_Window client_win, int *x, int *y)
{
    Ecore_X_Window root_window, win;
    int win_x, win_y;
    int sum_x = 0, sum_y = 0;

    root_window = ecore_x_window_root_get (client_win);
    win = client_win;

    while (root_window != win) {
        ecore_x_window_geometry_get (win, &win_x, &win_y, NULL, NULL);
        sum_x += win_x;
        sum_y += win_y;
        win = ecore_x_window_parent_get (win);
    }

    if (x)
        *x = sum_x;
    if (y)
        *y = sum_y;
}

static void
evas_focus_out_cb (void *data, Evas *e, void *event_info)
{
    Ecore_IMF_Context *ctx = (Ecore_IMF_Context *)data;

    if (!ctx) return;

    LOGD ("ctx : %p\n", ctx);

    if (input_panel_ctx == ctx && _scim_initialized) {
        isf_imf_context_input_panel_instant_hide (ctx);
    }
}

static void autoperiod_allow_changed_cb (keynode_t *key, void* data)
{
    SCIM_DEBUG_MAIN (3) << __FUNCTION__ << "...\n";

    autoperiod_allow = vconf_keynode_get_bool (key);
}

static void autocapital_allow_changed_cb (keynode_t *key, void* data)
{
    SCIM_DEBUG_MAIN (3) << __FUNCTION__ << "...\n";

    autocap_allow = vconf_keynode_get_bool (key);
}

EAPI void context_scim_imdata_get (Ecore_IMF_Context *ctx, void* data, int* length)
{
    EcoreIMFContextISF *context_scim = (EcoreIMFContextISF *)ecore_imf_context_data_get (ctx);

    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    if (data && context_scim->impl->imdata)
        memcpy (data, context_scim->impl->imdata, context_scim->impl->imdata_size);

    *length = context_scim->impl->imdata_size;
}

EAPI void
imengine_layout_set (Ecore_IMF_Context *ctx, Ecore_IMF_Input_Panel_Layout layout)
{
    EcoreIMFContextISF *context_scim = (EcoreIMFContextISF *)ecore_imf_context_data_get (ctx);

    if (context_scim && context_scim->impl) {
        _panel_client.prepare (context_scim->id);
        context_scim->impl->si->set_layout (layout);
        _panel_client.send ();
    }
}

/* Public functions */
/**
 * isf_imf_context_new
 *
 * This function will be called by Ecore IMF.
 * Create a instance of type EcoreIMFContextISF.
 *
 * Return value: A pointer to the newly created EcoreIMFContextISF instance
 */
EAPI EcoreIMFContextISF *
isf_imf_context_new (void)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    int val;

    EcoreIMFContextISF *context_scim = new EcoreIMFContextISF;
    if (context_scim == NULL) {
        std::cerr << "memory allocation failed in " << __FUNCTION__ << "\n";
        return NULL;
    }

    if (_context_count == 0) {
        _context_count = getpid () % 50000;
    }
    context_scim->id = _context_count++;

    if (!_scim_initialized) {
        ecore_x_init(NULL);
        initialize ();
        _scim_initialized = true;
        isf_imf_input_panel_init ();

        /* get autoperiod allow vconf value */
        if (vconf_get_bool (VCONFKEY_AUTOPERIOD_ALLOW_BOOL, &val) == 0) {
            if (val == EINA_TRUE)
                autoperiod_allow = EINA_TRUE;
        }

        vconf_notify_key_changed (VCONFKEY_AUTOPERIOD_ALLOW_BOOL, autoperiod_allow_changed_cb, NULL);

        /* get autocapital allow vconf value */
        if (vconf_get_bool (VCONFKEY_AUTOCAPITAL_ALLOW_BOOL, &val) == 0) {
            if (val == EINA_TRUE)
                autocap_allow = EINA_TRUE;
        }

        vconf_notify_key_changed (VCONFKEY_AUTOCAPITAL_ALLOW_BOOL, autocapital_allow_changed_cb, NULL);
    }

    return context_scim;
}

/**
 * isf_imf_context_shutdown
 *
 * It will be called when the scim im module is unloaded by ecore. It will do some
 * cleanup job.
 */
EAPI void
isf_imf_context_shutdown (void)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    if (_scim_initialized) {
        _scim_initialized = false;

        LOGD ("immodule shutdown\n");

        vconf_ignore_key_changed (VCONFKEY_AUTOPERIOD_ALLOW_BOOL, autoperiod_allow_changed_cb);
        vconf_ignore_key_changed (VCONFKEY_AUTOCAPITAL_ALLOW_BOOL, autocapital_allow_changed_cb);

        isf_imf_input_panel_shutdown ();
        finalize ();
        ecore_x_shutdown();
    }
}

EAPI void
isf_imf_context_add (Ecore_IMF_Context *ctx)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *context_scim = (EcoreIMFContextISF*)ecore_imf_context_data_get (ctx);

    if (!context_scim) return;

    context_scim->impl = NULL;

    if (_backend.null ())
        return;

    IMEngineInstancePointer si;

    // Use the default instance if "shared input method" mode is enabled.
    if (_shared_input_method && !_default_instance.null ()) {
        si = _default_instance;
        SCIM_DEBUG_FRONTEND(2) << "use default instance: " << si->get_id () << " " << si->get_factory_uuid () << "\n";
    }

    // Not in "shared input method" mode, or no default instance, create an instance.
    if (si.null ()) {
        IMEngineFactoryPointer factory = _backend->get_default_factory (_language, "UTF-8");
        if (factory.null ()) return;
        si = factory->create_instance ("UTF-8", _instance_count++);
        if (si.null ()) return;
        attach_instance (si);
        SCIM_DEBUG_FRONTEND(2) << "create new instance: " << si->get_id () << " " << si->get_factory_uuid () << "\n";
    }

    // If "shared input method" mode is enabled, and there is no default instance,
    // then store this instance as default one.
    if (_shared_input_method && _default_instance.null ()) {
        SCIM_DEBUG_FRONTEND(2) << "update default instance.\n";
        _default_instance = si;
    }

    context_scim->ctx                       = ctx;
    context_scim->impl                      = new_ic_impl (context_scim);
    if (context_scim->impl == NULL) {
        std::cerr << "memory allocation failed in " << __FUNCTION__ << "\n";
        return;
    }

    context_scim->impl->si                  = si;
    context_scim->impl->client_window       = 0;
    context_scim->impl->client_canvas       = NULL;
    context_scim->impl->preedit_caret       = 0;
    context_scim->impl->cursor_x            = 0;
    context_scim->impl->cursor_y            = 0;
    context_scim->impl->cursor_pos          = -1;
    context_scim->impl->cursor_top_y        = 0;
    context_scim->impl->is_on               = true;
    context_scim->impl->shared_si           = _shared_input_method;
    context_scim->impl->use_preedit         = _on_the_spot;
    context_scim->impl->preedit_started     = false;
    context_scim->impl->preedit_updating    = false;
    context_scim->impl->need_commit_preedit = false;
    context_scim->impl->prediction_allow    = true;

    if (!_ic_list)
        context_scim->next = NULL;
    else
        context_scim->next = _ic_list;
    _ic_list = context_scim;

    if (_shared_input_method)
        context_scim->impl->is_on = _config->read (String (SCIM_CONFIG_FRONTEND_IM_OPENED_BY_DEFAULT), context_scim->impl->is_on);

    _panel_client.prepare (context_scim->id);
    _panel_client.register_input_context (context_scim->id, si->get_factory_uuid ());
    set_ic_capabilities (context_scim);
    _panel_client.send ();

    SCIM_DEBUG_FRONTEND(2) << "input context created: id = " << context_scim->id << "\n";
}

EAPI void
isf_imf_context_del (Ecore_IMF_Context *ctx)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    if (!_ic_list) return;

    EcoreIMFContextISF *context_scim = (EcoreIMFContextISF*)ecore_imf_context_data_get (ctx);
    Ecore_IMF_Input_Panel_State input_panel_state = ecore_imf_context_input_panel_state_get (ctx);

    if (context_scim) {
        if (context_scim->id != _ic_list->id) {
            EcoreIMFContextISF * pre = _ic_list;
            EcoreIMFContextISF * cur = _ic_list->next;
            while (cur != NULL) {
                if (cur->id == context_scim->id) {
                    pre->next = cur->next;
                    break;
                }
                pre = cur;
                cur = cur->next;
            }
        } else {
            _ic_list = _ic_list->next;
        }
    }

    if (context_scim && context_scim->impl) {
        _panel_client.prepare (context_scim->id);

        if (context_scim == _focused_ic)
            context_scim->impl->si->focus_out ();

        if (context_scim->impl->client_canvas)
            evas_event_callback_del_full (context_scim->impl->client_canvas, EVAS_CALLBACK_CANVAS_FOCUS_OUT, evas_focus_out_cb, ctx);

        if (input_panel_ctx == ctx && _scim_initialized) {
            LOGD ("ctx : %p\n", ctx);
            if (input_panel_state == ECORE_IMF_INPUT_PANEL_STATE_WILL_SHOW ||
                input_panel_state == ECORE_IMF_INPUT_PANEL_STATE_SHOW) {
                ecore_imf_context_input_panel_hide (ctx);
                input_panel_event_callback_call (ECORE_IMF_INPUT_PANEL_STATE_EVENT, ECORE_IMF_INPUT_PANEL_STATE_HIDE);
                isf_imf_context_input_panel_send_will_hide_ack ();
            }
        }

        // Delete the instance.
        // FIXME:
        // In case the instance send out some helper event,
        // and this context has been focused out,
        // we need set the focused_ic to this context temporary.
        EcoreIMFContextISF *old_focused = _focused_ic;
        _focused_ic = context_scim;
        context_scim->impl->si.reset ();
        _focused_ic = old_focused;

        if (context_scim == _focused_ic) {
            _panel_client.turn_off (context_scim->id);
            _panel_client.focus_out (context_scim->id);
        }

        _panel_client.remove_input_context (context_scim->id);
        _panel_client.send ();

        if (context_scim->impl->client_window)
            isf_imf_context_client_window_set (ctx, NULL);

        if (context_scim->impl) {
            delete_ic_impl (context_scim->impl);
            context_scim->impl = 0;
        }
    }

    isf_imf_context_input_panel_event_callback_clear (ctx);

    if (context_scim == _focused_ic)
        _focused_ic = 0;

    if (context_scim) {
        delete context_scim;
        context_scim = 0;
    }
}

/**
 * isf_imf_context_client_canvas_set
 * @ctx: a #Ecore_IMF_Context
 * @canvas: the client canvas
 *
 * This function will be called by Ecore IMF.
 *
 * Set the client canvas for the Input Method Context; this is the canvas
 * in which the input appears.
 *
 * The canvas type can be determined by using the context canvas type.
 * Actually only canvas with type "evas" (Evas *) is supported. This canvas
 * may be used in order to correctly position status windows, and may also
 * be used for purposes internal to the Input Method Context.
 */
EAPI void
isf_imf_context_client_canvas_set (Ecore_IMF_Context *ctx, void *canvas)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *context_scim = (EcoreIMFContextISF *)ecore_imf_context_data_get (ctx);

    if (context_scim && context_scim->impl && context_scim->impl->client_canvas != (Evas*) canvas) {
        context_scim->impl->client_canvas = (Evas*)canvas;

        LOGD ("ctx : %p, canvas : %p\n", ctx, canvas);

        evas_event_callback_add (context_scim->impl->client_canvas, EVAS_CALLBACK_CANVAS_FOCUS_OUT, evas_focus_out_cb, ctx);
    }
}

/**
 * isf_imf_context_client_window_set
 * @ctx: a #Ecore_IMF_Context
 * @window: the client window
 *
 * This function will be called by Ecore IMF.
 *
 * Set the client window for the Input Method Context; this is the Ecore_X_Window
 * when using X11, Ecore_Win32_Window when using Win32, etc.
 *
 * This window is used in order to correctly position status windows,
 * and may also be used for purposes internal to the Input Method Context.
 */
EAPI void
isf_imf_context_client_window_set (Ecore_IMF_Context *ctx, void *window)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *context_scim = (EcoreIMFContextISF *)ecore_imf_context_data_get (ctx);

    if (context_scim && context_scim->impl && context_scim->impl->client_window != (Ecore_X_Window)((Ecore_Window)window)) {
        context_scim->impl->client_window = (Ecore_X_Window)((Ecore_Window)window);

        LOGD ("ctx : %p, client X win ID : %#x\n", ctx, context_scim->impl->client_window);

        if ((context_scim->impl->client_window != 0) &&
                (context_scim->impl->client_window != _client_window)) {
            _client_window = context_scim->impl->client_window;

            _check_desktop_mode (_client_window);

            ecore_event_handler_add (ECORE_X_EVENT_WINDOW_PROPERTY, _x_prop_change, window);
        }
    }
}

/**
 * isf_imf_context_focus_in
 * @ctx: a #Ecore_IMF_Context
 *
 * This function will be called by Ecore IMF.
 *
 * Notify the Input Method Context that the widget to which its correspond has gained focus.
 */
EAPI void
isf_imf_context_focus_in (Ecore_IMF_Context *ctx)
{
    EcoreIMFContextISF *context_scim = (EcoreIMFContextISF *)ecore_imf_context_data_get (ctx);

    if (!context_scim)
        return;

    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__<< "(" << context_scim->id << ")...\n";

    if (_focused_ic) {
        if (_focused_ic == context_scim) {
            SCIM_DEBUG_FRONTEND(1) << "It's already focused.\n";
            return;
        }
        SCIM_DEBUG_FRONTEND(1) << "Focus out previous IC first: " << _focused_ic->id << "\n";
        if (_focused_ic->ctx)
            isf_imf_context_focus_out (_focused_ic->ctx);
    }

    bool need_cap   = false;
    bool need_reset = false;
    bool need_reg   = false;

    if (context_scim && context_scim->impl) {
        _focused_ic = context_scim;
        isf_imf_context_control_focus_in (ctx);

        _panel_client.send ();

        _panel_client.prepare (context_scim->id);

        // Handle the "Shared Input Method" mode.
        if (_shared_input_method) {
            SCIM_DEBUG_FRONTEND(2) << "shared input method.\n";
            IMEngineFactoryPointer factory = _backend->get_default_factory (_language, "UTF-8");
            if (!factory.null ()) {
                if (_default_instance.null () || _default_instance->get_factory_uuid () != factory->get_uuid ()) {
                    _default_instance = factory->create_instance ("UTF-8", _default_instance.null () ? _instance_count++ : _default_instance->get_id ());
                    attach_instance (_default_instance);
                    SCIM_DEBUG_FRONTEND(2) << "create new default instance: " << _default_instance->get_id () << " " << _default_instance->get_factory_uuid () << "\n";
                }

                context_scim->impl->shared_si = true;
                context_scim->impl->si = _default_instance;

                context_scim->impl->is_on = _config->read (String (SCIM_CONFIG_FRONTEND_IM_OPENED_BY_DEFAULT), context_scim->impl->is_on);
                context_scim->impl->preedit_string.clear ();
                context_scim->impl->preedit_attrlist.clear ();
                context_scim->impl->preedit_caret = 0;
                context_scim->impl->preedit_started = false;
                need_cap = true;
                need_reset = true;
                need_reg = true;
            }
        } else if (context_scim->impl->shared_si) {
            SCIM_DEBUG_FRONTEND(2) << "exit shared input method.\n";
            IMEngineFactoryPointer factory = _backend->get_default_factory (_language, "UTF-8");
            if (!factory.null ()) {
                context_scim->impl->si = factory->create_instance ("UTF-8", _instance_count++);
                context_scim->impl->preedit_string.clear ();
                context_scim->impl->preedit_attrlist.clear ();
                context_scim->impl->preedit_caret = 0;
                context_scim->impl->preedit_started = false;
                attach_instance (context_scim->impl->si);
                need_cap = true;
                need_reg = true;
                context_scim->impl->shared_si = false;
                SCIM_DEBUG_FRONTEND(2) << "create new instance: " << context_scim->impl->si->get_id () << " " << context_scim->impl->si->get_factory_uuid () << "\n";
            }
        }

        context_scim->impl->si->set_frontend_data (static_cast <void*> (context_scim));

        if (need_reg) _panel_client.register_input_context (context_scim->id, context_scim->impl->si->get_factory_uuid ());
        if (need_cap) set_ic_capabilities (context_scim);

        panel_req_focus_in (context_scim);
//        panel_req_update_spot_location (context_scim);
//        panel_req_update_factory_info (context_scim);

        if (need_reset) context_scim->impl->si->reset ();
        if (context_scim->impl->is_on) {
            _panel_client.turn_on (context_scim->id);
//            _panel_client.hide_preedit_string (context_scim->id);
//            _panel_client.hide_aux_string (context_scim->id);
//            _panel_client.hide_lookup_table (context_scim->id);
            context_scim->impl->si->focus_in ();
            imengine_layout_set (ctx, ecore_imf_context_input_panel_layout_get (ctx));
            set_prediction_allow (ctx, context_scim->impl->prediction_allow);
            if (context_scim->impl->imdata)
                context_scim->impl->si->set_imdata ((const char *)context_scim->impl->imdata, context_scim->impl->imdata_size);
        } else {
            _panel_client.turn_off (context_scim->id);
        }

        _panel_client.send ();
        if (caps_mode_check (ctx, EINA_FALSE, EINA_TRUE) == EINA_FALSE) {
            context_scim->impl->next_shift_status = 0;
        }
    }

    LOGD ("ctx : %p\n", ctx);

    if (ecore_imf_context_input_panel_enabled_get (ctx))
        ecore_imf_context_input_panel_show (ctx);
    else
        LOGD ("ctx : %p input panel enable : FALSE\n", ctx);
}

/**
 * isf_imf_context_focus_out
 * @ctx: a #Ecore_IMF_Context
 *
 * This function will be called by Ecore IMF.
 *
 * Notify the Input Method Context that the widget to which its correspond has lost focus.
 */
EAPI void
isf_imf_context_focus_out (Ecore_IMF_Context *ctx)
{
    EcoreIMFContextISF *context_scim = (EcoreIMFContextISF *)ecore_imf_context_data_get (ctx);

    if (!context_scim) return;

    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "(" << context_scim->id << ")...\n";

    if (context_scim && context_scim->impl && context_scim == _focused_ic) {

        WideString wstr = context_scim->impl->preedit_string;

        LOGD ("ctx : %p\n", ctx);

        if (ecore_imf_context_input_panel_enabled_get (ctx))
            ecore_imf_context_input_panel_hide (ctx);

        if (context_scim->impl->need_commit_preedit) {
            panel_slot_hide_preedit_string (context_scim->id);

            if (wstr.length ()) {
                ecore_imf_context_commit_event_add (context_scim->ctx, utf8_wcstombs (wstr).c_str ());
                ecore_imf_context_event_callback_call (context_scim->ctx, ECORE_IMF_CALLBACK_COMMIT, (void *)utf8_wcstombs (wstr).c_str ());
            }
            _panel_client.prepare (context_scim->id);
            _panel_client.reset_input_context (context_scim->id);
            _panel_client.send ();
        }

        _panel_client.prepare (context_scim->id);

        context_scim->impl->si->focus_out ();
        context_scim->impl->si->reset ();

//          if (context_scim->impl->shared_si) context_scim->impl->si->reset ();

        _panel_client.focus_out (context_scim->id);
        _panel_client.send ();
        _focused_ic = 0;
    }
    _x_key_event_is_valid = false;
}

/**
 * isf_imf_context_reset
 * @ctx: a #Ecore_IMF_Context
 *
 * This function will be called by Ecore IMF.
 *
 * Notify the Input Method Context that a change such as a change in cursor
 * position has been made. This will typically cause the Input Method Context
 * to clear the preedit state.
 */
EAPI void
isf_imf_context_reset (Ecore_IMF_Context *ctx)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *context_scim = (EcoreIMFContextISF *)ecore_imf_context_data_get (ctx);

    if (context_scim && context_scim->impl && context_scim == _focused_ic) {
        WideString wstr = context_scim->impl->preedit_string;

        _panel_client.prepare (context_scim->id);
        context_scim->impl->si->reset ();
        _panel_client.reset_input_context (context_scim->id);
        _panel_client.send ();

        if (context_scim->impl->need_commit_preedit) {
            panel_slot_hide_preedit_string (context_scim->id);

            if (wstr.length ()) {
                ecore_imf_context_commit_event_add (context_scim->ctx, utf8_wcstombs (wstr).c_str ());
                ecore_imf_context_event_callback_call (context_scim->ctx, ECORE_IMF_CALLBACK_COMMIT, (void *)utf8_wcstombs (wstr).c_str ());
            }
        }
    }
}

/**
 * isf_imf_context_cursor_position_set
 * @ctx: a #Ecore_IMF_Context
 * @cursor_pos: New cursor position in characters.
 *
 * This function will be called by Ecore IMF.
 *
 * Notify the Input Method Context that a change in the cursor position has been made.
 */
EAPI void
isf_imf_context_cursor_position_set (Ecore_IMF_Context *ctx, int cursor_pos)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *context_scim = (EcoreIMFContextISF *)ecore_imf_context_data_get (ctx);

    if (context_scim && context_scim->impl && context_scim == _focused_ic) {
        if (context_scim->impl->cursor_pos != cursor_pos) {
            LOGD ("ctx : %p, cursor pos : %d\n", ctx, cursor_pos);
            context_scim->impl->cursor_pos = cursor_pos;

            caps_mode_check (ctx, EINA_FALSE, EINA_TRUE);

            if (context_scim->impl->preedit_updating)
                return;
            _panel_client.prepare (context_scim->id);
            context_scim->impl->si->update_cursor_position (cursor_pos);
            panel_req_update_cursor_position (context_scim, cursor_pos);
            _panel_client.send ();
        }
    }
}

/**
 * isf_imf_context_cursor_location_set
 * @ctx: a #Ecore_IMF_Context
 * @x: x position of New cursor.
 * @y: y position of New cursor.
 * @w: the width of New cursor.
 * @h: the height of New cursor.
 *
 * This function will be called by Ecore IMF.
 *
 * Notify the Input Method Context that a change in the cursor location has been made.
 */
EAPI void
isf_imf_context_cursor_location_set (Ecore_IMF_Context *ctx, int cx, int cy, int cw, int ch)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *context_scim = (EcoreIMFContextISF *)ecore_imf_context_data_get (ctx);
    Ecore_Evas *ee;
    int canvas_x, canvas_y;
    int new_cursor_x, new_cursor_y;

    if (cw == 0 && ch == 0)
        return;

    if (context_scim && context_scim->impl && context_scim == _focused_ic) {
        if (context_scim->impl->client_canvas) {
            ee = ecore_evas_ecore_evas_get (context_scim->impl->client_canvas);
            if (!ee) return;

            ecore_evas_geometry_get (ee, &canvas_x, &canvas_y, NULL, NULL);
        }
        else {
            if (context_scim->impl->client_window)
                window_to_screen_geometry_get (context_scim->impl->client_window, &canvas_x, &canvas_y);
            else
                return;
        }

        new_cursor_x = canvas_x + cx;
        new_cursor_y = canvas_y + cy + ch;

        // Don't update spot location while updating preedit string.
        if (context_scim->impl->preedit_updating && (context_scim->impl->cursor_y == new_cursor_y))
            return;

        if (context_scim->impl->cursor_x != new_cursor_x || context_scim->impl->cursor_y != new_cursor_y) {
            context_scim->impl->cursor_x     = new_cursor_x;
            context_scim->impl->cursor_y     = new_cursor_y;
            context_scim->impl->cursor_top_y = canvas_y + cy;
            _panel_client.prepare (context_scim->id);
            panel_req_update_spot_location (context_scim);
            _panel_client.send ();
            SCIM_DEBUG_FRONTEND(2) << "new cursor location = " << context_scim->impl->cursor_x << "," << context_scim->impl->cursor_y << "\n";
        }
    }
}

/**
 * isf_imf_context_input_mode_set
 * @ctx: a #Ecore_IMF_Context
 * @input_mode: the input mode
 *
 * This function will be called by Ecore IMF.
 *
 * To set the input mode of input method. The definition of Ecore_IMF_Input_Mode
 * is in Ecore_IMF.h.
 */
EAPI void
isf_imf_context_input_mode_set (Ecore_IMF_Context *ctx, Ecore_IMF_Input_Mode input_mode)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *context_scim = (EcoreIMFContextISF*)ecore_imf_context_data_get (ctx);
    if (context_scim && context_scim->impl) {
        context_scim->impl->input_mode = input_mode;
    }
}

/**
 * isf_imf_context_preedit_string_get
 * @ctx: a #Ecore_IMF_Context
 * @str: the preedit string
 * @cursor_pos: the cursor position
 *
 * This function will be called by Ecore IMF.
 *
 * To get the preedit string of the input method.
 */
EAPI void
isf_imf_context_preedit_string_get (Ecore_IMF_Context *ctx, char** str, int *cursor_pos)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *context_scim = (EcoreIMFContextISF*)ecore_imf_context_data_get (ctx);

    if (context_scim && context_scim->impl && context_scim->impl->is_on) {
        String mbs = utf8_wcstombs (context_scim->impl->preedit_string);

        if (str) {
            if (mbs.length ())
                *str = strdup (mbs.c_str ());
            else
                *str = strdup ("");
        }

        if (cursor_pos) {
            *cursor_pos = context_scim->impl->preedit_caret;
        }
    } else {
        if (str)
            *str = strdup ("");

        if (cursor_pos)
            *cursor_pos = 0;
    }
}

EAPI void
isf_imf_context_preedit_string_with_attributes_get (Ecore_IMF_Context *ctx, char** str, Eina_List **attrs, int *cursor_pos)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *context_scim = (EcoreIMFContextISF*)ecore_imf_context_data_get (ctx);

    if (context_scim && context_scim->impl && context_scim->impl->is_on) {
        String mbs = utf8_wcstombs (context_scim->impl->preedit_string);

        if (str) {
            if (mbs.length ())
                *str = strdup (mbs.c_str ());
            else
                *str = strdup ("");
        }

        if (cursor_pos) {
            *cursor_pos = context_scim->impl->preedit_caret;
        }

        if (attrs) {
            if (mbs.length ()) {
                int start_index, end_index;
                int wlen = context_scim->impl->preedit_string.length ();
                Ecore_IMF_Preedit_Attr *attr = NULL;
                AttributeList::const_iterator i;
                bool *attrs_flag = new bool [mbs.length ()];
                memset (attrs_flag, 0, mbs.length () * sizeof (bool));
                for (i = context_scim->impl->preedit_attrlist.begin ();
                    i != context_scim->impl->preedit_attrlist.end (); ++i) {
                    start_index = i->get_start ();
                    end_index = i->get_end ();
                    if (end_index <= wlen && start_index < end_index && i->get_type () != SCIM_ATTR_DECORATE_NONE) {
                        start_index = g_utf8_offset_to_pointer (mbs.c_str (), i->get_start ()) - mbs.c_str ();
                        end_index = g_utf8_offset_to_pointer (mbs.c_str (), i->get_end ()) - mbs.c_str ();
                        if (i->get_type () == SCIM_ATTR_DECORATE) {
                            attr = (Ecore_IMF_Preedit_Attr *)calloc(1, sizeof (Ecore_IMF_Preedit_Attr));
                            if (attr == NULL)
                                continue;
                            attr->start_index = start_index;
                            attr->end_index = end_index;

                            if (i->get_value () == SCIM_ATTR_DECORATE_UNDERLINE) {
                                attr->preedit_type = ECORE_IMF_PREEDIT_TYPE_SUB1;
                                *attrs = eina_list_append (*attrs, (void *)attr);
                            } else if (i->get_value () == SCIM_ATTR_DECORATE_REVERSE) {
                                attr->preedit_type = ECORE_IMF_PREEDIT_TYPE_SUB2;
                                *attrs = eina_list_append (*attrs, (void *)attr);
                            } else if (i->get_value () == SCIM_ATTR_DECORATE_HIGHLIGHT) {
                                attr->preedit_type = ECORE_IMF_PREEDIT_TYPE_SUB3;
                                *attrs = eina_list_append (*attrs, (void *)attr);
                            } else if (i->get_value () == SCIM_ATTR_DECORATE_BGCOLOR1) {
                                attr->preedit_type = ECORE_IMF_PREEDIT_TYPE_SUB4;
                                *attrs = eina_list_append (*attrs, (void *)attr);
                            } else if (i->get_value () == SCIM_ATTR_DECORATE_BGCOLOR2) {
                                attr->preedit_type = ECORE_IMF_PREEDIT_TYPE_SUB5;
                                *attrs = eina_list_append (*attrs, (void *)attr);
                            } else if (i->get_value () == SCIM_ATTR_DECORATE_BGCOLOR3) {
                                attr->preedit_type = ECORE_IMF_PREEDIT_TYPE_SUB6;
                                *attrs = eina_list_append (*attrs, (void *)attr);
                            } else if (i->get_value () == SCIM_ATTR_DECORATE_BGCOLOR4) {
                                attr->preedit_type = ECORE_IMF_PREEDIT_TYPE_SUB7;
                                *attrs = eina_list_append (*attrs, (void *)attr);
                            } else {
                                free (attr);
                            }
                            switch(i->get_value())
                            {
                                case SCIM_ATTR_DECORATE_UNDERLINE:
                                case SCIM_ATTR_DECORATE_REVERSE:
                                case SCIM_ATTR_DECORATE_HIGHLIGHT:
                                case SCIM_ATTR_DECORATE_BGCOLOR1:
                                case SCIM_ATTR_DECORATE_BGCOLOR2:
                                case SCIM_ATTR_DECORATE_BGCOLOR3:
                                case SCIM_ATTR_DECORATE_BGCOLOR4:
                                    // Record which character has attribute.
                                    for (int pos = start_index; pos < end_index; ++pos)
                                        attrs_flag [pos] = 1;
                                    break;
                                default:
                                    break;
                            }
                        } else if (i->get_type () == SCIM_ATTR_FOREGROUND) {
                            SCIM_DEBUG_FRONTEND(4) << "SCIM_ATTR_FOREGROUND\n";
                        } else if (i->get_type () == SCIM_ATTR_BACKGROUND) {
                            SCIM_DEBUG_FRONTEND(4) << "SCIM_ATTR_BACKGROUND\n";
                        }
                    }
                }
                // Add underline for all characters which don't have attribute.
                for (unsigned int pos = 0; pos < mbs.length (); ++pos) {
                    if (!attrs_flag [pos]) {
                        int begin_pos = pos;
                        while (pos < mbs.length () && !attrs_flag [pos])
                            ++pos;
                        // use REVERSE style as default
                        attr = (Ecore_IMF_Preedit_Attr *)calloc (1, sizeof (Ecore_IMF_Preedit_Attr));
                        if (attr == NULL)
                            continue;
                        attr->preedit_type = ECORE_IMF_PREEDIT_TYPE_SUB2;
                        attr->start_index = begin_pos;
                        attr->end_index = pos;
                        *attrs = eina_list_append(*attrs, (void *)attr);
                    }
                }
                delete [] attrs_flag;
            }
        }
    } else {
        if (str)
            *str = strdup ("");

        if (cursor_pos)
            *cursor_pos = 0;

        if (attrs)
            *attrs = NULL;
    }
}

/**
 * isf_imf_context_use_preedit_set
 * @ctx: a #Ecore_IMF_Context
 * @use_preedit: Whether the IM context should use the preedit string.
 *
 * This function will be called by Ecore IMF.
 *
 * Set whether the IM context should use the preedit string to display feedback.
 * If is 0 (default is 1), then the IM context may use some other method to
 * display feedback, such as displaying it in a child of the root window.
 */
EAPI void
isf_imf_context_use_preedit_set (Ecore_IMF_Context* ctx, Eina_Bool use_preedit)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " = " << (use_preedit == EINA_TRUE ? "true" : "false") << "...\n";

    EcoreIMFContextISF *context_scim = (EcoreIMFContextISF*)ecore_imf_context_data_get (ctx);

    if (!_on_the_spot) return;

    if (context_scim && context_scim->impl) {
        bool old = context_scim->impl->use_preedit;
        context_scim->impl->use_preedit = use_preedit;
        if (context_scim == _focused_ic) {
            _panel_client.prepare (context_scim->id);

            if (old != use_preedit)
                set_ic_capabilities (context_scim);

            if (context_scim->impl->preedit_string.length ())
                slot_show_preedit_string (context_scim->impl->si);

            _panel_client.send ();
        }
    }
}

/**
 * isf_imf_context_prediction_allow_set
 * @ctx: a #Ecore_IMF_Context
 * @prediction: Whether the IM context should use the prediction.
 *
 * This function will be called by Ecore IMF.
 *
 * Set whether the IM context should use the prediction.
 */
EAPI void
isf_imf_context_prediction_allow_set (Ecore_IMF_Context* ctx, Eina_Bool prediction)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " = " << (prediction == EINA_TRUE ? "true" : "false") << "...\n";

    EcoreIMFContextISF *context_scim = (EcoreIMFContextISF *)ecore_imf_context_data_get (ctx);

    if (context_scim && context_scim->impl && context_scim->impl->prediction_allow != prediction) {
        context_scim->impl->prediction_allow = prediction;
        set_prediction_allow (ctx, prediction);
    }
}

/**
 * isf_imf_context_prediction_allow_get
 * @ctx: a #Ecore_IMF_Context
 *
 * This function will be called by Ecore IMF.
 *
 * To get prediction allow flag for the IM context.
 *
 * Return value: the prediction allow flag for the IM context
 */
EAPI Eina_Bool
isf_imf_context_prediction_allow_get (Ecore_IMF_Context* ctx)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *context_scim = (EcoreIMFContextISF *)ecore_imf_context_data_get (ctx);

    Eina_Bool ret = EINA_FALSE;
    if (context_scim && context_scim->impl) {
        ret = context_scim->impl->prediction_allow;
    } else {
        std::cerr << __FUNCTION__ << " failed!!!\n";
    }
    return ret;
}

/**
 * isf_imf_context_autocapital_type_set
 * @ctx: a #Ecore_IMF_Context
 * @autocapital_type: the autocapital type for the IM context.
 *
 * This function will be called by Ecore IMF.
 *
 * Set autocapital type for the IM context.
 */
EAPI void
isf_imf_context_autocapital_type_set (Ecore_IMF_Context* ctx, Ecore_IMF_Autocapital_Type autocapital_type)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " = " << autocapital_type << "...\n";

    EcoreIMFContextISF *context_scim = (EcoreIMFContextISF *)ecore_imf_context_data_get (ctx);

    if (context_scim && context_scim->impl && context_scim->impl->autocapital_type != autocapital_type) {
        context_scim->impl->autocapital_type = autocapital_type;
    }
}

/**
 * isf_imf_context_filter_event
 * @ctx: a #Ecore_IMF_Context
 * @type: The type of event defined by Ecore_IMF_Event_Type.
 * @event: The event itself.
 * Return value: %TRUE if the input method handled the key event.
 *
 * This function will be called by Ecore IMF.
 *
 * Allow an Ecore Input Context to internally handle an event. If this function
 * returns 1, then no further processing should be done for this event. Input
 * methods must be able to accept all types of events (simply returning 0 if
 * the event was not handled), but there is no obligation of any events to be
 * submitted to this function.
 */
EAPI Eina_Bool
isf_imf_context_filter_event (Ecore_IMF_Context *ctx, Ecore_IMF_Event_Type type, Ecore_IMF_Event *event)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *ic = (EcoreIMFContextISF*)ecore_imf_context_data_get (ctx);
    Eina_Bool ret = EINA_FALSE;

    if (ic == NULL || ic->impl == NULL)
        return ret;

    if (type == ECORE_IMF_EVENT_KEY_DOWN || type == ECORE_IMF_EVENT_KEY_UP) {
        if (hw_keyboard_num_get () == 0 && !_x_key_event_is_valid) {
            std::cerr << "    Hardware keyboard is not connected and key event is not valid!!!\n";
            return EINA_TRUE;
        }
    }

    KeyEvent key;
    unsigned int timestamp;

    if (type == ECORE_IMF_EVENT_KEY_DOWN) {
        Ecore_IMF_Event_Key_Down *ev = (Ecore_IMF_Event_Key_Down *)event;
        timestamp = ev->timestamp;
        scim_string_to_key (key, ev->key);
        if (ev->modifiers & ECORE_IMF_KEYBOARD_MODIFIER_SHIFT) key.mask |=SCIM_KEY_ShiftMask;
        if (ev->modifiers & ECORE_IMF_KEYBOARD_MODIFIER_CTRL) key.mask |=SCIM_KEY_ControlMask;
        if (ev->modifiers & ECORE_IMF_KEYBOARD_MODIFIER_ALT) key.mask |=SCIM_KEY_AltMask;
        if (ev->modifiers & ECORE_IMF_KEYBOARD_MODIFIER_ALTGR) key.mask |=SCIM_KEY_Mod5Mask;
        if (ev->locks & ECORE_IMF_KEYBOARD_LOCK_CAPS) key.mask |=SCIM_KEY_CapsLockMask;
        if (ev->locks & ECORE_IMF_KEYBOARD_LOCK_NUM) key.mask |=SCIM_KEY_NumLockMask;
    } else if (type == ECORE_IMF_EVENT_KEY_UP) {
        Ecore_IMF_Event_Key_Up *ev = (Ecore_IMF_Event_Key_Up *)event;
        timestamp = ev->timestamp;
        scim_string_to_key (key, ev->key);
        key.mask = SCIM_KEY_ReleaseMask;
        if (ev->modifiers & ECORE_IMF_KEYBOARD_MODIFIER_SHIFT) key.mask |=SCIM_KEY_ShiftMask;
        if (ev->modifiers & ECORE_IMF_KEYBOARD_MODIFIER_CTRL) key.mask |=SCIM_KEY_ControlMask;
        if (ev->modifiers & ECORE_IMF_KEYBOARD_MODIFIER_ALT) key.mask |=SCIM_KEY_AltMask;
        if (ev->modifiers & ECORE_IMF_KEYBOARD_MODIFIER_ALTGR) key.mask |=SCIM_KEY_Mod5Mask;
        if (ev->locks & ECORE_IMF_KEYBOARD_LOCK_CAPS) key.mask |=SCIM_KEY_CapsLockMask;
        if (ev->locks & ECORE_IMF_KEYBOARD_LOCK_NUM) key.mask |=SCIM_KEY_NumLockMask;
    } else if (type == ECORE_IMF_EVENT_MOUSE_UP) {
        if (ecore_imf_context_input_panel_enabled_get (ctx)) {
            LOGD ("[Mouse-up event] ctx : %p\n", ctx);
            if (ic == _focused_ic)
                ecore_imf_context_input_panel_show (ctx);
            else
                LOGW ("Can't show IME because there is no focus. ctx : %p\n", ctx);
        }
        return EINA_FALSE;
    } else {
        return ret;
    }

    key.mask &= _valid_key_mask;

    _panel_client.prepare (ic->id);

    ret = EINA_TRUE;
    if (!filter_hotkeys (ic, key)) {
        if (timestamp == 0) {
            ret = EINA_FALSE;
            // in case of generated event
            if (type == ECORE_IMF_EVENT_KEY_DOWN) {
                char code = key.get_ascii_code ();
                if (isgraph (code)) {
                    char string[2] = {0};
                    snprintf (string, sizeof (string), "%c", code);

                    if (strlen (string) != 0) {
                        ecore_imf_context_commit_event_add (ic->ctx, string);
                        ecore_imf_context_event_callback_call (ic->ctx, ECORE_IMF_CALLBACK_COMMIT, (void *)string);
                        caps_mode_check (ctx, EINA_FALSE, EINA_TRUE);
                        ret = EINA_TRUE;
                    }
                }
                else {
                    if (key.code == SCIM_KEY_space ||
                        key.code == SCIM_KEY_KP_Space)
                        autoperiod_insert (ctx);
                }
            }
            _panel_client.send ();
            return ret;
        }

        if (!_focused_ic || !_focused_ic->impl->is_on ||
            !_focused_ic->impl->si->process_key_event (key)) {
            ret = EINA_FALSE;
        }
    }

    _panel_client.send ();

    return ret;
}

/**
 * Set up an ISE specific data
 *
 * @param[in] ctx a #Ecore_IMF_Context
 * @param[in] data pointer of data to sets up to ISE
 * @param[in] length length of data
 */
EAPI void isf_imf_context_imdata_set (Ecore_IMF_Context *ctx, const void* data, int length)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " data length ( " << length << ") ...\n";
    EcoreIMFContextISF *context_scim = (EcoreIMFContextISF *)ecore_imf_context_data_get (ctx);

    if (context_scim == NULL || data == NULL || length <= 0)
        return;

    if (context_scim && context_scim->impl) {
        if (context_scim->impl->imdata)
            free (context_scim->impl->imdata);

        context_scim->impl->imdata = calloc (1, length);
        memcpy (context_scim->impl->imdata, data, length);
        context_scim->impl->imdata_size = length;

        if (context_scim->impl->si && _focused_ic == context_scim) {
            _panel_client.prepare (context_scim->id);
            context_scim->impl->si->set_imdata ((const char *)data, length);
            _panel_client.send ();
        }
    }

    isf_imf_context_input_panel_imdata_set (ctx, data, length);
}

/**
 * Get the ISE specific data from ISE
 *
 * @param[in] ctx a #Ecore_IMF_Context
 * @param[out] data pointer of data to return
 * @param[out] length length of data
 */
EAPI void isf_imf_context_imdata_get (Ecore_IMF_Context *ctx, void* data, int* length)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    isf_imf_context_input_panel_imdata_get (ctx, data, length);
}

/* Panel Slot functions */
static void
panel_slot_reload_config (int context)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    _config->reload ();
}

static void
panel_slot_exit (int /* context */)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    finalize ();
}

static void
panel_slot_update_candidate_item_layout (int context, const std::vector<uint32> &row_items)
{
    EcoreIMFContextISF *ic = find_ic (context);
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " context=" << context << " row size=" << row_items.size () << " ic=" << ic << "\n";
    if (ic && ic->impl) {
        _panel_client.prepare (ic->id);
        ic->impl->si->update_candidate_item_layout (row_items);
        _panel_client.send ();
    }
}

static void
panel_slot_update_lookup_table_page_size (int context, int page_size)
{
    EcoreIMFContextISF *ic = find_ic (context);
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " context=" << context << " page_size=" << page_size << " ic=" << ic << "\n";
    if (ic && ic->impl) {
        _panel_client.prepare (ic->id);
        ic->impl->si->update_lookup_table_page_size (page_size);
        _panel_client.send ();
    }
}

static void
panel_slot_lookup_table_page_up (int context)
{
    EcoreIMFContextISF *ic = find_ic (context);
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " context=" << context << " ic=" << ic << "\n";
    if (ic && ic->impl) {
        _panel_client.prepare (ic->id);
        ic->impl->si->lookup_table_page_up ();
        _panel_client.send ();
    }
}

static void
panel_slot_lookup_table_page_down (int context)
{
    EcoreIMFContextISF *ic = find_ic (context);
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " context=" << context << " ic=" << ic << "\n";
    if (ic && ic->impl) {
        _panel_client.prepare (ic->id);
        ic->impl->si->lookup_table_page_down ();
        _panel_client.send ();
    }
}

static void
panel_slot_trigger_property (int context, const String &property)
{
    EcoreIMFContextISF *ic = find_ic (context);
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " context=" << context << " property=" << property << " ic=" << ic << "\n";
    if (ic && ic->impl) {
        _panel_client.prepare (ic->id);
        ic->impl->si->trigger_property (property);
        _panel_client.send ();
    }
}

static void
panel_slot_process_helper_event (int context, const String &target_uuid, const String &helper_uuid, const Transaction &trans)
{
    EcoreIMFContextISF *ic = find_ic (context);
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " context=" << context << " target=" << target_uuid
                           << " helper=" << helper_uuid << " ic=" << ic << " ic->impl=" << (ic != NULL ? ic->impl : 0) << " ic-uuid="
                           << ((ic && ic->impl) ? ic->impl->si->get_factory_uuid () : "" ) << "\n";
    if (ic && ic->impl && ic->impl->si->get_factory_uuid () == target_uuid) {
        _panel_client.prepare (ic->id);
        SCIM_DEBUG_FRONTEND(2) << "call process_helper_event\n";
        ic->impl->si->process_helper_event (helper_uuid, trans);
        _panel_client.send ();
    }
}

static void
panel_slot_move_preedit_caret (int context, int caret_pos)
{
    EcoreIMFContextISF *ic = find_ic (context);
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " context=" << context << " caret=" << caret_pos << " ic=" << ic << "\n";
    if (ic && ic->impl) {
        _panel_client.prepare (ic->id);
        ic->impl->si->move_preedit_caret (caret_pos);
        _panel_client.send ();
    }
}

static void
panel_slot_update_preedit_caret (int context, int caret)
{
    EcoreIMFContextISF *ic = find_ic (context);
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " context=" << context << " caret=" << caret << " ic=" << ic << "\n";

    if (ic && ic->impl && _focused_ic == ic && ic->impl->preedit_caret != caret) {
        ic->impl->preedit_caret = caret;
        if (ic->impl->use_preedit) {
            if (!ic->impl->preedit_started) {
                ecore_imf_context_preedit_start_event_add (ic->ctx);
                ecore_imf_context_event_callback_call (ic->ctx, ECORE_IMF_CALLBACK_PREEDIT_START, NULL);
                ic->impl->preedit_started = true;
            }
            ecore_imf_context_preedit_changed_event_add (ic->ctx);
            ecore_imf_context_event_callback_call (ic->ctx, ECORE_IMF_CALLBACK_PREEDIT_CHANGED, NULL);
        } else {
            _panel_client.prepare (ic->id);
            _panel_client.update_preedit_caret (ic->id, caret);
            _panel_client.send ();
        }
    }
}

static void
panel_slot_select_aux (int context, int aux_index)
{
    EcoreIMFContextISF *ic = find_ic (context);
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " context=" << context << " aux=" << aux_index << " ic=" << ic << "\n";
    if (ic && ic->impl) {
        _panel_client.prepare (ic->id);
        ic->impl->si->select_aux (aux_index);
        _panel_client.send ();
    }
}

static void
panel_slot_select_candidate (int context, int cand_index)
{
    EcoreIMFContextISF *ic = find_ic (context);
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " context=" << context << " candidate=" << cand_index << " ic=" << ic << "\n";
    if (ic && ic->impl) {
        _panel_client.prepare (ic->id);
        ic->impl->si->select_candidate (cand_index);
        _panel_client.send ();
    }
}

static int
_keyname_to_keycode (const char *keyname)
{
    int keycode = 0;
    int keysym;
    Display *display = (Display *)ecore_x_display_get ();

    keysym = XStringToKeysym (keyname);

    if (!strncmp (keyname, "Keycode-", 8)) {
        keycode = atoi (keyname + 8);
    } else {
        keycode = XKeysymToKeycode (display, keysym);
    }

    return keycode;
}

static Eina_Bool
feed_key_event (EcoreIMFContextISF *ic, const KeyEvent &key, bool fake)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    if (key.code <= 0x7F ||
        (key.code >= SCIM_KEY_BackSpace && key.code <= SCIM_KEY_Delete) ||
        (key.code >= SCIM_KEY_Home && key.code <= SCIM_KEY_Hyper_R)) {
        // ascii code and function keys
        send_x_key_event (key, fake);
        return EINA_TRUE;
    } else {
        return EINA_FALSE;
    }
}

static void
panel_slot_process_key_event (int context, const KeyEvent &key)
{
    EcoreIMFContextISF *ic = find_ic (context);
    Eina_Bool process_key = EINA_TRUE;
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " context=" << context << " key=" << key.get_key_string () << " ic=" << ic << "\n";

    if (!(ic && ic->impl))
        return;

    if ((_focused_ic != NULL) && (_focused_ic != ic))
        return;

    KeyEvent _key = key;
    if (key.is_key_press () &&
        ecore_imf_context_input_panel_layout_get (ic->ctx) == ECORE_IMF_INPUT_PANEL_LAYOUT_NORMAL) {
        if (key.code == SHIFT_MODE_OFF ||
            key.code == SHIFT_MODE_ON ||
            key.code == SHIFT_MODE_LOCK) {
            ic->impl->next_shift_status = _key.code;
        } else if (key.code == SHIFT_MODE_ENABLE ) {
            ic->impl->shift_mode_enabled = true;
            caps_mode_check (ic->ctx, EINA_TRUE, EINA_TRUE);
        } else if (key.code == SHIFT_MODE_DISABLE ) {
            ic->impl->shift_mode_enabled = false;
        }
    }

    if (key.code != SHIFT_MODE_OFF &&
        key.code != SHIFT_MODE_ON &&
        key.code != SHIFT_MODE_LOCK &&
        key.code != SHIFT_MODE_ENABLE &&
        key.code != SHIFT_MODE_DISABLE) {
        if (feed_key_event (ic, _key, false)) return;
    }

    if (key.code == SHIFT_MODE_ENABLE ||
        key.code == SHIFT_MODE_DISABLE) {
        process_key = EINA_FALSE;
    }

    _panel_client.prepare (ic->id);

    if (!filter_hotkeys (ic, _key)) {
        if (process_key) {
            if (!_focused_ic || !_focused_ic->impl->is_on ||
                    !_focused_ic->impl->si->process_key_event (_key)) {
                _fallback_instance->process_key_event (_key);
            }
        }
    }

    _panel_client.send ();
}

static void
panel_slot_commit_string (int context, const WideString &wstr)
{
    EcoreIMFContextISF *ic = find_ic (context);
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " context=" << context << " str=" << utf8_wcstombs (wstr) << " ic=" << ic << "\n";

    if (ic && ic->impl) {
        if (_focused_ic != ic)
            return;

        if (ic->impl->need_commit_preedit)
            panel_slot_hide_preedit_string (ic->id);
        ecore_imf_context_commit_event_add (ic->ctx, utf8_wcstombs (wstr).c_str ());
        ecore_imf_context_event_callback_call (ic->ctx, ECORE_IMF_CALLBACK_COMMIT, (void *)utf8_wcstombs (wstr).c_str ());
    }
}

static void
panel_slot_forward_key_event (int context, const KeyEvent &key)
{
    EcoreIMFContextISF *ic = find_ic (context);
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " context=" << context << " key=" << key.get_key_string () << " ic=" << ic << "\n";

    if (!(ic && ic->impl))
        return;

    if ((_focused_ic != NULL) && (_focused_ic != ic))
        return;

    if (strlen (key.get_key_string ().c_str ()) >= 116)
        return;

    feed_key_event (ic, key, true);
}

static void
panel_slot_request_help (int context)
{
    EcoreIMFContextISF *ic = find_ic (context);
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " context=" << context << " ic=" << ic << "\n";
    if (ic && ic->impl) {
        _panel_client.prepare (ic->id);
        panel_req_show_help (ic);
        _panel_client.send ();
    }
}

static void
panel_slot_request_factory_menu (int context)
{
    EcoreIMFContextISF *ic = find_ic (context);
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " context=" << context << " ic=" << ic << "\n";
    if (ic && ic->impl) {
        _panel_client.prepare (ic->id);
        panel_req_show_factory_menu (ic);
        _panel_client.send ();
    }
}

static void
panel_slot_change_factory (int context, const String &uuid)
{
    EcoreIMFContextISF *ic = find_ic (context);
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " context=" << context << " factory=" << uuid << " ic=" << ic << "\n";
    if (ic && ic->impl) {
        _panel_client.prepare (ic->id);
        ic->impl->si->reset ();
        open_specific_factory (ic, uuid);
        _panel_client.send ();
    }
}

static void
panel_slot_reset_keyboard_ise (int context)
{
    EcoreIMFContextISF *ic = find_ic (context);
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " context=" << context << " ic=" << ic << "\n";
    if (ic && ic->impl) {
        WideString wstr = ic->impl->preedit_string;
        if (ic->impl->need_commit_preedit) {
            panel_slot_hide_preedit_string (ic->id);

            if (wstr.length ()) {
                ecore_imf_context_commit_event_add (ic->ctx, utf8_wcstombs (wstr).c_str ());
                ecore_imf_context_event_callback_call (ic->ctx, ECORE_IMF_CALLBACK_COMMIT, (void *)utf8_wcstombs (wstr).c_str ());
            }
        }
        _panel_client.prepare (ic->id);
        ic->impl->si->reset ();
        _panel_client.send ();
    }
}

static void
panel_slot_update_keyboard_ise (int context)
{
    EcoreIMFContextISF *ic = find_ic (context);
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " context=" << context << " ic=" << ic << "\n";

    _backend->add_module (_config, "socket", false);
}

static void
panel_slot_show_preedit_string (int context)
{
    EcoreIMFContextISF *ic = find_ic (context);
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " context=" << context << "\n";

    if (ic && ic->impl && _focused_ic == ic) {
        if (!ic->impl->is_on)
            ic->impl->is_on = true;

        if (ic->impl->use_preedit) {
            if (!ic->impl->preedit_started) {
                ecore_imf_context_preedit_start_event_add (_focused_ic->ctx);
                ecore_imf_context_event_callback_call (_focused_ic->ctx, ECORE_IMF_CALLBACK_PREEDIT_START, NULL);
                ic->impl->preedit_started     = true;
                ic->impl->need_commit_preedit = true;
            }
        } else {
            _panel_client.prepare (ic->id);
            _panel_client.show_preedit_string (ic->id);
            _panel_client.send ();
        }
    }
}

static void
panel_slot_hide_preedit_string (int context)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *ic = find_ic (context);

    if (ic && ic->impl && _focused_ic == ic) {
        if (!ic->impl->is_on)
            ic->impl->is_on = true;

        bool emit = false;
        if (ic->impl->preedit_string.length ()) {
            ic->impl->preedit_string = WideString ();
            ic->impl->preedit_caret  = 0;
            ic->impl->preedit_attrlist.clear ();
            emit = true;
        }
        if (ic->impl->use_preedit) {
            if (emit) {
                ecore_imf_context_preedit_changed_event_add (ic->ctx);
                ecore_imf_context_event_callback_call (ic->ctx, ECORE_IMF_CALLBACK_PREEDIT_CHANGED, NULL);
            }
            if (ic->impl->preedit_started) {
                ecore_imf_context_preedit_end_event_add (ic->ctx);
                ecore_imf_context_event_callback_call (ic->ctx, ECORE_IMF_CALLBACK_PREEDIT_END, NULL);
                ic->impl->preedit_started     = false;
                ic->impl->need_commit_preedit = false;
            }
        } else {
            _panel_client.prepare (ic->id);
            _panel_client.hide_preedit_string (ic->id);
            _panel_client.send ();
        }
    }
}

static void
panel_slot_update_preedit_string (int context,
                                  const WideString    &str,
                                  const AttributeList &attrs)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *ic = find_ic (context);

    if (ic && ic->impl && _focused_ic == ic) {
        if (!ic->impl->is_on)
            ic->impl->is_on = true;

        if (ic->impl->preedit_string != str || str.length ()) {
            ic->impl->preedit_string   = str;
            ic->impl->preedit_attrlist = attrs;

            if (ic->impl->use_preedit) {
                if (!ic->impl->preedit_started) {
                    ecore_imf_context_preedit_start_event_add (ic->ctx);
                    ecore_imf_context_event_callback_call (ic->ctx, ECORE_IMF_CALLBACK_PREEDIT_START, NULL);
                    ic->impl->preedit_started = true;
                    ic->impl->need_commit_preedit = true;
                }
                ic->impl->preedit_caret    = str.length ();
                ic->impl->preedit_updating = true;
                ecore_imf_context_preedit_changed_event_add (ic->ctx);
                ecore_imf_context_event_callback_call (ic->ctx, ECORE_IMF_CALLBACK_PREEDIT_CHANGED, NULL);
                ic->impl->preedit_updating = false;
            } else {
                _panel_client.prepare (ic->id);
                _panel_client.update_preedit_string (ic->id, str, attrs);
                _panel_client.send ();
            }
        }
    }
}

static void
panel_slot_get_surrounding_text (int context, int maxlen_before, int maxlen_after)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *ic = find_ic (context);

    if (ic && ic->impl && _focused_ic == ic && ic->impl->si) {
        int cursor = 0;
        WideString text = WideString ();
        slot_get_surrounding_text (ic->impl->si, text, cursor, maxlen_before, maxlen_after);
        _panel_client.prepare (ic->id);
        _panel_client.update_surrounding_text (ic->id, text, cursor);
        _panel_client.send ();
    }
}

static void
panel_slot_delete_surrounding_text (int context, int offset, int len)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *ic = find_ic (context);

    if (ic && ic->impl && _focused_ic == ic && ic->impl->si)
        slot_delete_surrounding_text (ic->impl->si, offset, len);
}

static void
panel_slot_update_displayed_candidate_number (int context, int number)
{
    EcoreIMFContextISF *ic = find_ic (context);
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " context=" << context << " number=" << number << " ic=" << ic << "\n";
    if (ic && ic->impl && _focused_ic == ic && ic->impl->si) {
        _panel_client.prepare (ic->id);
        ic->impl->si->update_displayed_candidate_number (number);
        _panel_client.send ();
    }
}

static void
panel_slot_candidate_more_window_show (int context)
{
    EcoreIMFContextISF *ic = find_ic (context);
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " context=" << context << " ic=" << ic << "\n";
    if (ic && ic->impl && _focused_ic == ic && ic->impl->si) {
        _panel_client.prepare (ic->id);
        ic->impl->si->candidate_more_window_show ();
        _panel_client.send ();
    }
}

static void
panel_slot_candidate_more_window_hide (int context)
{
    EcoreIMFContextISF *ic = find_ic (context);
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " context=" << context << " ic=" << ic << "\n";
    if (ic && ic->impl && _focused_ic == ic && ic->impl->si) {
        _panel_client.prepare (ic->id);
        ic->impl->si->candidate_more_window_hide ();
        _panel_client.send ();
    }
}

static void
panel_slot_longpress_candidate (int context, int index)
{
    EcoreIMFContextISF *ic = find_ic (context);
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " context=" << context << " index=" << index << " ic=" << ic << "\n";
    if (ic && ic->impl && _focused_ic == ic && ic->impl->si) {
        _panel_client.prepare (ic->id);
        ic->impl->si->longpress_candidate (index);
        _panel_client.send ();
    }
}

static void
panel_slot_update_client_id (int context, int client_id)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " context=" << context << " client_id=" << client_id << "\n";

    _panel_client_id = client_id;
}

/* Panel Requestion functions. */
static void
panel_req_show_help (EcoreIMFContextISF *ic)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    String help;

    help =  String (_("Smart Common Input Method platform ")) +
            String (SCIM_VERSION) +
            String (_("\n(C) 2002-2005 James Su <suzhe@tsinghua.org.cn>\n\n"));

    if (ic && ic->impl) {
        IMEngineFactoryPointer sf = _backend->get_factory (ic->impl->si->get_factory_uuid ());
        if (sf) {
            help += utf8_wcstombs (sf->get_name ());
            help += String (_(":\n\n"));

            help += utf8_wcstombs (sf->get_help ());
            help += String (_("\n\n"));

            help += utf8_wcstombs (sf->get_credits ());
        }
        _panel_client.show_help (ic->id, help);
    }
}

static void
panel_req_show_factory_menu (EcoreIMFContextISF *ic)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    std::vector<IMEngineFactoryPointer> factories;
    std::vector <PanelFactoryInfo> menu;

    _backend->get_factories_for_encoding (factories, "UTF-8");

    for (size_t i = 0; i < factories.size (); ++ i) {
        menu.push_back (PanelFactoryInfo (
                            factories [i]->get_uuid (),
                            utf8_wcstombs (factories [i]->get_name ()),
                            factories [i]->get_language (),
                            factories [i]->get_icon_file ()));
    }

    if (menu.size ())
        _panel_client.show_factory_menu (ic->id, menu);
}

static void
panel_req_update_factory_info (EcoreIMFContextISF *ic)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    if (ic && ic->impl && ic == _focused_ic) {
        PanelFactoryInfo info;
        if (ic->impl->is_on) {
            IMEngineFactoryPointer sf = _backend->get_factory (ic->impl->si->get_factory_uuid ());
            if (sf)
                info = PanelFactoryInfo (sf->get_uuid (), utf8_wcstombs (sf->get_name ()), sf->get_language (), sf->get_icon_file ());
        } else {
            info = PanelFactoryInfo (String (""), String (_("English/Keyboard")), String ("C"), String (SCIM_KEYBOARD_ICON_FILE));
        }
        _panel_client.update_factory_info (ic->id, info);
    }
}

static void
panel_req_focus_in (EcoreIMFContextISF *ic)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    _panel_client.focus_in (ic->id, ic->impl->si->get_factory_uuid ());
}

static void
panel_req_update_spot_location (EcoreIMFContextISF *ic)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    _panel_client.update_spot_location (ic->id, ic->impl->cursor_x, ic->impl->cursor_y, ic->impl->cursor_top_y);
}

static void
panel_req_update_cursor_position (EcoreIMFContextISF *ic, int cursor_pos)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    _panel_client.update_cursor_position (ic->id, cursor_pos);
}

static bool
filter_hotkeys (EcoreIMFContextISF *ic, const KeyEvent &key)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    bool ret = false;

    _frontend_hotkey_matcher.push_key_event (key);
    _imengine_hotkey_matcher.push_key_event (key);

    FrontEndHotkeyAction hotkey_action = _frontend_hotkey_matcher.get_match_result ();

    if (hotkey_action == SCIM_FRONTEND_HOTKEY_TRIGGER) {
        if (!ic->impl->is_on)
            turn_on_ic (ic);
        else
            turn_off_ic (ic);

        _display_input_language (ic);
        ret = true;
    } else if (hotkey_action == SCIM_FRONTEND_HOTKEY_ON) {
        if (!ic->impl->is_on) {
            turn_on_ic (ic);
            _display_input_language (ic);
        }
        ret = true;
    } else if (hotkey_action == SCIM_FRONTEND_HOTKEY_OFF) {
        if (ic->impl->is_on) {
            turn_off_ic (ic);
            _display_input_language (ic);
        }
        ret = true;
    } else if (hotkey_action == SCIM_FRONTEND_HOTKEY_NEXT_FACTORY) {
        open_next_factory (ic);
        ret = true;
    } else if (hotkey_action == SCIM_FRONTEND_HOTKEY_PREVIOUS_FACTORY) {
        open_previous_factory (ic);
        ret = true;
    } else if (hotkey_action == SCIM_FRONTEND_HOTKEY_SHOW_FACTORY_MENU) {
        panel_req_show_factory_menu (ic);
        ret = true;
    } else if (_imengine_hotkey_matcher.is_matched ()) {
        ISEInfo info = _imengine_hotkey_matcher.get_match_result ();
        ISE_TYPE type = info.type;
        if (type == IMENGINE_T)
            open_specific_factory (ic, info.uuid);
        else if (type == HELPER_T)
            _panel_client.start_helper (ic->id, info.uuid);
        ret = true;
    }
    return ret;
}

static bool
panel_initialize (void)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    String display_name;
    {
        const char *p = getenv ("DISPLAY");
        if (p) display_name = String (p);
    }

    if (_panel_client.open_connection (_config->get_name (), display_name) >= 0) {
        int fd = _panel_client.get_connection_number ();

        _panel_iochannel_read_handler = ecore_main_fd_handler_add (fd, ECORE_FD_READ, panel_iochannel_handler, NULL, NULL, NULL);
//        _panel_iochannel_err_handler  = ecore_main_fd_handler_add (fd, ECORE_FD_ERROR, panel_iochannel_handler, NULL, NULL, NULL);

        SCIM_DEBUG_FRONTEND(2) << " Panel FD= " << fd << "\n";

        EcoreIMFContextISF *context_scim = _ic_list;
        while (context_scim != NULL) {
            _panel_client.prepare (context_scim->id);
            _panel_client.register_input_context (context_scim->id, context_scim->impl->si->get_factory_uuid ());
            _panel_client.send ();
            context_scim = context_scim->next;
        }

        if (_focused_ic) {
            _panel_client.prepare (_focused_ic->id);
            panel_req_focus_in (_focused_ic);
            _panel_client.send ();
        }

        return true;
    }
    std::cerr << "panel_initialize () failed!!!\n";
    return false;
}

static void
panel_finalize (void)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    _panel_client.close_connection ();

    if (_panel_iochannel_read_handler) {
        ecore_main_fd_handler_del (_panel_iochannel_read_handler);
        _panel_iochannel_read_handler = 0;
    }
    if (_panel_iochannel_err_handler) {
        ecore_main_fd_handler_del (_panel_iochannel_err_handler);
        _panel_iochannel_err_handler = 0;
    }
}

static Eina_Bool
panel_iochannel_handler (void *data, Ecore_Fd_Handler *fd_handler)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    if (fd_handler == _panel_iochannel_read_handler) {
        if (_panel_client.has_pending_event () && !_panel_client.filter_event ()) {
            panel_finalize ();
            panel_initialize ();
            return ECORE_CALLBACK_CANCEL;
        }
    } else if (fd_handler == _panel_iochannel_err_handler) {
        panel_finalize ();
        panel_initialize ();
        return ECORE_CALLBACK_CANCEL;
    }
    return ECORE_CALLBACK_RENEW;
}

static void
turn_on_ic (EcoreIMFContextISF *ic)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    if (ic && ic->impl && !ic->impl->is_on) {
        ic->impl->is_on = true;

        if (ic == _focused_ic) {
            panel_req_focus_in (ic);
//            panel_req_update_spot_location (ic);
            panel_req_update_factory_info (ic);
            _panel_client.turn_on (ic->id);
//            _panel_client.hide_preedit_string (ic->id);
//            _panel_client.hide_aux_string (ic->id);
//            _panel_client.hide_lookup_table (ic->id);
            ic->impl->si->focus_in ();
            ic->impl->si->set_layout (ecore_imf_context_input_panel_layout_get (ic->ctx));
            set_prediction_allow (ic->ctx, ic->impl->prediction_allow);
        }

        //Record the IC on/off status
        if (_shared_input_method) {
            _config->write (String (SCIM_CONFIG_FRONTEND_IM_OPENED_BY_DEFAULT), true);
            _config->flush ();
        }

        if (ic->impl->use_preedit && ic->impl->preedit_string.length ()) {
            ecore_imf_context_preedit_start_event_add (ic->ctx);
            ecore_imf_context_event_callback_call (ic->ctx, ECORE_IMF_CALLBACK_PREEDIT_START, NULL);
            ecore_imf_context_preedit_changed_event_add (ic->ctx);
            ecore_imf_context_event_callback_call (ic->ctx, ECORE_IMF_CALLBACK_PREEDIT_CHANGED, NULL);
            ic->impl->preedit_started = true;
        }
    }
}

static void
turn_off_ic (EcoreIMFContextISF *ic)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    if (ic && ic->impl && ic->impl->is_on) {
        ic->impl->is_on = false;

        if (ic == _focused_ic) {
            ic->impl->si->focus_out ();

//            panel_req_update_factory_info (ic);
            _panel_client.turn_off (ic->id);
        }

        //Record the IC on/off status
        if (_shared_input_method) {
            _config->write (String (SCIM_CONFIG_FRONTEND_IM_OPENED_BY_DEFAULT), false);
            _config->flush ();
        }

        if (ic->impl->use_preedit && ic->impl->preedit_string.length ()) {
            ecore_imf_context_preedit_changed_event_add (ic->ctx);
            ecore_imf_context_event_callback_call (ic->ctx, ECORE_IMF_CALLBACK_PREEDIT_CHANGED, NULL);
            ecore_imf_context_preedit_end_event_add (ic->ctx);
            ecore_imf_context_event_callback_call (ic->ctx, ECORE_IMF_CALLBACK_PREEDIT_END, NULL);
            ic->impl->preedit_started = false;
        }
    }
}

static void
set_ic_capabilities (EcoreIMFContextISF *ic)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    if (ic && ic->impl) {
        unsigned int cap = SCIM_CLIENT_CAP_ALL_CAPABILITIES;

        if (!_on_the_spot || !ic->impl->use_preedit)
            cap -= SCIM_CLIENT_CAP_ONTHESPOT_PREEDIT;

        ic->impl->si->update_client_capabilities (cap);
    }
}

static bool
check_socket_frontend (void)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    SocketAddress address;
    SocketClient client;

    uint32 magic;

    address.set_address (scim_get_default_socket_frontend_address ());

    if (!client.connect (address))
        return false;

    if (!scim_socket_open_connection (magic,
                                      String ("ConnectionTester"),
                                      String ("SocketFrontEnd"),
                                      client,
                                      1000)) {
        return false;
    }

    return true;
}

void
initialize (void)
{
    std::vector<String>     config_list;
    std::vector<String>     engine_list;
    std::vector<String>     helper_list;
    std::vector<String>     load_engine_list;

    std::vector<String>::iterator it;

    bool                    manual = false;
    bool                    socket = true;
    String                  config_module_name = "simple";

    SCIM_DEBUG_FRONTEND(1) << "Initializing Ecore ISF IMModule...\n";

    // Get system language.
    _language = scim_get_locale_language (scim_get_current_locale ());

    if (socket) {
        // If no Socket FrontEnd is running, then launch one.
        // And set manual to false.
        bool check_result = check_socket_frontend ();
        if (!check_result) {
            std::cerr << "Launching a ISF daemon with Socket FrontEnd...\n";
            //get modules list
            scim_get_imengine_module_list (engine_list);
            scim_get_helper_module_list (helper_list);

            for (it = engine_list.begin (); it != engine_list.end (); it++) {
                if (*it != "socket")
                    load_engine_list.push_back (*it);
            }
            for (it = helper_list.begin (); it != helper_list.end (); it++)
                load_engine_list.push_back (*it);
            const char *new_argv [] = { "--no-stay", 0 };
            scim_launch (true,
                         config_module_name,
                         (load_engine_list.size () > 0 ? scim_combine_string_list (load_engine_list, ',') : "none"),
                         "socket",
                         (char **)new_argv);
            manual = false;
        }

        // If there is one Socket FrontEnd running and it's not manual mode,
        // then just use this Socket Frontend.
        if (!manual) {
            for (int i = 0; i < 200; ++i) {
                if (check_result) {
                    config_module_name = "socket";
                    load_engine_list.clear ();
                    load_engine_list.push_back ("socket");
                    break;
                }
                scim_usleep (50000);
                check_result = check_socket_frontend ();
            }
        }
    }

    if (config_module_name != "dummy") {
        //load config module
        SCIM_DEBUG_FRONTEND(1) << "Loading Config module: " << config_module_name << "...\n";
        _config_module = new ConfigModule (config_module_name);

        //create config instance
        if (_config_module != NULL && _config_module->valid ())
            _config = _config_module->create_config ();
    }

    if (_config.null ()) {
        SCIM_DEBUG_FRONTEND(1) << "Config module cannot be loaded, using dummy Config.\n";

        if (_config_module) delete _config_module;
        _config_module = NULL;

        _config = new DummyConfig ();
        config_module_name = "dummy";
    }

    reload_config_callback (_config);
    _config->signal_connect_reload (slot (reload_config_callback));

    // create backend
    _backend = new CommonBackEnd (_config, load_engine_list.size () > 0 ? load_engine_list : engine_list);

    if (_backend.null ()) {
        std::cerr << "Cannot create BackEnd Object!\n";
    } else {
        _backend->initialize (_config, load_engine_list.size () > 0 ? load_engine_list : engine_list, false, false);
        _fallback_factory = _backend->get_factory (SCIM_COMPOSE_KEY_FACTORY_UUID);
    }

    if (_fallback_factory.null ())
        _fallback_factory = new DummyIMEngineFactory ();

    _fallback_instance = _fallback_factory->create_instance (String ("UTF-8"), 0);
    _fallback_instance->signal_connect_commit_string (slot (fallback_commit_string_cb));

    // Attach Panel Client signal.
    _panel_client.signal_connect_reload_config                 (slot (panel_slot_reload_config));
    _panel_client.signal_connect_exit                          (slot (panel_slot_exit));
    _panel_client.signal_connect_update_candidate_item_layout  (slot (panel_slot_update_candidate_item_layout));
    _panel_client.signal_connect_update_lookup_table_page_size (slot (panel_slot_update_lookup_table_page_size));
    _panel_client.signal_connect_lookup_table_page_up          (slot (panel_slot_lookup_table_page_up));
    _panel_client.signal_connect_lookup_table_page_down        (slot (panel_slot_lookup_table_page_down));
    _panel_client.signal_connect_trigger_property              (slot (panel_slot_trigger_property));
    _panel_client.signal_connect_process_helper_event          (slot (panel_slot_process_helper_event));
    _panel_client.signal_connect_move_preedit_caret            (slot (panel_slot_move_preedit_caret));
    _panel_client.signal_connect_update_preedit_caret          (slot (panel_slot_update_preedit_caret));
    _panel_client.signal_connect_select_aux                    (slot (panel_slot_select_aux));
    _panel_client.signal_connect_select_candidate              (slot (panel_slot_select_candidate));
    _panel_client.signal_connect_process_key_event             (slot (panel_slot_process_key_event));
    _panel_client.signal_connect_commit_string                 (slot (panel_slot_commit_string));
    _panel_client.signal_connect_forward_key_event             (slot (panel_slot_forward_key_event));
    _panel_client.signal_connect_request_help                  (slot (panel_slot_request_help));
    _panel_client.signal_connect_request_factory_menu          (slot (panel_slot_request_factory_menu));
    _panel_client.signal_connect_change_factory                (slot (panel_slot_change_factory));
    _panel_client.signal_connect_reset_keyboard_ise            (slot (panel_slot_reset_keyboard_ise));
    _panel_client.signal_connect_update_keyboard_ise           (slot (panel_slot_update_keyboard_ise));
    _panel_client.signal_connect_show_preedit_string           (slot (panel_slot_show_preedit_string));
    _panel_client.signal_connect_hide_preedit_string           (slot (panel_slot_hide_preedit_string));
    _panel_client.signal_connect_update_preedit_string         (slot (panel_slot_update_preedit_string));
    _panel_client.signal_connect_get_surrounding_text          (slot (panel_slot_get_surrounding_text));
    _panel_client.signal_connect_delete_surrounding_text       (slot (panel_slot_delete_surrounding_text));
    _panel_client.signal_connect_update_displayed_candidate_number (slot (panel_slot_update_displayed_candidate_number));
    _panel_client.signal_connect_candidate_more_window_show    (slot (panel_slot_candidate_more_window_show));
    _panel_client.signal_connect_candidate_more_window_hide    (slot (panel_slot_candidate_more_window_hide));
    _panel_client.signal_connect_longpress_candidate           (slot (panel_slot_longpress_candidate));
    _panel_client.signal_connect_update_client_id              (slot (panel_slot_update_client_id));

    if (!panel_initialize ()) {
        std::cerr << "Ecore IM Module: Cannot connect to Panel!\n";
    }
}

static void
finalize (void)
{
    SCIM_DEBUG_FRONTEND(1) << "Finalizing Ecore ISF IMModule...\n";

    // Reset this first so that the shared instance could be released correctly afterwards.
    _default_instance.reset ();

    SCIM_DEBUG_FRONTEND(2) << "Finalize all IC partially.\n";
    while (_used_ic_impl_list) {
        // In case in "shared input method" mode,
        // all contexts share only one instance,
        // so we need point the reference pointer correctly before finalizing.
        _used_ic_impl_list->si->set_frontend_data (static_cast <void*> (_used_ic_impl_list->parent));
        isf_imf_context_del (_used_ic_impl_list->parent->ctx);
    }

    delete_all_ic_impl ();

    _fallback_instance.reset ();
    _fallback_factory.reset ();

    SCIM_DEBUG_FRONTEND(2) << " Releasing BackEnd...\n";
    _backend.reset ();

    SCIM_DEBUG_FRONTEND(2) << " Releasing Config...\n";
    _config.reset ();

    if (_config_module) {
        SCIM_DEBUG_FRONTEND(2) << " Deleting _config_module...\n";
        delete _config_module;
        _config_module = 0;
    }

    _focused_ic = NULL;
    _ic_list = NULL;

    _scim_initialized = false;

    _panel_client.reset_signal_handler ();
    panel_finalize ();
}

static void
_popup_message (const char *_ptext)
{
    if (_ptext == NULL)
        return;

    notification_status_message_post(_ptext);
}

static void
_display_input_language (EcoreIMFContextISF *ic)
{
    IMEngineFactoryPointer sf;

    if (ic && ic->impl) {
        if (ic->impl->is_on) {
            sf = _backend->get_factory (ic->impl->si->get_factory_uuid ());
            _popup_message (scim_get_language_name (sf->get_language ()).c_str ());
        }
        else {
            _popup_message (scim_get_language_name ("en").c_str());
        }
    }
}

static void
open_next_factory (EcoreIMFContextISF *ic)
{
    SCIM_DEBUG_FRONTEND(2) << __FUNCTION__ << " context=" << ic->id << "\n";
    IMEngineFactoryPointer sf = _backend->get_next_factory ("", "UTF-8", ic->impl->si->get_factory_uuid ());

    if (!sf.null ()) {
        turn_off_ic (ic);
        ic->impl->si = sf->create_instance ("UTF-8", ic->impl->si->get_id ());
        ic->impl->si->set_frontend_data (static_cast <void*> (ic));
        ic->impl->preedit_string = WideString ();
        ic->impl->preedit_caret = 0;
        attach_instance (ic->impl->si);
        _backend->set_default_factory (_language, sf->get_uuid ());
        _panel_client.register_input_context (ic->id, sf->get_uuid ());
        _panel_client.set_candidate_style (ic->id, ONE_LINE_CANDIDATE, FIXED_CANDIDATE_WINDOW);
        set_ic_capabilities (ic);
        turn_on_ic (ic);

        if (_shared_input_method) {
            _default_instance = ic->impl->si;
            ic->impl->shared_si = true;
        }
        _popup_message (utf8_wcstombs (sf->get_name ()).c_str ());
    }
}

static void
open_previous_factory (EcoreIMFContextISF *ic)
{
    if (ic == NULL)
        return;

    SCIM_DEBUG_FRONTEND(2) << __FUNCTION__ << " context=" << ic->id << "\n";
    IMEngineFactoryPointer sf = _backend->get_previous_factory ("", "UTF-8", ic->impl->si->get_factory_uuid ());

    if (!sf.null ()) {
        turn_off_ic (ic);
        ic->impl->si = sf->create_instance ("UTF-8", ic->impl->si->get_id ());
        ic->impl->si->set_frontend_data (static_cast <void*> (ic));
        ic->impl->preedit_string = WideString ();
        ic->impl->preedit_caret = 0;
        attach_instance (ic->impl->si);
        _backend->set_default_factory (_language, sf->get_uuid ());
        _panel_client.register_input_context (ic->id, sf->get_uuid ());
        _panel_client.set_candidate_style (ic->id, ONE_LINE_CANDIDATE, FIXED_CANDIDATE_WINDOW);
        set_ic_capabilities (ic);
        turn_on_ic (ic);

        if (_shared_input_method) {
            _default_instance = ic->impl->si;
            ic->impl->shared_si = true;
        }
        _popup_message (utf8_wcstombs (sf->get_name ()).c_str ());
    }
}

static void
open_specific_factory (EcoreIMFContextISF *ic,
                       const String     &uuid)
{
    if (ic == NULL)
        return;

    SCIM_DEBUG_FRONTEND(2) << __FUNCTION__ << " context=" << ic->id << "\n";

    // The same input method is selected, just turn on the IC.
    if (ic->impl->si->get_factory_uuid () == uuid) {
        turn_on_ic (ic);
        return;
    }

    IMEngineFactoryPointer sf = _backend->get_factory (uuid);

    if (uuid.length () && !sf.null ()) {
        turn_off_ic (ic);
        ic->impl->si = sf->create_instance ("UTF-8", ic->impl->si->get_id ());
        ic->impl->si->set_frontend_data (static_cast <void*> (ic));
        ic->impl->preedit_string = WideString ();
        ic->impl->preedit_caret = 0;
        attach_instance (ic->impl->si);
        _backend->set_default_factory (_language, sf->get_uuid ());
        _panel_client.register_input_context (ic->id, sf->get_uuid ());
        set_ic_capabilities (ic);
        turn_on_ic (ic);

        if (_shared_input_method) {
            _default_instance = ic->impl->si;
            ic->impl->shared_si = true;
        }
    } else {
        std::cerr << "open_specific_factory () is failed!!!!!!\n";
        LOGE ("open_specific_factory () is failed. ic : %x uuid : %s", ic->id, uuid.c_str());

        // turn_off_ic comment out panel_req_update_factory_info ()
        //turn_off_ic (ic);
        if (ic && ic->impl->is_on) {
            ic->impl->is_on = false;

            if (ic == _focused_ic) {
                ic->impl->si->focus_out ();

                panel_req_update_factory_info (ic);
                _panel_client.turn_off (ic->id);
            }

            //Record the IC on/off status
            if (_shared_input_method) {
                _config->write (String (SCIM_CONFIG_FRONTEND_IM_OPENED_BY_DEFAULT), false);
                _config->flush ();
            }

            if (ic->impl->use_preedit && ic->impl->preedit_string.length ()) {
                ecore_imf_context_preedit_changed_event_add (ic->ctx);
                ecore_imf_context_event_callback_call (ic->ctx, ECORE_IMF_CALLBACK_PREEDIT_CHANGED, NULL);
                ecore_imf_context_preedit_end_event_add (ic->ctx);
                ecore_imf_context_event_callback_call (ic->ctx, ECORE_IMF_CALLBACK_PREEDIT_END, NULL);
                ic->impl->preedit_started = false;
            }
        }
    }
}

static void initialize_modifier_bits (Display *display)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    if (__current_display == display)
        return;

    __current_display = display;

    if (display == 0) {
        __current_alt_mask     = Mod1Mask;
        __current_meta_mask    = ShiftMask | Mod1Mask;
        __current_super_mask   = 0;
        __current_hyper_mask   = 0;
        __current_numlock_mask = Mod2Mask;
        return;
    }

    XModifierKeymap *mods = NULL;

    ::KeyCode ctrl_l  = XKeysymToKeycode (display, XK_Control_L);
    ::KeyCode ctrl_r  = XKeysymToKeycode (display, XK_Control_R);
    ::KeyCode meta_l  = XKeysymToKeycode (display, XK_Meta_L);
    ::KeyCode meta_r  = XKeysymToKeycode (display, XK_Meta_R);
    ::KeyCode alt_l   = XKeysymToKeycode (display, XK_Alt_L);
    ::KeyCode alt_r   = XKeysymToKeycode (display, XK_Alt_R);
    ::KeyCode super_l = XKeysymToKeycode (display, XK_Super_L);
    ::KeyCode super_r = XKeysymToKeycode (display, XK_Super_R);
    ::KeyCode hyper_l = XKeysymToKeycode (display, XK_Hyper_L);
    ::KeyCode hyper_r = XKeysymToKeycode (display, XK_Hyper_R);
    ::KeyCode numlock = XKeysymToKeycode (display, XK_Num_Lock);

    int i, j;

    mods = XGetModifierMapping (display);
    if (mods == NULL)
        return;

    __current_alt_mask     = 0;
    __current_meta_mask    = 0;
    __current_super_mask   = 0;
    __current_hyper_mask   = 0;
    __current_numlock_mask = 0;

    /* We skip the first three sets for Shift, Lock, and Control.  The
        remaining sets are for Mod1, Mod2, Mod3, Mod4, and Mod5.  */
    for (i = 3; i < 8; i++) {
        for (j = 0; j < mods->max_keypermod; j++) {
            ::KeyCode code = mods->modifiermap [i * mods->max_keypermod + j];
            if (! code) continue;
            if (code == alt_l || code == alt_r)
                __current_alt_mask |= (1 << i);
            else if (code == meta_l || code == meta_r)
                __current_meta_mask |= (1 << i);
            else if (code == super_l || code == super_r)
                __current_super_mask |= (1 << i);
            else if (code == hyper_l || code == hyper_r)
                __current_hyper_mask |= (1 << i);
            else if (code == numlock)
                __current_numlock_mask |= (1 << i);
        }
    }

    /* Check whether there is a combine keys mapped to Meta */
    if (__current_meta_mask == 0) {
        char buf [32];
        XKeyEvent xkey;
        KeySym keysym_l, keysym_r;

        xkey.type = KeyPress;
        xkey.display = display;
        xkey.serial = 0L;
        xkey.send_event = False;
        xkey.x = xkey.y = xkey.x_root = xkey.y_root = 0;
        xkey.time = 0;
        xkey.same_screen = False;
        xkey.subwindow = None;
        xkey.window = None;
        xkey.root = DefaultRootWindow (display);
        xkey.state = ShiftMask;

        xkey.keycode = meta_l;
        XLookupString (&xkey, buf, 32, &keysym_l, 0);
        xkey.keycode = meta_r;
        XLookupString (&xkey, buf, 32, &keysym_r, 0);

        if ((meta_l == alt_l && keysym_l == XK_Meta_L) || (meta_r == alt_r && keysym_r == XK_Meta_R))
            __current_meta_mask = ShiftMask + __current_alt_mask;
        else if ((meta_l == ctrl_l && keysym_l == XK_Meta_L) || (meta_r == ctrl_r && keysym_r == XK_Meta_R))
            __current_meta_mask = ShiftMask + ControlMask;
    }

    XFreeModifiermap (mods);
}

static unsigned int scim_x11_keymask_scim_to_x11 (Display *display, uint16 scimkeymask)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    unsigned int state = 0;

    initialize_modifier_bits (display);

    if (scimkeymask & SCIM_KEY_ShiftMask)    state |= ShiftMask;
    if (scimkeymask & SCIM_KEY_CapsLockMask) state |= LockMask;
    if (scimkeymask & SCIM_KEY_ControlMask)  state |= ControlMask;
    if (scimkeymask & SCIM_KEY_AltMask)      state |= __current_alt_mask;
    if (scimkeymask & SCIM_KEY_MetaMask)     state |= __current_meta_mask;
    if (scimkeymask & SCIM_KEY_SuperMask)    state |= __current_super_mask;
    if (scimkeymask & SCIM_KEY_HyperMask)    state |= __current_hyper_mask;
    if (scimkeymask & SCIM_KEY_NumLockMask)  state |= __current_numlock_mask;

    return state;
}

static XKeyEvent createKeyEvent (bool press, int keycode, int modifiers, bool fake)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    XKeyEvent event;
    Window focus_win;
    Display *display = (Display *)ecore_x_display_get ();
    int revert = RevertToParent;

    XGetInputFocus (display, &focus_win, &revert);

    event.display     = display;
    event.window      = focus_win;
    event.root        = DefaultRootWindow (display);
    event.subwindow   = None;
    if (fake)
        event.time    = 0;
    else
        event.time    = get_time ();

    event.x           = 1;
    event.y           = 1;
    event.x_root      = 1;
    event.y_root      = 1;
    event.same_screen = True;
    event.state       = modifiers;
    event.keycode     = keycode;
    if (press)
        event.type = KeyPress;
    else
        event.type = KeyRelease;
    event.send_event  = False;
    event.serial      = 0;

    return event;
}

static void send_x_key_event (const KeyEvent &key, bool fake)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    ::KeyCode keycode = 0;
    ::KeySym keysym = 0;
    int shift = 0;
    char key_string[256] = {0};
    char keysym_str[256] = {0};
    Window focus_win;
    int revert = RevertToParent;

    // Obtain the X11 display.
    Display *display = (Display *)ecore_x_display_get ();
    if (display == NULL) {
        std::cerr << "ecore_x_display_get () failed\n";
        return;
    }

    // Check focus window
    XGetInputFocus (display, &focus_win, &revert);
    if (focus_win == None) {
        LOGE ("Input focus window is None\n");
        return;
    }

    if (strncmp (key.get_key_string ().c_str (), "KeyRelease+", 11) == 0) {
        snprintf (key_string, sizeof (key_string), "%s", key.get_key_string ().c_str () + 11);
    } else {
        snprintf (key_string, sizeof (key_string), "%s", key.get_key_string ().c_str ());
    }

    if (strncmp (key_string, "Shift+", 6) == 0) {
        snprintf (keysym_str, sizeof (keysym_str), "%s", key_string + 6);
    } else {
        snprintf (keysym_str, sizeof (keysym_str), "%s", key_string);
    }

    // get x keysym, keycode, keyname, and key
    keysym = XStringToKeysym (keysym_str);
    if (keysym == NoSymbol)
        return;

    keycode = _keyname_to_keycode (keysym_str);
    if (XkbKeycodeToKeysym (display, keycode, 0, 0) != keysym) {
        if (XkbKeycodeToKeysym (display, keycode, 0, 1) == keysym)
            shift = 1;
        else
            keycode = 0;
    } else {
        shift = 0;
    }

    if (keycode == 0) {
        static int mod = 0;
        KeySym *keysyms;
        int keycode_min, keycode_max, keycode_num;
        int i;

        XDisplayKeycodes (display, &keycode_min, &keycode_max);
        keysyms = XGetKeyboardMapping (display, keycode_min,
                keycode_max - keycode_min + 1,
                &keycode_num);
        mod = (mod + 1) & 0x7;
        i = (keycode_max - keycode_min - mod - 1) * keycode_num;

        keysyms[i] = keysym;
        XChangeKeyboardMapping (display, keycode_min, keycode_num,
                keysyms, (keycode_max - keycode_min));
        XFree (keysyms);
        XSync (display, False);
        keycode = keycode_max - mod - 1;
    }

    unsigned int modifier = scim_x11_keymask_scim_to_x11 (display, key.mask);

    if (shift)
        modifier |= ShiftMask;

    XKeyEvent event;
    if (key.is_key_press ()) {
        if (shift) {
            event = createKeyEvent (true, XKeysymToKeycode (display, XK_Shift_L), modifier, fake);
            XSendEvent (event.display, event.window, True, KeyPressMask, (XEvent *)&event);
        }

        event = createKeyEvent (true, keycode, modifier, fake);
        XSendEvent (event.display, event.window, True, KeyPressMask, (XEvent *)&event);
    } else {
        event = createKeyEvent (false, keycode, modifier, fake);
        XSendEvent (event.display, event.window, True, KeyReleaseMask, (XEvent *)&event);

        if (shift) {
            event = createKeyEvent (false, XKeysymToKeycode (display, XK_Shift_L), modifier, fake);
            XSendEvent (event.display, event.window, True, KeyReleaseMask, (XEvent *)&event);
        }
    }
    _x_key_event_is_valid = true;
}

static void
attach_instance (const IMEngineInstancePointer &si)
{
    si->signal_connect_show_preedit_string (
        slot (slot_show_preedit_string));
    si->signal_connect_show_aux_string (
        slot (slot_show_aux_string));
    si->signal_connect_show_lookup_table (
        slot (slot_show_lookup_table));

    si->signal_connect_hide_preedit_string (
        slot (slot_hide_preedit_string));
    si->signal_connect_hide_aux_string (
        slot (slot_hide_aux_string));
    si->signal_connect_hide_lookup_table (
        slot (slot_hide_lookup_table));

    si->signal_connect_update_preedit_caret (
        slot (slot_update_preedit_caret));
    si->signal_connect_update_preedit_string (
        slot (slot_update_preedit_string));
    si->signal_connect_update_aux_string (
        slot (slot_update_aux_string));
    si->signal_connect_update_lookup_table (
        slot (slot_update_lookup_table));

    si->signal_connect_commit_string (
        slot (slot_commit_string));

    si->signal_connect_forward_key_event (
        slot (slot_forward_key_event));

    si->signal_connect_register_properties (
        slot (slot_register_properties));

    si->signal_connect_update_property (
        slot (slot_update_property));

    si->signal_connect_beep (
        slot (slot_beep));

    si->signal_connect_start_helper (
        slot (slot_start_helper));

    si->signal_connect_stop_helper (
        slot (slot_stop_helper));

    si->signal_connect_send_helper_event (
        slot (slot_send_helper_event));

    si->signal_connect_get_surrounding_text (
        slot (slot_get_surrounding_text));

    si->signal_connect_delete_surrounding_text (
        slot (slot_delete_surrounding_text));

    si->signal_connect_expand_candidate (
        slot (slot_expand_candidate));
    si->signal_connect_contract_candidate (
        slot (slot_contract_candidate));

    si->signal_connect_set_candidate_style (
        slot (slot_set_candidate_style));
}

// Implementation of slot functions
static void
slot_show_preedit_string (IMEngineInstanceBase *si)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *ic = static_cast<EcoreIMFContextISF *> (si->get_frontend_data ());

    if (ic && ic->impl && _focused_ic == ic) {
        if (ic->impl->use_preedit) {
            if (!ic->impl->preedit_started) {
                ecore_imf_context_preedit_start_event_add (_focused_ic->ctx);
                ecore_imf_context_event_callback_call (_focused_ic->ctx, ECORE_IMF_CALLBACK_PREEDIT_START, NULL);
                ic->impl->preedit_started = true;
            }
            //if (ic->impl->preedit_string.length ())
            //    ecore_imf_context_preedit_changed_event_add (_focused_ic->ctx);
        } else {
            _panel_client.show_preedit_string (ic->id);
        }
    }
}

static void
slot_show_aux_string (IMEngineInstanceBase *si)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *ic = static_cast<EcoreIMFContextISF *> (si->get_frontend_data ());

    if (ic && ic->impl && _focused_ic == ic)
        _panel_client.show_aux_string (ic->id);
}

static void
slot_show_lookup_table (IMEngineInstanceBase *si)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *ic = static_cast<EcoreIMFContextISF *> (si->get_frontend_data ());

    if (ic && ic->impl && _focused_ic == ic)
        _panel_client.show_lookup_table (ic->id);
}

static void
slot_hide_preedit_string (IMEngineInstanceBase *si)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *ic = static_cast<EcoreIMFContextISF *> (si->get_frontend_data ());

    if (ic && ic->impl && _focused_ic == ic) {
        bool emit = false;
        if (ic->impl->preedit_string.length ()) {
            ic->impl->preedit_string = WideString ();
            ic->impl->preedit_caret = 0;
            ic->impl->preedit_attrlist.clear ();
            emit = true;
        }
        if (ic->impl->use_preedit) {
            if (emit) {
                ecore_imf_context_preedit_changed_event_add (ic->ctx);
                ecore_imf_context_event_callback_call (ic->ctx, ECORE_IMF_CALLBACK_PREEDIT_CHANGED, NULL);
            }
            if (ic->impl->preedit_started) {
                ecore_imf_context_preedit_end_event_add (ic->ctx);
                ecore_imf_context_event_callback_call (ic->ctx, ECORE_IMF_CALLBACK_PREEDIT_END, NULL);
                ic->impl->preedit_started = false;
            }
        } else {
            _panel_client.hide_preedit_string (ic->id);
        }
    }
}

static void
slot_hide_aux_string (IMEngineInstanceBase *si)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *ic = static_cast<EcoreIMFContextISF *> (si->get_frontend_data ());

    if (ic && ic->impl && _focused_ic == ic)
        _panel_client.hide_aux_string (ic->id);
}

static void
slot_hide_lookup_table (IMEngineInstanceBase *si)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *ic = static_cast<EcoreIMFContextISF *> (si->get_frontend_data ());

    if (ic && ic->impl && _focused_ic == ic)
        _panel_client.hide_lookup_table (ic->id);
}

static void
slot_update_preedit_caret (IMEngineInstanceBase *si, int caret)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *ic = static_cast<EcoreIMFContextISF *> (si->get_frontend_data ());

    if (ic && ic->impl && _focused_ic == ic && ic->impl->preedit_caret != caret) {
        ic->impl->preedit_caret = caret;
        if (ic->impl->use_preedit) {
            if (!ic->impl->preedit_started) {
                ecore_imf_context_preedit_start_event_add (ic->ctx);
                ecore_imf_context_event_callback_call (ic->ctx, ECORE_IMF_CALLBACK_PREEDIT_START, NULL);
                ic->impl->preedit_started = true;
            }
            ecore_imf_context_preedit_changed_event_add (ic->ctx);
            ecore_imf_context_event_callback_call (ic->ctx, ECORE_IMF_CALLBACK_PREEDIT_CHANGED, NULL);
        } else {
            _panel_client.update_preedit_caret (ic->id, caret);
        }
    }
}

static void
slot_update_preedit_string (IMEngineInstanceBase *si,
                            const WideString & str,
                            const AttributeList & attrs)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *ic = static_cast<EcoreIMFContextISF *> (si->get_frontend_data ());

    if (ic && ic->impl && _focused_ic == ic && (ic->impl->preedit_string != str || str.length ())) {
        ic->impl->preedit_string   = str;
        ic->impl->preedit_attrlist = attrs;
        if (ic->impl->use_preedit) {
            if (!ic->impl->preedit_started) {
                ecore_imf_context_preedit_start_event_add (_focused_ic->ctx);
                ecore_imf_context_event_callback_call (_focused_ic->ctx, ECORE_IMF_CALLBACK_PREEDIT_START, NULL);
                ic->impl->preedit_started = true;
            }
            ic->impl->preedit_caret    = str.length ();
            ic->impl->preedit_updating = true;
            ecore_imf_context_preedit_changed_event_add (ic->ctx);
            ecore_imf_context_event_callback_call (ic->ctx, ECORE_IMF_CALLBACK_PREEDIT_CHANGED, NULL);
            ic->impl->preedit_updating = false;
        } else {
            _panel_client.update_preedit_string (ic->id, str, attrs);
        }
    }
}

static void
slot_update_aux_string (IMEngineInstanceBase *si,
                        const WideString & str,
                        const AttributeList & attrs)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *ic = static_cast<EcoreIMFContextISF *> (si->get_frontend_data ());

    if (ic && ic->impl && _focused_ic == ic)
        _panel_client.update_aux_string (ic->id, str, attrs);
}

static void
slot_commit_string (IMEngineInstanceBase *si,
                    const WideString & str)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *ic = static_cast<EcoreIMFContextISF *> (si->get_frontend_data ());

    if (ic && ic->ctx) {
        if (strcmp (utf8_wcstombs (str).c_str (), " ") == 0)
            autoperiod_insert (ic->ctx);

        Eina_Bool auto_capitalized = EINA_FALSE;

        if (ic->impl) {
            if (ecore_imf_context_input_panel_layout_get (ic->ctx) == ECORE_IMF_INPUT_PANEL_LAYOUT_NORMAL &&
                ic->impl->shift_mode_enabled &&
                ic->impl->autocapital_type != ECORE_IMF_AUTOCAPITAL_TYPE_NONE &&
                hw_keyboard_num_get() == 0) {
                char converted[2] = {'\0'};
                if (utf8_wcstombs (str).length () == 1) {
                    Eina_Bool uppercase;
                    switch (ic->impl->next_shift_status) {
                        case 0:
                            uppercase = caps_mode_check (ic->ctx, EINA_FALSE, EINA_FALSE);
                            break;
                        case SHIFT_MODE_OFF:
                            uppercase = EINA_FALSE;
                            ic->impl->next_shift_status = 0;
                            break;
                        case SHIFT_MODE_ON:
                            uppercase = EINA_TRUE;
                            ic->impl->next_shift_status = 0;
                            break;
                        case SHIFT_MODE_LOCK:
                            uppercase = EINA_TRUE;
                            break;
                        default:
                            uppercase = EINA_FALSE;
                    }
                    converted[0] = utf8_wcstombs (str).at (0);
                    if (uppercase) {
                        if(converted[0] >= 'a' && converted[0] <= 'z')
                            converted[0] -= 32;
                    } else {
                        if(converted[0] >= 'A' && converted[0] <= 'Z')
                            converted[0] += 32;
                    }

                    ecore_imf_context_commit_event_add (ic->ctx, converted);
                    ecore_imf_context_event_callback_call (ic->ctx, ECORE_IMF_CALLBACK_COMMIT, (void *)converted);

                    auto_capitalized = EINA_TRUE;
                }
            }
        }

        if (!auto_capitalized) {
            ecore_imf_context_commit_event_add (ic->ctx, utf8_wcstombs (str).c_str ());
            ecore_imf_context_event_callback_call (ic->ctx, ECORE_IMF_CALLBACK_COMMIT, (void *)utf8_wcstombs (str).c_str ());
        }
    }
}

static void
slot_forward_key_event (IMEngineInstanceBase *si,
                        const KeyEvent & key)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *ic = static_cast<EcoreIMFContextISF *> (si->get_frontend_data ());

    if (ic && _focused_ic == ic) {
        if (!_fallback_instance->process_key_event (key)) {
            feed_key_event (ic, key, true);
        }
    }
}

static void
slot_update_lookup_table (IMEngineInstanceBase *si,
                          const LookupTable & table)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *ic = static_cast<EcoreIMFContextISF *> (si->get_frontend_data ());

    if (ic && ic->impl && _focused_ic == ic)
        _panel_client.update_lookup_table (ic->id, table);
}

static void
slot_register_properties (IMEngineInstanceBase *si,
                          const PropertyList & properties)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *ic = static_cast<EcoreIMFContextISF *> (si->get_frontend_data ());

    if (ic && ic->impl && _focused_ic == ic)
        _panel_client.register_properties (ic->id, properties);
}

static void
slot_update_property (IMEngineInstanceBase *si,
                      const Property & property)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *ic = static_cast<EcoreIMFContextISF *> (si->get_frontend_data ());

    if (ic && ic->impl && _focused_ic == ic)
        _panel_client.update_property (ic->id, property);
}

static void
slot_beep (IMEngineInstanceBase *si)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *ic = static_cast<EcoreIMFContextISF *> (si->get_frontend_data ());

    if (ic && ic->impl && _focused_ic == ic)
        ecore_x_bell (0);
}

static void
slot_start_helper (IMEngineInstanceBase *si,
                   const String &helper_uuid)
{
    EcoreIMFContextISF *ic = static_cast<EcoreIMFContextISF *> (si->get_frontend_data ());

    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " helper= " << helper_uuid << " context="
                           << (ic != NULL ? ic->id : -1) << " ic=" << ic
                           << " ic-uuid=" << ((ic != NULL ) ? ic->impl->si->get_factory_uuid () : "") << "...\n";

    if (ic && ic->impl)
        _panel_client.start_helper (ic->id, helper_uuid);
}

static void
slot_stop_helper (IMEngineInstanceBase *si,
                  const String &helper_uuid)
{
    EcoreIMFContextISF *ic = static_cast<EcoreIMFContextISF *> (si->get_frontend_data ());

    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " helper= " << helper_uuid << " context=" << (ic != NULL ? ic->id : -1) << " ic=" << ic << "...\n";

    if (ic && ic->impl)
        _panel_client.stop_helper (ic->id, helper_uuid);
}

static void
slot_send_helper_event (IMEngineInstanceBase *si,
                        const String      &helper_uuid,
                        const Transaction &trans)
{
    EcoreIMFContextISF *ic = static_cast<EcoreIMFContextISF *> (si->get_frontend_data ());

    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << " helper= " << helper_uuid << " context="
                           << (ic != NULL ? ic->id : -1) << " ic=" << ic
                           << " ic-uuid=" << ((ic != NULL) ? ic->impl->si->get_factory_uuid () : "") << "...\n";

    if (ic && ic->impl)
        _panel_client.send_helper_event (ic->id, helper_uuid, trans);
}

static bool
slot_get_surrounding_text (IMEngineInstanceBase *si,
                           WideString            &text,
                           int                   &cursor,
                           int                    maxlen_before,
                           int                    maxlen_after)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *ic = static_cast<EcoreIMFContextISF *> (si->get_frontend_data ());

    if (ic && ic->impl && _focused_ic == ic) {
        char *surrounding = NULL;
        int   cursor_index;
        if (ecore_imf_context_surrounding_get (_focused_ic->ctx, &surrounding, &cursor_index)) {
            SCIM_DEBUG_FRONTEND(2) << "Surrounding text: " << surrounding <<"\n";
            SCIM_DEBUG_FRONTEND(2) << "Cursor Index    : " << cursor_index <<"\n";

            if (!surrounding)
                return false;

            if (cursor_index < 0) {
                free (surrounding);
                surrounding = NULL;
                return false;
            }

            WideString before = utf8_mbstowcs (String (surrounding));
            free (surrounding);
            surrounding = NULL;

            if (cursor_index > (int)before.length())
                return false;
            WideString after = before;
            before = before.substr (0, cursor_index);
            after =  after.substr (cursor_index, after.length () - cursor_index);
            if (maxlen_before > 0 && ((unsigned int)maxlen_before) < before.length ())
                before = WideString (before.begin () + (before.length () - maxlen_before), before.end ());
            else if (maxlen_before == 0)
                before = WideString ();
            if (maxlen_after > 0 && ((unsigned int)maxlen_after) < after.length ())
                after = WideString (after.begin (), after.begin () + maxlen_after);
            else if (maxlen_after == 0)
                after = WideString ();
            text = before + after;
            cursor = before.length ();
            return true;
        }
    }
    return false;
}

static bool
slot_delete_surrounding_text (IMEngineInstanceBase *si,
                              int                   offset,
                              int                   len)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *ic = static_cast<EcoreIMFContextISF *> (si->get_frontend_data ());

    if (ic && ic->impl && _focused_ic == ic) {
        Ecore_IMF_Event_Delete_Surrounding ev;
        ev.ctx = _focused_ic->ctx;
        ev.n_chars = len;
        ev.offset = offset;
        ecore_imf_context_delete_surrounding_event_add (_focused_ic->ctx, offset, len);
        ecore_imf_context_event_callback_call (_focused_ic->ctx, ECORE_IMF_CALLBACK_DELETE_SURROUNDING, &ev);
        return true;
    }
    return false;
}

static void
slot_expand_candidate (IMEngineInstanceBase *si)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *ic = static_cast<EcoreIMFContextISF *> (si->get_frontend_data ());

    if (ic && ic->impl && _focused_ic == ic)
        _panel_client.expand_candidate (ic->id);
}

static void
slot_contract_candidate (IMEngineInstanceBase *si)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *ic = static_cast<EcoreIMFContextISF *> (si->get_frontend_data ());

    if (ic && ic->impl && _focused_ic == ic)
        _panel_client.contract_candidate (ic->id);
}

static void
slot_set_candidate_style (IMEngineInstanceBase *si, ISF_CANDIDATE_PORTRAIT_LINE_T portrait_line, ISF_CANDIDATE_MODE_T mode)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    EcoreIMFContextISF *ic = static_cast<EcoreIMFContextISF *> (si->get_frontend_data ());

    if (ic && ic->impl && _focused_ic == ic)
        _panel_client.set_candidate_style (ic->id, portrait_line, mode);
}

static void
reload_config_callback (const ConfigPointer &config)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    _frontend_hotkey_matcher.load_hotkeys (config);
    _imengine_hotkey_matcher.load_hotkeys (config);

    KeyEvent key;
    scim_string_to_key (key,
                        config->read (String (SCIM_CONFIG_HOTKEYS_FRONTEND_VALID_KEY_MASK),
                                      String ("Shift+Control+Alt+Lock")));

    _valid_key_mask = (key.mask > 0) ? (key.mask) : 0xFFFF;
    _valid_key_mask |= SCIM_KEY_ReleaseMask;
    // Special treatment for two backslash keys on jp106 keyboard.
    _valid_key_mask |= SCIM_KEY_QuirkKanaRoMask;

    _on_the_spot = config->read (String (SCIM_CONFIG_FRONTEND_ON_THE_SPOT), _on_the_spot);
    _shared_input_method = config->read (String (SCIM_CONFIG_FRONTEND_SHARED_INPUT_METHOD), _shared_input_method);

    // Get keyboard layout setting
    // Flush the global config first, in order to load the new configs from disk.
    scim_global_config_flush ();

    _keyboard_layout = scim_get_default_keyboard_layout ();
}

static void
fallback_commit_string_cb (IMEngineInstanceBase  *si,
                           const WideString      &str)
{
    SCIM_DEBUG_FRONTEND(1) << __FUNCTION__ << "...\n";

    if (_focused_ic && _focused_ic->impl) {
        ecore_imf_context_commit_event_add (_focused_ic->ctx, utf8_wcstombs (str).c_str ());
        ecore_imf_context_event_callback_call (_focused_ic->ctx, ECORE_IMF_CALLBACK_COMMIT, (void *)utf8_wcstombs (str).c_str ());
    }
}

/*
vi:ts=4:expandtab:nowrap
*/
