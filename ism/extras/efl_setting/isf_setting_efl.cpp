/*
 * ISF(Input Service Framework)
 *
 * ISF is based on SCIM 1.4.7 and extended for supporting more mobile fitable.
 * Copyright (c) 2012-2013 Samsung Electronics Co., Ltd.
 *
 * Contact: Shuo Liu <shuo0805.liu@samsung.com>, Hengliang Luo <hl.luo@samsung.com>
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

#ifndef UG_MODULE_API
#define UG_MODULE_API __attribute__ ((visibility("default")))
#endif

#define Uses_SCIM_PANEL_AGENT
#define Uses_SCIM_CONFIG_PATH
#define Uses_SCIM_HELPER_MODULE
#define Uses_SCIM_IMENGINE_MODULE
#define Uses_SCIM_COMPOSE_KEY

#include <stdio.h>
#include <fstream>
#include <iostream>
#include <Elementary.h>
#include <Ecore_IMF.h>
#include <Ecore_X.h>
#include <glib.h>
#include <glib-object.h>
#include <vconf-keys.h>
#include <vconf.h>
#include <ui-gadget-module.h>
#include <ui-gadget.h>
#include <pkgmgr-info.h>
#include <efl_assist.h>
#include "scim.h"
#include "scim_stl_map.h"
#include "isf_setting_efl.h"
#include "../efl_panel/isf_panel_utility.h"
#include "scim_setup_module_efl.h"
#include "isf_control.h"

using namespace scim;
using namespace std;

#define _EDJ(x)                                   elm_layout_edje_get(x)
#define ISF_SETTING_EDJ                           (SCIM_DATADIR "/isfsetting.edj")
#define PADDING_X                                 25

#define SETTING_PACKAGE                           "isfsetting-efl"
#define SETTING_LOCALEDIR                         "/usr/ug/res/locale"
#define _T(s)                                     dgettext(SETTING_PACKAGE, s)

#define CSC_FILEPATH                              "/opt/system/csc-default/preset/csc-default-preset.ini"

enum {
    AUTO_CAPITALIZATION_ITEM = 0,
    AUTO_CAPITALIZATION_TXT_ITEM,
    AUTO_FULL_STOP_ITEM,
    AUTO_FULL_STOP_TXT_ITEM,
    SW_KEYBOARD_GROUP_TITLE_ITEM,
    SW_KEYBOARD_SEL_ITEM,
    SW_ISE_OPTION_ITEM,
    HW_KEYBOARD_GROUP_TITLE_ITEM,
    HW_KEYBOARD_SEL_ITEM,
    HW_ISE_OPTION_ITEM,
    ITEM_TOTAL_COUNT
};

typedef enum {
    SEPARATOR_TYPE1,
    SEPARATOR_TYPE2
} separator_type;

enum {
    ITEM_STYLE_NONE,
    ITEM_STYLE_TOP,
    ITEM_STYLE_CENTER,
    ITEM_STYLE_BOTTOM
};

typedef enum {
    ISE_OPTION_MODULE_EXIST_SO = 0,
    ISE_OPTION_MODULE_EXIST_XML,
    ISE_OPTION_MODULE_NO_EXIST
} ISE_OPTION_MODULE_STATE;

struct ItemData
{
    char *text;
    char *sub_text;
    int   mode;
    int   item_style_type;
};

static ISE_OPTION_MODULE_STATE      _ise_option_module_stat   = ISE_OPTION_MODULE_NO_EXIST;
static struct ug_data              *_common_ugd               = NULL;
static ItemData                    *_p_items[ITEM_TOTAL_COUNT];

static Ecore_X_Window               _root_win;
static Ecore_X_Atom                 _prop_x_ext_keyboard_exist= 0;
static Ecore_Event_Handler         *_prop_change_handler      = NULL;

static bool                         _hw_kbd_connected         = false;
static unsigned int                 _hw_kbd_num               = 0;

static int                          _sw_ise_index             = 0;
static int                          _hw_ise_index             = 0;
static Evas_Object                 *_sw_radio_grp             = NULL;
static Evas_Object                 *_hw_radio_grp             = NULL;
static std::vector<String>          _sw_ise_list;
static std::vector<String>          _hw_ise_list;
static std::vector <String>         _setup_modules;
static String                       _mdl_name;

static char                         _sw_ise_bak[256]          = {'\0'};
static char                         _hw_ise_bak[256]          = {'\0'};
static char                         _sw_ise_name[256]         = {'\0'};
static char                         _hw_ise_name[256]         = {'\0'};

static SetupModule                 *_mdl                      = NULL;
static Ecore_IMF_Context           *_imf_context              = NULL;

static Eina_Bool                    _auto_capitalisation      = EINA_FALSE;
static Eina_Bool                    _auto_full_stop           = EINA_FALSE;

static ConfigPointer                _config;
static Connection                   _reload_signal_connection;

static Elm_Genlist_Item_Class       itc1, itc2, itc3, itc4, itc5, itcText, itcTitle, itcSeparator[2];

extern std::vector <String>         _names;
extern std::vector <String>         _uuids;
extern std::vector <String>         _module_names;
extern std::vector <String>         _langs;
extern std::vector<TOOLBAR_MODE_T>  _modes;

static Evas_Object *_gl_icon_get (void *data, Evas_Object *obj, const char *part);
static char        *_gl_label_get (void *data, Evas_Object *obj, const char *part);
static Eina_Bool    _gl_state_get (void *data, Evas_Object *obj, const char *part);
static void         _gl_del (void *data, Evas_Object *obj);
static void         _gl_sel (void *data, Evas_Object *obj, void *event_info);
static void         _gl_sw_ise_sel (void *data, Evas_Object *obj, void *event_info);
static void         _gl_hw_ise_sel (void *data, Evas_Object *obj, void *event_info);
static void         _gl_keyboard_sel (void *data, Evas_Object *obj, void *event_info);
static char        *_gl_exp_sw_label_get (void *data, Evas_Object *obj, const char *part);
static Evas_Object *_gl_exp_sw_icon_get (void *data, Evas_Object *obj, const char *part);

static void         create_sw_keyboard_selection_view (ug_data *ugd);
static void         create_hw_keyboard_selection_view (ug_data *ugd);

static char* trim_string (const char *str)
{
    Eina_Strbuf *buf = eina_strbuf_new ();
    char *result;

    eina_strbuf_append (buf, str);
    eina_strbuf_replace_all (buf, "\r", "");
    eina_strbuf_replace_all (buf, "\"", "");
    eina_strbuf_trim (buf);

    result = strdup (eina_strbuf_string_get (buf));

    eina_strbuf_free (buf);

    return result;
}

static void append_separator (Evas_Object *genlist, separator_type style_type)
{
    // Separator
    Elm_Object_Item *item;
    item = elm_genlist_item_append (
            genlist,                // genlist object
            &itcSeparator[style_type], // item class
            NULL,                   // data
            NULL,
            ELM_GENLIST_ITEM_NONE,
            NULL,
            NULL);
    elm_genlist_item_select_mode_set (item, ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY);
}

static Evas_Object *create_bg (Evas_Object *win)
{
    Evas_Object *bg = elm_bg_add (win);
    evas_object_size_hint_weight_set (bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_show (bg);
    return bg;
}

static Evas_Object *create_fullview (Evas_Object *parent, struct ug_data *ugd)
{
    Evas_Object *bg = create_bg (parent);
    Evas_Object *layout_main = elm_layout_add (parent);

    if (layout_main == NULL)
        return NULL;

    elm_layout_theme_set (layout_main, "layout", "application", "default");
    elm_object_style_set (bg, "group_list");
    elm_object_part_content_set (layout_main, "elm.swallow.bg", bg);

    return layout_main;
}

static Evas_Object *create_frameview (Evas_Object *parent, struct ug_data *ugd)
{
    Evas_Object *bg = create_bg (parent);
    Evas_Object *layout_main = elm_layout_add (parent);
    if (layout_main == NULL)
        return NULL;

    elm_layout_theme_set (layout_main, "layout", "application", "default");
    elm_object_style_set (bg, "group_list");
    elm_object_part_content_set (layout_main, "elm.swallow.bg", bg);
    return layout_main;
}

static Evas_Object *create_naviframe_layout (Evas_Object* parent)
{
    Evas_Object *naviframe = elm_naviframe_add (parent);
    elm_naviframe_prev_btn_auto_pushed_set (naviframe, EINA_FALSE);
    ea_object_event_callback_add (naviframe, EA_CALLBACK_BACK, ea_naviframe_back_cb, NULL);
    elm_object_part_content_set (parent, "elm.swallow.content", naviframe);
    evas_object_show (naviframe);

    return naviframe;
}

static Eina_Bool back_cb (void *data, Elm_Object_Item *it)
{
    static bool in_exit = false;

    if (in_exit)
        return EINA_TRUE;
    in_exit = true;

    if (data == NULL)
        return EINA_TRUE;

    struct ug_data *ugd = (struct ug_data *)data;
    ug_destroy_me (ugd->ug);

    return EINA_TRUE;
}

static void imeug_destroy_cb (ui_gadget_h ug, void *data)
{
    if (!ug) return;

    if (ug)
        ug_destroy (ug);
}

static void imeug_layout_cb(ui_gadget_h ug, enum ug_mode mode,
        void *priv)
{
    if (!ug) return;

    Evas_Object *base;

    base = (Evas_Object *) ug_get_layout(ug);
    if (!base)
        return;

    switch (mode) {
        case UG_MODE_FULLVIEW:
            evas_object_size_hint_weight_set(base, EVAS_HINT_EXPAND,
                    EVAS_HINT_EXPAND);
            evas_object_show(base);
            break;
        default:
            break;
    }
}

static void launch_setting_plugin_ug_for_ime_setting(const char *ise_uuid)
{
    String pkgname = String ("");
    String mdl_name = String("");
    for (unsigned int i = 0; i < _uuids.size (); i++) {
        if (_uuids[i] == String (ise_uuid)) {
            mdl_name = _module_names[i];
            ISFUG_DEBUG ("module name:%s", mdl_name.c_str());
            pkgname = mdl_name.substr (0, mdl_name.find_first_of ('.'));
            break;
        }
    }

    struct ug_cbs cbs = {0,};
    cbs.layout_cb  = imeug_layout_cb;
    cbs.result_cb  = NULL;
    cbs.destroy_cb = imeug_destroy_cb;
    cbs.priv = NULL;

    service_h service = NULL;
    service_create (&service);
    service_add_extra_data (service, "pkgname", pkgname.c_str ());
    ui_gadget_h ug = ug_create (NULL, "setting-plugin-efl", UG_MODE_FULLVIEW, service, &cbs);
    if (ug == NULL) {
        ISFUG_DEBUG ("fail to load setting-plugin-efl ug ");
    }
    else
        ISFUG_DEBUG ("load setting-plugin-efl ug successfully");

    service_destroy (service);
    service = NULL;

}

static int pkg_list_cb (pkgmgrinfo_appinfo_h handle, void *user_data)
{
    char *id = (char *)user_data;
    char *pkgid = NULL;
    if (pkgmgrinfo_pkginfo_get_pkgid (handle, &pkgid) != PMINFO_R_OK)
        return -1;

    if (strcmp (pkgid , id) == 0) {
        _ise_option_module_stat = ISE_OPTION_MODULE_EXIST_XML;
        ISFUG_DEBUG ("pkgid : %s\n", pkgid);
        return -1;
    }
    else {
        ISFUG_DEBUG ("%s", pkgid);
        return 0;
    }
}

static ISE_OPTION_MODULE_STATE find_ise_option_module (const char *ise_uuid)
{
    ISFUG_DEBUG ("%s", ise_uuid);

    String mdl_name;
    _ise_option_module_stat = ISE_OPTION_MODULE_NO_EXIST;

    for (unsigned int i = 0; i < _uuids.size (); i++) {
        if (_uuids[i] == String (ise_uuid)) {
            if (_modes[i] == TOOLBAR_KEYBOARD_MODE)
                mdl_name = _module_names[i] + String ("-imengine-setup");
            else
                mdl_name = _module_names[i] + String ("-setup");
        }
    }
    _mdl_name = mdl_name;
    for (unsigned int i = 0; i < _setup_modules.size (); i++) {
        if (mdl_name == _setup_modules[i]) {
            _ise_option_module_stat = ISE_OPTION_MODULE_EXIST_SO;
            goto result;
        }
    }

    //if not found , check if there's osp ime directory for it
    for (unsigned int i = 0; i < _uuids.size (); i++) {
        if (_uuids[i] == String (ise_uuid)) {
            int ret = 0;
            pkgmgrinfo_pkginfo_filter_h handle;
            ret = pkgmgrinfo_pkginfo_filter_create (&handle);
            if (ret != PMINFO_R_OK) {
                ISFUG_DEBUG ("pkgmgrinfo_appinfo_filter_create FAIL");
                goto result;
            }

            const bool support_appsetting = true;

            ret = pkgmgrinfo_pkginfo_filter_add_bool (handle, PMINFO_PKGINFO_PROP_PACKAGE_APPSETTING, support_appsetting);
            if (ret != PMINFO_R_OK) {
                pkgmgrinfo_pkginfo_filter_destroy (handle);
                ISFUG_DEBUG ("pkgmgrinfo_pkginfo_filter_add_bool FAIL");
                goto result;
            }

            //mdl name is equal the ime appid due to name rule, appid = pkgid.pkgname
            mdl_name = _module_names[i];
            String pkgid = mdl_name.substr (0, mdl_name.find_first_of ('.'));
            ret = pkgmgrinfo_pkginfo_filter_foreach_pkginfo (handle, pkg_list_cb, (void *)(pkgid.c_str ()));

            if (ret != PMINFO_R_OK) {
                pkgmgrinfo_pkginfo_filter_destroy (handle);
                ISFUG_DEBUG ("pkgmgrinfo_pkginfo_filter_foreach_appinfo FAIL");
                goto result;
            }

            pkgmgrinfo_pkginfo_filter_destroy(handle);
        }
    }

result:
    return _ise_option_module_stat;
}

static void set_autocap_mode (void)
{
    if (vconf_set_bool (VCONFKEY_AUTOCAPITAL_ALLOW_BOOL, _auto_capitalisation) == -1)
        std::cerr << "Failed to set vconf autocapital\n";
}

static void set_auto_full_stop_mode (void)
{
    if (vconf_set_bool (VCONFKEY_AUTOPERIOD_ALLOW_BOOL, _auto_full_stop) == -1)
        std::cerr << "Failed to set vconf autoperiod\n";
}

//hw sw ise may have the same name
static String sw_name_to_uuid (String name)
{
    String strUuid ("");
    for (unsigned int i = 0; i < _names.size (); i++) {
        if ((strcmp (name.c_str (), _names[i].c_str ()) == 0) && (_modes[i] == TOOLBAR_HELPER_MODE)) {
            strUuid = _uuids[i];
            break;
        }
    }
    return strUuid;
}

static String hw_name_to_uuid (String name)
{
    String strUuid ("");
    for (unsigned int i = 0; i < _names.size (); i++) {
        if ((strcmp (name.c_str (), _names[i].c_str ()) == 0) && (_modes[i] == TOOLBAR_KEYBOARD_MODE)) {
            strUuid = _uuids[i];
            break;
        }
    }
    return strUuid;
}

static String uuid_to_name (String uuid)
{
    String strName ("");
    for (unsigned int i = 0; i < _uuids.size (); i++) {
        if (strcmp (uuid.c_str (), _uuids[i].c_str ()) == 0) {
            strName = _names[i];
            break;
        }
    }
    return strName;
}

static void update_setting_main_view (ug_data *ugd)
{
    _p_items[SW_KEYBOARD_SEL_ITEM]->sub_text = strdup (_sw_ise_name);
    elm_object_item_data_set (ugd->sw_ise_item_tizen, _p_items[SW_KEYBOARD_SEL_ITEM]);
    if (_hw_kbd_connected || ISE_OPTION_MODULE_NO_EXIST == find_ise_option_module ((const char *)(sw_name_to_uuid (_sw_ise_name).c_str())))
        elm_object_item_disabled_set (ugd->sw_ise_opt_item_tizen, EINA_TRUE);
    else
        elm_object_item_disabled_set (ugd->sw_ise_opt_item_tizen, EINA_FALSE);
    _p_items[HW_KEYBOARD_SEL_ITEM]->sub_text = strdup (_hw_ise_name);
    elm_object_item_data_set (ugd->hw_ise_item_tizen, _p_items[HW_KEYBOARD_SEL_ITEM]);
    if (!_hw_kbd_connected ||ISE_OPTION_MODULE_NO_EXIST == find_ise_option_module ((const char *)(hw_name_to_uuid (_hw_ise_name).c_str())))
        elm_object_item_disabled_set (ugd->hw_ise_opt_item_tizen, EINA_TRUE);
    else
        elm_object_item_disabled_set (ugd->hw_ise_opt_item_tizen, EINA_FALSE);

    elm_genlist_item_update (ugd->sw_ise_item_tizen);
    elm_genlist_item_update (ugd->hw_ise_item_tizen);
}

static void set_active_sw_ise()
{
    if (strcmp (_sw_ise_bak, _sw_ise_name) != 0) {
        // If ISE is changed, active it.
        String uuid;
        for (unsigned int i = 0; i < _names.size (); i++) {
            if (strcmp (_names[i].c_str (), _sw_ise_name) == 0)
                uuid = _uuids[i];
        }
        isf_control_set_active_ise_by_uuid (uuid.c_str ());

        snprintf (_sw_ise_bak, sizeof (_sw_ise_bak), "%s", _sw_ise_name);
    }
}

static Eina_Bool sw_keyboard_selection_view_set_cb (void *data, Elm_Object_Item *it)
{
    if (data == NULL)
        return EINA_TRUE;

    struct ug_data *ugd = (struct ug_data *)data;

    set_active_sw_ise();

    update_setting_main_view (ugd);

    ugd->key_end_cb = back_cb;

    return EINA_TRUE;
}

static void sw_keyboard_radio_cb (void *data, Evas_Object *obj, void *event_info)
{
    int index = GPOINTER_TO_INT(data);
    elm_radio_value_set (_sw_radio_grp, index);
    snprintf (_sw_ise_name, sizeof (_sw_ise_name), "%s", _sw_ise_list[index].c_str ());
}

static void hw_keyboard_radio_cb (void *data, Evas_Object *obj, void *event_info)
{
    int index = GPOINTER_TO_INT(data);
    elm_radio_value_set (_hw_radio_grp, index);
    snprintf (_hw_ise_name, sizeof (_hw_ise_name), "%s", _hw_ise_list[index].c_str ());
}

static Eina_Bool language_view_back_cb (void *data, Elm_Object_Item *it)
{
    _common_ugd->key_end_cb = sw_keyboard_selection_view_set_cb;
    return EINA_TRUE;
}

static void show_language_cb (void *data, Evas_Object *obj, void *event_info)
{
    int index = GPOINTER_TO_INT(data);
    String langlist_str, normal_langlist_str;

    for (unsigned int i = 0; i < _names.size (); i++) {
        if (strcmp (_names[i].c_str (), _sw_ise_list[index].c_str ()) == 0)
            langlist_str = _langs[i];
    }
    std::vector<String> langlist_vec,normal_langlist_vec;
    scim_split_string_list (langlist_vec, langlist_str);
    normal_langlist_str = ((scim_get_language_name (langlist_vec[0].c_str ())).c_str ());
    for (unsigned int i = 1; i < langlist_vec.size (); i++) {
        normal_langlist_str += String (", ");
        normal_langlist_str += ((scim_get_language_name (langlist_vec[i].c_str ())).c_str ());
    }

    Evas_Object* layout = elm_layout_add (_common_ugd->naviframe);
    elm_layout_file_set (layout, ISF_SETTING_EDJ, "isfsetting/languageview");

    Evas_Object *scroller = elm_scroller_add (layout);
    elm_scroller_bounce_set (scroller, EINA_FALSE, EINA_FALSE);
    evas_object_size_hint_weight_set (scroller, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

    Evas_Object *label = elm_label_add (layout);
    elm_label_line_wrap_set (label, ELM_WRAP_WORD);

    Evas_Coord win_w = 0, win_h = 0;
    ecore_x_window_size_get (ecore_x_window_root_first_get (), &win_w, &win_h);
    elm_label_wrap_width_set (label, win_w - PADDING_X * 2);
    elm_object_text_set (label, normal_langlist_str.c_str ());
    elm_object_content_set (scroller, label);
    elm_object_part_content_set (layout, "content", scroller);

    // Push the layout along with function buttons and title
    Elm_Object_Item *it = elm_naviframe_item_push (_common_ugd->naviframe, _sw_ise_list[index].c_str (), NULL, NULL, layout, NULL);
    elm_naviframe_item_pop_cb_set (it, language_view_back_cb, NULL);

    _common_ugd->key_end_cb = language_view_back_cb;
}

static void helper_ise_reload_config (void)
{
    String display_name = String (":13");
    const char *p = getenv ("DISPLAY");
    if (p != NULL)
        display_name = String (p);
    HelperAgent helper_agent;
    HelperInfo  helper_info ("fd491a70-22f5-11e2-89f3-eb5999be869e", "ISF Setting", "", "", SCIM_HELPER_STAND_ALONE);
    int id = helper_agent.open_connection (helper_info, display_name);
    if (id == -1) {
        std::cerr << "    open_connection failed!!!!!!\n";
    } else {
        helper_agent.reload_config ();
        helper_agent.close_connection ();
    }
}

static Eina_Bool ise_option_view_set_cb (void *data, Elm_Object_Item *it)
{
    if (!data)
        return EINA_TRUE;

    struct ug_data *ugd = (struct ug_data *)data;
    ugd->key_end_cb = back_cb;
    _mdl->save_config (_config);

    helper_ise_reload_config ();

    return EINA_TRUE;
}

static void ise_option_show (ug_data *ugd, const char *ise_uuid)
{
    if (ISE_OPTION_MODULE_EXIST_SO == find_ise_option_module (ise_uuid)) {
        char title[256];
        snprintf (title, sizeof (title), _T("Keyboard settings"));

        if (_mdl) {
            delete _mdl;
            _mdl = NULL;
        }

        if (_mdl_name.length () > 0)
            _mdl = new SetupModule (String (_mdl_name));

        if (_mdl == NULL || !_mdl->valid ()) {
            return;
        } else {
            _mdl->load_config (_config);
            ugd->opt_eo = _mdl->create_ui (ugd->layout_main, ugd->naviframe);

            Elm_Object_Item *it = elm_naviframe_item_push (ugd->naviframe, title, NULL, NULL, ugd->opt_eo, NULL);
            elm_naviframe_item_pop_cb_set (it, ise_option_view_set_cb, ugd);

            ugd->key_end_cb = ise_option_view_set_cb;
        }
    }
    else if (ISE_OPTION_MODULE_EXIST_XML == find_ise_option_module (ise_uuid)) {
        launch_setting_plugin_ug_for_ime_setting (ise_uuid);
    }
}

static char *_gl_text_get (void *data, Evas_Object *obj, const char *part)
{
    ItemData *item_data = (ItemData *)data;

    if (item_data)
        return strdup (item_data->text);
    else
        return strdup ("");
}

static char *_gl_label_get (void *data, Evas_Object *obj, const char *part)
{
    ItemData *item_data = (ItemData *)data;
    if (!strcmp (part, "elm.text")) {
        return strdup (item_data->text);
    }
    if (!strcmp (part, "elm.text.1")) {
        return strdup (item_data->text);
    }
    if (!strcmp (part, "elm.text.2")) {
        return strdup (item_data->sub_text);
    }
    return NULL;
}

static void onoff_check_cb (void *data, Evas_Object *obj, void *event_info)
{
    int id = (int)(data);

    if (id == AUTO_CAPITALIZATION_ITEM) {
        _auto_capitalisation = (_auto_capitalisation == EINA_TRUE ? EINA_FALSE : EINA_TRUE);
        set_autocap_mode ();
    } else if (id == AUTO_FULL_STOP_ITEM) {
        _auto_full_stop = (_auto_full_stop == EINA_TRUE ? EINA_FALSE : EINA_TRUE);
        set_auto_full_stop_mode ();
    }
}

static Evas_Object *_gl_icon_get (void *data, Evas_Object *obj, const char *part)
{
    ItemData *item_data = (ItemData *)data;

    if (!strcmp (part, "elm.icon")) {
        Evas_Object *onoff_ck = elm_check_add (obj);
        elm_object_style_set (onoff_ck, "on&off");
        evas_object_smart_callback_add (onoff_ck, "changed", onoff_check_cb, (void *) (item_data->mode));
        evas_object_propagate_events_set (onoff_ck, EINA_FALSE);
        if (item_data->mode == AUTO_CAPITALIZATION_ITEM) {
            elm_check_state_set (onoff_ck, _auto_capitalisation);
        } else if (item_data->mode == AUTO_FULL_STOP_ITEM) {
            elm_check_state_set (onoff_ck, _auto_full_stop);
        }
        return onoff_ck;
    }

    return NULL;
}

static Eina_Bool _gl_state_get (void *data, Evas_Object *obj, const char *part)
{
    return EINA_FALSE;
}

static void _gl_del (void *data, Evas_Object *obj)
{
}

static void _gl_sw_ise_del (void *data, Evas_Object *obj)
{
    _sw_radio_grp = NULL;
}

static void _gl_sw_ise_sel (void *data, Evas_Object *obj, void *event_info)
{
    Elm_Object_Item *item = (Elm_Object_Item *)event_info;
    elm_genlist_item_selected_set (item, EINA_FALSE);

    _sw_ise_index = (int)(data);
    snprintf (_sw_ise_name, sizeof (_sw_ise_name), "%s", _sw_ise_list[_sw_ise_index].c_str ());

    elm_genlist_item_update (item);
}

static void _gl_hw_ise_sel (void *data, Evas_Object *obj, void *event_info)
{
    Elm_Object_Item *item = (Elm_Object_Item *)event_info;
    elm_genlist_item_selected_set (item, EINA_FALSE);

    _hw_ise_index = (int)(data);
    snprintf (_hw_ise_name, sizeof (_hw_ise_name), "%s", _hw_ise_list[_hw_ise_index].c_str ());

    elm_genlist_item_update (item);
}

static void _gl_sel (void *data, Evas_Object *obj, void *event_info)
{
    Elm_Object_Item *item = (Elm_Object_Item *)event_info;
    elm_genlist_item_selected_set (item, EINA_FALSE);

    int id = (int)(data);

    if (id == AUTO_CAPITALIZATION_ITEM) {
        _auto_capitalisation = (_auto_capitalisation == EINA_TRUE ? EINA_FALSE : EINA_TRUE);
        set_autocap_mode ();
    } else if (id == AUTO_FULL_STOP_ITEM) {
        _auto_full_stop = (_auto_full_stop == EINA_TRUE ? EINA_FALSE : EINA_TRUE);
        set_auto_full_stop_mode ();
    }

    elm_genlist_item_update (item);
}

static void _gl_keyboard_sel (void *data, Evas_Object *obj, void *event_info)
{
    Elm_Object_Item *item = (Elm_Object_Item *)event_info;
    ug_data *ugd = (ug_data *)data;
    if (item == ugd->sw_ise_item_tizen)
        create_sw_keyboard_selection_view (ugd);
    if (item == ugd->hw_ise_item_tizen)
        create_hw_keyboard_selection_view (ugd);
    elm_genlist_item_selected_set (item, EINA_FALSE);
}

static void _gl_ise_option_sel (void *data, Evas_Object *obj, void *event_info)
{
    Elm_Object_Item *item = (Elm_Object_Item *)event_info;
    ug_data *ugd = (ug_data *)data;
    String ise_uuid = String ("");
    if (item == ugd->sw_ise_opt_item_tizen)
        ise_uuid = sw_name_to_uuid (_sw_ise_name);
    else
        ise_uuid = hw_name_to_uuid (_hw_ise_name);
    ise_option_show (ugd, ise_uuid.c_str ());
    elm_genlist_item_selected_set (item, EINA_FALSE);
}

static char *_gl_exp_sw_label_get (void *data, Evas_Object *obj, const char *part)
{
    int index = (int)(data);
    if (!strcmp (part, "elm.text")) {
        return strdup (_sw_ise_list[index].c_str ());
    }
    return NULL;
}

static Evas_Object *_gl_exp_sw_icon_get (void *data, Evas_Object *obj, const char *part)
{
    int index = (int)(data);
    if (!strcmp (part, "elm.icon.1")) {
        Evas_Object *radio = elm_radio_add (obj);
        elm_radio_state_value_set (radio, index);
        if (_sw_radio_grp == NULL)
            _sw_radio_grp = elm_radio_add (obj);
        elm_radio_group_add (radio, _sw_radio_grp);
        evas_object_show (radio);
        evas_object_smart_callback_add (radio, "changed", sw_keyboard_radio_cb, (void *) (index));
        if (_sw_ise_index == index) {
            elm_radio_value_set (_sw_radio_grp, _sw_ise_index);
        }
        return radio;
    }
    if (!strcmp (part, "elm.icon.2")) {
        Evas_Object *icon = elm_button_add (obj);
        elm_object_style_set (icon, "reveal");
        evas_object_smart_callback_add (icon, "clicked", show_language_cb, (void *)(index));
        evas_object_propagate_events_set (icon, EINA_FALSE); // Not propagate to genlist item.
        return icon;
    }
    return NULL;
}

static void _gl_exp_sw_realized (void *data, Evas_Object *obj, void *event_info)
{
    Elm_Object_Item *it = (Elm_Object_Item *)event_info;
    int index;

    if (!it) return;

    index = (int)elm_object_item_data_get (it);

    if (_sw_ise_list.size () < 2)
        return;

    if (index == 0)
        elm_object_item_signal_emit (it, "elm,state,top", "");
    else if (index == (int)(_sw_ise_list.size () - 1))
        elm_object_item_signal_emit (it, "elm,state,bottom", "");
    else
        elm_object_item_signal_emit (it, "elm,state,center", "");
}

static char *_gl_exp_hw_label_get (void *data, Evas_Object *obj, const char *part)
{
    int index = (int)(data);
    if (!strcmp (part, "elm.text")) {
        return strdup (_hw_ise_list[index].c_str ());
    }
    return NULL;
}

static Evas_Object *_gl_exp_hw_icon_get (void *data, Evas_Object *obj, const char *part)
{
    if (!strcmp (part, "elm.icon")) {
        int index = (int)(data);
        Evas_Object *radio = elm_radio_add (obj);
        elm_radio_state_value_set (radio, index);
        if (_hw_radio_grp == NULL)
            _hw_radio_grp = elm_radio_add (obj);
        elm_radio_group_add (radio, _hw_radio_grp);
        evas_object_show (radio);
        evas_object_smart_callback_add (radio, "changed", hw_keyboard_radio_cb, (void *) (index));
        if (_hw_ise_index == index) {
            elm_radio_value_set (_hw_radio_grp, _hw_ise_index);
        }
        return radio;
    }
    return NULL;
}

static void _gl_exp_hw_realized (void *data, Evas_Object *obj, void *event_info)
{
    Elm_Object_Item *it = (Elm_Object_Item *)event_info;
    int index;

    if (!it) return;

    index = (int)elm_object_item_data_get (it);

    if (_hw_ise_list.size () < 2)
        return;

    if (index == 0)
        elm_object_item_signal_emit (it, "elm,state,top", "");
    else if (index == (int)(_hw_ise_list.size () - 1))
        elm_object_item_signal_emit (it, "elm,state,bottom", "");
    else
        elm_object_item_signal_emit (it, "elm,state,center", "");
}

static void _gl_realized (void *data, Evas_Object *obj, void *event_info)
{
    Elm_Object_Item *it = (Elm_Object_Item *)event_info;
    ItemData *item_data;

    if (!it) return;

    item_data = (ItemData *)elm_object_item_data_get (it);
    if (!item_data) return;

    switch (item_data->item_style_type) {
        case ITEM_STYLE_TOP:
            elm_object_item_signal_emit (it, "elm,state,top", "");
            break;
        case ITEM_STYLE_BOTTOM:
            elm_object_item_signal_emit (it, "elm,state,bottom", "");
            break;
        case ITEM_STYLE_CENTER:
            elm_object_item_signal_emit (it, "elm,state,center", "");
            break;
        default:
            break;
    }
}

static void create_sw_keyboard_selection_view (ug_data *ugd)
{
    ugd->key_end_cb = sw_keyboard_selection_view_set_cb;

    if (_sw_radio_grp != NULL) {
        evas_object_del (_sw_radio_grp);
        _sw_radio_grp = NULL;
    }

    Evas_Object *genlist = elm_genlist_add (ugd->naviframe);
    evas_object_smart_callback_add (genlist, "realized", _gl_exp_sw_realized, NULL);
    elm_object_style_set (genlist, "dialogue");
    evas_object_show (genlist);

    // Push the layout along with function buttons and title
    Elm_Object_Item *it = elm_naviframe_item_push (ugd->naviframe, _T("Keyboard selection"), NULL, NULL, genlist, NULL);
    elm_naviframe_item_pop_cb_set (it, sw_keyboard_selection_view_set_cb, ugd);

    unsigned int i = 0;

    std::sort (_sw_ise_list.begin (), _sw_ise_list.end ());

    if (_sw_ise_list.size () > 0) {
        // Separator
        append_separator (genlist, SEPARATOR_TYPE1);
    }

    for (i = 0; i < _sw_ise_list.size (); i++) {
        if (strcmp (_sw_ise_name, _sw_ise_list[i].c_str ()) == 0) {
            _sw_ise_index = i;
            break;
        }
    }

    // Set item class for text + radio button
    itc4.item_style       = "dialogue/1text.2icon.2";
    itc4.func.text_get    = _gl_exp_sw_label_get;
    itc4.func.content_get = _gl_exp_sw_icon_get;
    itc4.func.state_get   = _gl_state_get;
    itc4.func.del         = _gl_sw_ise_del;
    for (i = 0; i < _sw_ise_list.size (); i++) {
        elm_genlist_item_append (genlist, &itc4, (void *)(i), NULL, ELM_GENLIST_ITEM_NONE, _gl_sw_ise_sel, (void *)(i));
    }
}

static void set_active_hw_ise (void)
{
    if (strcmp (_hw_ise_bak, _hw_ise_name) != 0) {
        // If ISE is changed, active it.
        String uuid;
        for (unsigned int i = 0; i < _names.size (); i++) {
            if (strcmp (_names[i].c_str (), _hw_ise_name) == 0)
                uuid = _uuids[i];
        }
        isf_control_set_active_ise_by_uuid (uuid.c_str ());
        //printf ("    Set keyboard ISE: %s\n", _hw_ise_name);
        snprintf (_hw_ise_bak, sizeof (_hw_ise_bak), "%s", _hw_ise_name);
    }
}

static Eina_Bool hw_keyboard_selection_view_set_cb (void *data, Elm_Object_Item *it)
{
    if (!data)
        return EINA_TRUE;

    struct ug_data *ugd = (struct ug_data *)data;

    set_active_hw_ise ();

    update_setting_main_view (ugd);

    ugd->key_end_cb = back_cb;

    return EINA_TRUE;
}

static void create_hw_keyboard_selection_view (ug_data * ugd)
{
    ugd->key_end_cb = hw_keyboard_selection_view_set_cb;

    if (_hw_radio_grp != NULL) {
        evas_object_del (_hw_radio_grp);
        _hw_radio_grp = NULL;
    }

    Evas_Object *genlist = elm_genlist_add (ugd->naviframe);
    elm_object_style_set (genlist, "dialogue");
    evas_object_smart_callback_add (genlist, "realized", _gl_exp_hw_realized, NULL);
    evas_object_show (genlist);

    // Push the layout along with function buttons and title
    Elm_Object_Item *nf_it = elm_naviframe_item_push (ugd->naviframe, _T("Keyboard selection"), NULL, NULL, genlist, NULL);
    elm_naviframe_item_pop_cb_set (nf_it, hw_keyboard_selection_view_set_cb, ugd);

    unsigned int i = 0;

    if (_hw_ise_list.size () > 0) {
        // Seperator
        append_separator (genlist, SEPARATOR_TYPE1);
    }

    std::sort (_hw_ise_list.begin (), _hw_ise_list.end ());

    for (i = 0; i < _hw_ise_list.size (); i++) {
        if (strcmp (_hw_ise_name, _hw_ise_list[i].c_str ()) == 0) {
            _hw_ise_index = i;
            break;
        }
    }

    // Set item class for text + radio button)
    itc5.item_style       = "dialogue/1text.1icon.2";
    itc5.func.text_get    = _gl_exp_hw_label_get;
    itc5.func.content_get = _gl_exp_hw_icon_get;
    itc5.func.state_get   = _gl_state_get;
    itc5.func.del         = _gl_del;
    for (i = 0; i < _hw_ise_list.size (); i++) {
        elm_genlist_item_append (genlist, &itc5, (void *)(i), NULL, ELM_GENLIST_ITEM_NONE, _gl_hw_ise_sel, (void *)(i));
    }
}

static Elm_Object_Item *nf_main_it = NULL;
static Evas_Object *create_setting_main_view (ug_data *ugd)
{
    Elm_Object_Item *item     = NULL;
    Eina_Bool        fullstop = EINA_FALSE;

    if (ugd->naviframe == NULL) {
        ugd->naviframe  = create_naviframe_layout (ugd->layout_main);
        ugd->key_end_cb = back_cb;

        Evas_Object *genlist = elm_genlist_add (ugd->naviframe);
        elm_object_style_set (genlist, "dialogue");
        evas_object_smart_callback_add (genlist, "realized", _gl_realized, NULL);
        elm_genlist_mode_set (genlist, ELM_LIST_COMPRESS);
        // Set item class for 1text.1icon(text+radiobutton)
        itc1.item_style       = "dialogue/1text.1icon";
        itc1.func.text_get    = _gl_label_get;
        itc1.func.content_get = _gl_icon_get;
        itc1.func.state_get   = _gl_state_get;
        itc1.func.del         = _gl_del;

        // Set item class for 2text(text+subtext)
        itc2.item_style       = "dialogue/2text.3";
        itc2.func.text_get    = _gl_label_get;
        itc2.func.content_get = NULL;
        itc2.func.state_get   = _gl_state_get;
        itc2.func.del         = _gl_del;

        // Set item class for normal text(text)
        itc3.item_style       = "dialogue/1text";
        itc3.func.text_get    = _gl_label_get;
        itc3.func.content_get = NULL;
        itc3.func.state_get   = _gl_state_get;
        itc3.func.del         = _gl_del;

        // Set item class for dialogue group seperator
        itcSeparator[SEPARATOR_TYPE1].item_style       = "dialogue/separator";
        itcSeparator[SEPARATOR_TYPE1].func.text_get    = NULL;
        itcSeparator[SEPARATOR_TYPE1].func.content_get = NULL;
        itcSeparator[SEPARATOR_TYPE1].func.state_get   = NULL;
        itcSeparator[SEPARATOR_TYPE1].func.del         = NULL;

        itcSeparator[SEPARATOR_TYPE2].item_style       = "dialogue/separator.2";
        itcSeparator[SEPARATOR_TYPE2].func.text_get    = NULL;
        itcSeparator[SEPARATOR_TYPE2].func.content_get = NULL;
        itcSeparator[SEPARATOR_TYPE2].func.state_get   = NULL;
        itcSeparator[SEPARATOR_TYPE2].func.del         = NULL;

        // Set item class for dialogue group title
        itcTitle.item_style       = "dialogue/grouptitle";
        itcTitle.func.text_get    = _gl_label_get;
        itcTitle.func.content_get = NULL;
        itcTitle.func.state_get   = _gl_state_get;
        itcTitle.func.del         = _gl_del;

        itcText.item_style       = "multiline/1text";
        itcText.func.text_get    = _gl_text_get;
        itcText.func.content_get = NULL;
        itcText.func.state_get   = _gl_state_get;
        itcText.func.del         = _gl_del;

        //==================================group begin =======================
        ItemData *item_data = NULL;

        // Separator
        append_separator (genlist, SEPARATOR_TYPE1);

        // Group1 item1
        item_data = (ItemData *)malloc (sizeof (ItemData));
        if (item_data != NULL) {
            memset (item_data, 0, sizeof (ItemData));
            _p_items[AUTO_CAPITALIZATION_ITEM] = item_data;
            item_data->text = strdup (_T("Auto capitalization"));
            item_data->mode = AUTO_CAPITALIZATION_ITEM;
            ugd->autocapital_item = elm_genlist_item_append (
                    genlist,                // genlist object
                    &itc1,                  // item class
                    item_data,              // data
                    NULL,
                    ELM_GENLIST_ITEM_NONE,
                    _gl_sel,
                    (void *)(item_data->mode));

            if (_hw_kbd_connected)
                elm_object_item_disabled_set (ugd->autocapital_item, EINA_TRUE);
        }

        // Separator
        append_separator (genlist, SEPARATOR_TYPE2);

        // Text
        item_data = (ItemData *)malloc (sizeof (ItemData));
        if (item_data != NULL) {
            memset (item_data, 0, sizeof (ItemData));
            _p_items[AUTO_CAPITALIZATION_TXT_ITEM] = item_data;
            item_data->text = strdup (_T("Automatically capitalize first letter of sentence"));
            item = elm_genlist_item_append (
                    genlist,                                // genlist object
                    &itcText,                               // item class
                    item_data,                              // data
                    NULL,
                    ELM_GENLIST_ITEM_NONE,
                    NULL,
                    NULL);
            elm_genlist_item_select_mode_set (item, ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY);
        }

        char *env = getenv ("ISF_AUTOFULLSTOP");
        if (env)
            fullstop = !!atoi (env);

        if (fullstop) {
            // Separator
            append_separator (genlist, SEPARATOR_TYPE1);

            // Group1 item2
            item_data = (ItemData *)malloc (sizeof (ItemData));
            if (item_data != NULL) {
                memset (item_data, 0, sizeof (ItemData));
                _p_items[AUTO_FULL_STOP_ITEM] = item_data;
                item_data->text = strdup (_T("Automatic full stop"));
                item_data->mode = AUTO_FULL_STOP_ITEM;
                elm_genlist_item_append (
                        genlist,                // genlist object
                        &itc1,                  // item class
                        item_data,              // data
                        NULL,
                        ELM_GENLIST_ITEM_NONE,
                        _gl_sel,
                        (void *)(item_data->mode));
            }

            // Separator
            append_separator (genlist, SEPARATOR_TYPE2);

            // Text
            item_data = (ItemData *)malloc (sizeof (ItemData));
            if (item_data != NULL) {
                memset (item_data, 0, sizeof (ItemData));
                _p_items[AUTO_FULL_STOP_TXT_ITEM] = item_data;
                item_data->text = strdup (_T("Automatically insert a full stop by tapping the space bar twice"));
                item = elm_genlist_item_append (
                        genlist,                            // genlist object
                        &itcText,                           // item class
                        item_data,                          // data
                        NULL,
                        ELM_GENLIST_ITEM_NONE,
                        NULL,
                        NULL);
                elm_genlist_item_select_mode_set (item, ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY);
            }
        }

        // Separator
        append_separator (genlist, SEPARATOR_TYPE1);

        // Group2 title
        item_data = (ItemData *)malloc (sizeof (ItemData));
        if (item_data != NULL) {
            memset (item_data, 0, sizeof (ItemData));
            _p_items[SW_KEYBOARD_GROUP_TITLE_ITEM] = item_data;
            item_data->text = strdup (_T("Software keyboard"));
            item = elm_genlist_item_append (
                    genlist,                // genlist object
                    &itcTitle,              // item class
                    item_data,              // data
                    NULL,
                    ELM_GENLIST_ITEM_NONE,
                    NULL,
                    NULL);
            elm_genlist_item_select_mode_set (item, ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY);
        }

        // Group2 item1
        std::vector<String> ise_names;

        String ise_uuid = _config->read (SCIM_CONFIG_DEFAULT_HELPER_ISE, String (""));
        snprintf (_sw_ise_name, sizeof (_sw_ise_name), "%s", (uuid_to_name (ise_uuid)).c_str ());
        snprintf (_sw_ise_bak, sizeof (_sw_ise_bak), "%s", _sw_ise_name);

        item_data = (ItemData *)malloc (sizeof (ItemData));
        if (item_data != NULL) {
            memset (item_data, 0, sizeof (ItemData));
            _p_items[SW_KEYBOARD_SEL_ITEM] = item_data;
            item_data->text = strdup (_T("Keyboard selection"));
            item_data->sub_text = strdup (_sw_ise_name);
            item_data->item_style_type = ITEM_STYLE_TOP;

            ugd->sw_ise_item_tizen = elm_genlist_item_append (
                    genlist,                // genlist object
                    &itc2,                  // item class
                    item_data,              // data
                    NULL,
                    ELM_GENLIST_ITEM_NONE,
                    _gl_keyboard_sel,
                    (void *)ugd);

            if (_hw_kbd_connected)
                elm_object_item_disabled_set (ugd->sw_ise_item_tizen, EINA_TRUE);
        }

        // Group2 item2
        item_data = (ItemData *)malloc (sizeof (ItemData));
        if (item_data != NULL) {
            memset (item_data, 0, sizeof (ItemData));
            _p_items[SW_ISE_OPTION_ITEM] = item_data;
            item_data->text = strdup (_T("Keyboard settings"));
            item_data->mode = SW_ISE_OPTION_ITEM;
            item_data->item_style_type = ITEM_STYLE_BOTTOM;
            ugd->sw_ise_opt_item_tizen = elm_genlist_item_append (
                    genlist,                // genlist object
                    &itc3,                  // item class
                    item_data,              // data
                    NULL,
                    ELM_GENLIST_ITEM_NONE,
                    _gl_ise_option_sel,
                    (void *)ugd);

            if (_hw_kbd_connected ||ISE_OPTION_MODULE_NO_EXIST == find_ise_option_module ((const char *)(sw_name_to_uuid (_sw_ise_name).c_str())))
                elm_object_item_disabled_set (ugd->sw_ise_opt_item_tizen, EINA_TRUE);
        }

        // Separator
        append_separator (genlist, SEPARATOR_TYPE1);

        // Group3 title
        item_data = (ItemData *)malloc (sizeof (ItemData));
        if (item_data != NULL) {
            memset (item_data, 0, sizeof (ItemData));
            _p_items[HW_KEYBOARD_GROUP_TITLE_ITEM] = item_data;
            item_data->text = strdup (_T("Hardware keyboard"));
            item = elm_genlist_item_append (
                    genlist,                // genlist object
                    &itcTitle,              // item class
                    item_data,              // data
                    NULL,
                    ELM_GENLIST_ITEM_NONE,
                    NULL,
                    NULL);
            elm_genlist_item_select_mode_set (item, ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY);
        }

        // Group3 item1
        uint32 option = 0;
        String uuid, name;
        isf_get_keyboard_ise (_config, uuid, name, option);
        snprintf (_hw_ise_name, sizeof (_hw_ise_name), "%s", name.c_str ());
        snprintf (_hw_ise_bak, sizeof (_hw_ise_bak), "%s", _hw_ise_name);

        if (option & SCIM_IME_NOT_SUPPORT_HARDWARE_KEYBOARD) {
            std::cerr << "    Keyboard ISE (" << _hw_ise_name << ") can not support hardware keyboard!!!\n";

            uuid = String (SCIM_COMPOSE_KEY_FACTORY_UUID);
            isf_get_keyboard_ise (_config, uuid, name, option);
            snprintf (_hw_ise_name, sizeof (_hw_ise_name), "%s", name.c_str ());
        }

        item_data = (ItemData *)malloc (sizeof (ItemData));
        if (item_data != NULL) {
            memset (item_data, 0, sizeof (ItemData));
            _p_items[HW_KEYBOARD_SEL_ITEM] = item_data;
            item_data->text     = strdup (_T("Keyboard selection"));
            item_data->sub_text = strdup (_hw_ise_name);
            item_data->item_style_type = ITEM_STYLE_TOP;
            ugd->hw_ise_item_tizen = elm_genlist_item_append (
                    genlist,                // genlist object
                    &itc2,                  // item class
                    item_data,              // data
                    NULL,
                    ELM_GENLIST_ITEM_NONE,
                    _gl_keyboard_sel,
                    (void *)ugd);

            if (!_hw_kbd_connected)
                elm_object_item_disabled_set (ugd->hw_ise_item_tizen, EINA_TRUE);
        }
        // Group3 item2
        item_data = (ItemData *)malloc (sizeof (ItemData));
        if (item_data != NULL) {
            memset (item_data, 0, sizeof (ItemData));
            _p_items[HW_ISE_OPTION_ITEM] = item_data;
            item_data->text = strdup (_T("Keyboard settings"));
            item_data->mode = HW_ISE_OPTION_ITEM;
            item_data->item_style_type = ITEM_STYLE_BOTTOM;
            ugd->hw_ise_opt_item_tizen = elm_genlist_item_append (
                    genlist,                // genlist object
                    &itc3,                  // item class
                    item_data,              // data
                    NULL,
                    ELM_GENLIST_ITEM_NONE,
                    _gl_ise_option_sel,
                    (void *)ugd);

            if (!_hw_kbd_connected || ISE_OPTION_MODULE_NO_EXIST == find_ise_option_module ((const char *)(hw_name_to_uuid (_hw_ise_name).c_str())))
                elm_object_item_disabled_set (ugd->hw_ise_opt_item_tizen, EINA_TRUE);
        }

        // Separator
        append_separator (genlist, SEPARATOR_TYPE1);

        //==================================group end =========================
        Evas_Object *back_btn = elm_button_add (ugd->naviframe);
        elm_object_style_set (back_btn, "naviframe/back_btn/default");
        nf_main_it = elm_naviframe_item_push (ugd->naviframe, _T("Keyboard"), back_btn, NULL, genlist, NULL);
        elm_naviframe_item_pop_cb_set (nf_main_it, back_cb, ugd);
    }
    return ugd->naviframe;
}

static void update_ise_list (void)
{
    // Request ISF to update ISE list, below codes are very important, donot remove
    char **iselist = NULL;
    int count = isf_control_get_ise_list (&iselist);

    for (int i = 0; i < count; i++) {
        SCIM_DEBUG_MAIN (3) << " [" << i << " : " << iselist[i] << "] \n";
        delete [] (iselist[i]);
    }
    if (iselist != NULL)
        free (iselist);
}

static void load_config_data (ConfigPointer config)
{
    int tmp_cap    = 0;
    int tmp_period = 0;
    vconf_get_bool (VCONFKEY_AUTOCAPITAL_ALLOW_BOOL, &tmp_cap);
    vconf_get_bool (VCONFKEY_AUTOPERIOD_ALLOW_BOOL, &tmp_period);
    if (tmp_cap == true)
        _auto_capitalisation = EINA_TRUE;
    if (tmp_period == true)
        _auto_full_stop = EINA_TRUE;
}

ConfigPointer isf_imf_context_get_config (void);
static void load_config_module (void)
{
    _config = isf_imf_context_get_config ();
    if (_config.null ()) {
        std::cerr << "Create dummy config!!!\n";
        _config = new DummyConfig ();
    }

    if (_config.null ()) {
        std::cerr << "Can not create Config Object!\n";
    }
}

static void hw_connection_change_cb (ug_data *ugd)
{
    // enable/disable switch
    elm_object_item_disabled_set (ugd->autocapital_item, !elm_object_item_disabled_get (ugd->autocapital_item));
    elm_object_item_disabled_set (ugd->sw_ise_item_tizen, !elm_object_item_disabled_get (ugd->sw_ise_item_tizen));
    elm_object_item_disabled_set (ugd->hw_ise_item_tizen, !elm_object_item_disabled_get (ugd->hw_ise_item_tizen));
    if (_hw_kbd_connected || ISE_OPTION_MODULE_NO_EXIST == find_ise_option_module ((const char *)(sw_name_to_uuid (_sw_ise_name).c_str())))
        elm_object_item_disabled_set (ugd->sw_ise_opt_item_tizen, EINA_TRUE);
    else
        elm_object_item_disabled_set (ugd->sw_ise_opt_item_tizen, EINA_FALSE);

    if (!_hw_kbd_connected || ISE_OPTION_MODULE_NO_EXIST == find_ise_option_module ((const char *)(hw_name_to_uuid (_hw_ise_name).c_str())))
        elm_object_item_disabled_set (ugd->hw_ise_opt_item_tizen, EINA_TRUE);
    else
        elm_object_item_disabled_set (ugd->hw_ise_opt_item_tizen, EINA_FALSE);

    if (!_hw_kbd_connected) {
        String uuid;
        for (unsigned int i = 0; i < _names.size (); i++) {
            if (strcmp (_names[i].c_str (), _sw_ise_name) == 0)
                uuid = _uuids[i];
        }
        isf_control_set_active_ise_by_uuid (uuid.c_str ());
    }
}

static Eina_Bool x_window_property_change_cb (void *data, int ev_type, void *ev)
{
    Ecore_X_Event_Window_Property *event = (Ecore_X_Event_Window_Property *)ev;
    unsigned int val = 0;

    if (event->win != _root_win || event->atom != _prop_x_ext_keyboard_exist)
        return ECORE_CALLBACK_PASS_ON;

    if (!ecore_x_window_prop_card32_get (event->win, _prop_x_ext_keyboard_exist, &val, 1) > 0)
        return ECORE_CALLBACK_PASS_ON;

    if (val != 0)
        _hw_kbd_connected = true;
    else
        _hw_kbd_connected = false;

    ug_data *ugd = (ug_data *)data;
    hw_connection_change_cb (ugd);
    _hw_kbd_num = val;

    return ECORE_CALLBACK_PASS_ON;
}

static void init_hw_keyboard_listener (ug_data *ugd)
{
    if (_prop_change_handler)
        return;

    _root_win = ecore_x_window_root_first_get ();
    ecore_x_event_mask_set (_root_win, ECORE_X_EVENT_MASK_WINDOW_PROPERTY);

    _prop_change_handler = ecore_event_handler_add (ECORE_X_EVENT_WINDOW_PROPERTY, x_window_property_change_cb, (void *)ugd);

    if (!_prop_x_ext_keyboard_exist)
        _prop_x_ext_keyboard_exist = ecore_x_atom_get (PROP_X_EXT_KEYBOARD_EXIST);

    if (!ecore_x_window_prop_card32_get (_root_win, _prop_x_ext_keyboard_exist, &_hw_kbd_num, 1)) {
        std::cerr << "ecore_x_window_prop_card32_get () is failed!!!\n";
        return;
    }

    if (_hw_kbd_num == 0)
        _hw_kbd_connected = false;
    else
        _hw_kbd_connected = true;
}

static void reload_config_cb (const ConfigPointer &config)
{
    uint32 option = 0;
    String uuid, name;
    isf_get_keyboard_ise (_config, uuid, name, option);
    snprintf (_hw_ise_name, sizeof (_hw_ise_name), "%s", name.c_str ());
    update_setting_main_view (_common_ugd);
    //std::cout << "    " << __func__ << " (keyboard ISE : " << name << ")\n";
}

static void load_ise_info ()
{
    isf_load_ise_information (ALL_ISE, _config);

    std::vector<String> all_langs, sw_uuid_list, hw_uuid_list;
    _sw_ise_list.clear ();
    isf_get_all_languages (all_langs);
    isf_get_helper_ises_in_languages (all_langs, sw_uuid_list, _sw_ise_list);

    _hw_ise_list.clear ();
    isf_get_keyboard_ises_in_languages (all_langs, hw_uuid_list, _hw_ise_list);
}

static void *on_create (ui_gadget_h ug, enum ug_mode mode, service_h s, void *priv)
{
    Evas_Object *parent  = NULL;
    Evas_Object *content = NULL;

    if (ug == NULL || priv == NULL)
        return NULL;

    bindtextdomain (SETTING_PACKAGE, SETTING_LOCALEDIR);
    struct ug_data *ugd = (struct ug_data *)priv;
    ugd->ug = ug;
    parent = (Evas_Object *) ug_get_parent_layout (ug);
    if (parent == NULL)
        return NULL;

    if (_imf_context == NULL) {
        const char *ctx_id = ecore_imf_context_default_id_get ();
        if (ctx_id) {
            _imf_context = ecore_imf_context_add (ctx_id);
        }
    }

    load_config_module ();
    load_config_data (_config);
    scim_get_setup_module_list (_setup_modules);
    update_ise_list ();
    load_ise_info ();
    init_hw_keyboard_listener (ugd);

    _reload_signal_connection = _config->signal_connect_reload (slot (reload_config_cb));

    //-------------------------- ise infomation ----------------------------

    Evas_Object *parent_win = (Evas_Object *)ug_get_window ();
    Evas_Object *conform = elm_conformant_add (parent_win);

    // Create keyboard setting UI
    if (mode == UG_MODE_FULLVIEW)
        ugd->layout_main = create_fullview (conform, ugd);
    else
        ugd->layout_main = create_frameview (conform, ugd);

    if (ugd->layout_main != NULL) {
        content = create_setting_main_view (ugd);
        elm_object_part_content_set (ugd->layout_main, "elm.swallow.content", content);

        evas_object_size_hint_weight_set (conform, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set (conform, EVAS_HINT_FILL, EVAS_HINT_FILL);
        elm_win_resize_object_add (parent_win, conform);
        elm_object_content_set (conform, ugd->layout_main);
        evas_object_show (conform);
    }
    return (void *)ugd->layout_main;
}

static void on_start (ui_gadget_h ug, service_h s, void *priv)
{
}

static void on_pause (ui_gadget_h ug, service_h s, void *priv)
{
    if (ug == NULL || priv == NULL)
        return;
    struct ug_data *ugd = (struct ug_data *) priv;
    if (ugd->key_end_cb == ise_option_view_set_cb) {    //inside ise setup module
        _mdl->save_config (_config);
        helper_ise_reload_config ();
    } else if (ugd->key_end_cb == sw_keyboard_selection_view_set_cb) {
        set_active_sw_ise ();
    } else if (ugd->key_end_cb == hw_keyboard_selection_view_set_cb) {
        set_active_hw_ise ();
    }
}

static void on_resume (ui_gadget_h ug, service_h s, void *priv)
{
    if (ug == NULL || priv == NULL)
        return;

    if (_mdl != NULL) {
        _mdl->query_changed();
    }
}

static void on_destroy (ui_gadget_h ug, service_h s, void *priv)
{
    if (ug == NULL || priv == NULL)
        return;

    if (_imf_context != NULL) {
        ecore_imf_context_del (_imf_context);
        _imf_context = NULL;
    }

    struct ug_data *ugd = (struct ug_data *) priv;

    if (ugd->naviframe != NULL) {
        evas_object_del (ugd->naviframe);
        ugd->naviframe = NULL;
    }

    if (ugd->layout_main != NULL) {
        evas_object_del (ugd->layout_main);
        ugd->layout_main = NULL;
    }

    if (_mdl) {
        delete _mdl;
        _mdl = NULL;
    }

    if (!_config.null ()) {
        _config->flush ();
        _config.reset ();
    }

    for (int i = 0; i < ITEM_TOTAL_COUNT; i++) {
        if (_p_items[i] != NULL) {
            if (_p_items[i]->text) {
                free (_p_items[i]->text);
                _p_items[i]->text = NULL;
            }
            if (_p_items[i]->sub_text) {
                free (_p_items[i]->sub_text);
                _p_items[i]->sub_text = NULL;
            }
            free (_p_items[i]);
            _p_items[i] = NULL;
        }
    }

    if (_prop_change_handler != NULL) {
        ecore_event_handler_del (_prop_change_handler);
        _prop_change_handler = NULL;
    }

    _reload_signal_connection.disconnect ();
}

static void on_message (ui_gadget_h ug, service_h msg, service_h data, void *priv)
{
}

static void on_event (ui_gadget_h ug, enum ug_event event, service_h s, void *priv)
{
    switch (event) {
        case UG_EVENT_LOW_MEMORY:
            break;
        case UG_EVENT_LOW_BATTERY:
            break;
        case UG_EVENT_LANG_CHANGE:
            break;
        case UG_EVENT_ROTATE_PORTRAIT:
            break;
        case UG_EVENT_ROTATE_PORTRAIT_UPSIDEDOWN:
            break;
        case UG_EVENT_ROTATE_LANDSCAPE:
            break;
        case UG_EVENT_ROTATE_LANDSCAPE_UPSIDEDOWN:
            break;
        default:
            break;
    }
}

static void on_key_event (ui_gadget_h ug, enum ug_key_event event, service_h s, void *priv)
{
    if (ug == NULL || priv == NULL)
        return;

    struct ug_data *ugd = (struct ug_data *)priv;
    Elm_Object_Item *top_it;

    switch (event) {
        case UG_KEY_EVENT_END:
            top_it = elm_naviframe_top_item_get (ugd->naviframe);
            // ISE option maybe multiple layouts
            if (top_it && (elm_object_item_content_get (top_it) != ugd->opt_eo) && (ugd->key_end_cb == ise_option_view_set_cb))
                _mdl->key_proceeding (TYPE_KEY_END);
            else
                ugd->key_end_cb (priv, NULL);

            break;
        default:
            break;
    }
}

#ifdef __cplusplus
extern "C"
{
#endif
    UG_MODULE_API int UG_MODULE_INIT (struct ug_module_ops *ops)
    {
        if (ops == NULL)
            return -1;

        struct ug_data *ugd = (ug_data*)calloc (1, sizeof (struct ug_data));
        if (ugd == NULL)
            return -1;

        _common_ugd  = ugd;
        ops->create  = on_create;
        ops->start   = on_start;
        ops->pause   = on_pause;
        ops->resume  = on_resume;
        ops->destroy = on_destroy;
        ops->message = on_message;
        ops->event   = on_event;
        ops->key_event = on_key_event;
        ops->priv    = ugd;
        ops->opt     = UG_OPT_INDICATOR_ENABLE;

        return 0;
    }

    UG_MODULE_API void UG_MODULE_EXIT (struct ug_module_ops *ops)
    {
        if (ops == NULL)
            return;

        struct ug_data *ugd = (struct ug_data *)(ops->priv);
        if (ugd != NULL)
            free (ugd);
    }

    // Reset keyboard setting
    UG_MODULE_API int setting_plugin_reset (service_h s, void *priv)
    {
        const int BUFF_MAX = 1024;
        char readbuf[BUFF_MAX] = {0,};
        char *value_str = NULL;
        char *key = NULL;

        if (vconf_set_bool (VCONFKEY_AUTOCAPITAL_ALLOW_BOOL, true) == -1)
            return -1;
        if (vconf_set_bool (VCONFKEY_AUTOPERIOD_ALLOW_BOOL, false) == -1)
            return -1;

        if (_imf_context == NULL) {
            const char *ctx_id = ecore_imf_context_default_id_get ();
            if (ctx_id) {
                _imf_context = ecore_imf_context_add (ctx_id);
            }
        }

        load_config_module ();
        isf_load_ise_information (ALL_ISE, _config);

        String uuid = scim_global_config_read (String (SCIM_GLOBAL_CONFIG_INITIAL_ISE_UUID), String (SCIM_COMPOSE_KEY_FACTORY_UUID));

        // Read and parse CSC data
        ifstream csc_file (CSC_FILEPATH);

        if (csc_file.is_open ()) {
            while (1) {
                if (csc_file.getline (readbuf, BUFF_MAX) == NULL)
                    break;

                char **items = eina_str_split (readbuf, "=", 2);
                if (!items || !items[0]) continue;

                key = trim_string (items[0]);
                if (!key) continue;

                if (!strcmp (key, "keyboard_uuid")) {
                    if (items[1]) {
                        value_str = trim_string (items[1]);
                        uuid = String (value_str);
                        free (value_str);
                    }
                }
                else if (!strcmp (key, "keyboard_lang")) {
                    if (items[1]) {
                        value_str = trim_string (items[1]);
                        vconf_set_str (VCONFKEY_ISF_INPUT_LANGUAGE, value_str);
                        free (value_str);
                    }
                }

                free (key);
                free (items[0]);
                free (items);
            }

            csc_file.close ();
        }

        TOOLBAR_MODE_T type = (TOOLBAR_MODE_T)scim_global_config_read (String (SCIM_GLOBAL_CONFIG_INITIAL_ISE_TYPE), TOOLBAR_KEYBOARD_MODE);
        if (ecore_x_window_prop_card32_get (ecore_x_window_root_first_get (), ecore_x_atom_get (PROP_X_EXT_KEYBOARD_EXIST), &_hw_kbd_num, 1) > 0) {
            if (_hw_kbd_num != 0) {
                if (type != TOOLBAR_KEYBOARD_MODE) {
                    _config->write (String (SCIM_CONFIG_DEFAULT_HELPER_ISE), uuid);
                    _config->flush ();
                    uuid = String (SCIM_COMPOSE_KEY_FACTORY_UUID);
                }
            }
        }

        isf_control_set_active_ise_by_uuid (uuid.c_str ());

        String mdl_name;
        for (unsigned int i = 0; i < _module_names.size (); i++) {
            if (_modes[i] == TOOLBAR_KEYBOARD_MODE)
                mdl_name = _module_names[i] + String ("-imengine-setup");
            else
                mdl_name = _module_names[i] + String ("-setup");

            SetupModule *setup_module = new SetupModule (mdl_name);
            if (setup_module != NULL && setup_module->valid ()) {
                if (setup_module->option_reset (_config) == false)
                    std::cerr << mdl_name << " failed to reset option!\n";
            } else {
                std::cerr << "Load " << mdl_name << " is failed!!!\n";
            }

            if (setup_module) {
                delete setup_module;
                setup_module = NULL;
            }
        }

        helper_ise_reload_config ();

        if (_imf_context != NULL) {
            ecore_imf_context_del (_imf_context);
            _imf_context = NULL;
        }
        return 0;
    }

#ifdef __cplusplus
}
#endif

/*
vi:ts=4:ai:nowrap:expandtab
 */
