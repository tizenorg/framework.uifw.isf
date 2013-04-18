/*
 * ISF(Input Service Framework)
 *
 * ISF is based on SCIM 1.4.7 and extended for supporting more mobile fitable.
 * Copyright (c) 2012-2013 Samsung Electronics Co., Ltd.
 *
 * Contact: Shuo Liu <shuo0805.liu@samsung.com>, Jihoon Kim <jihoon48.kim@samsung.com>
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

#include "isf_demo_efl.h"
#include "isf_control.h"

static Ecore_IMF_Context *imf_context = NULL;
static Elm_Genlist_Item_Class itci;

enum {
    INPUT_PANEL_GEOMETRY_GET,
    INPUT_PANEL_SHOW,
    INPUT_PANEL_HIDE,
    INPUT_PANEL_IMDATA_SET,
    INPUT_PANEL_IMDATA_GET,
    INPUT_PANEL_LAYOUT_SET,
    INPUT_PANEL_LAYOUT_GET,
    INPUT_PANEL_STATE_GET,
    GET_ACTIVE_ISE,
    GET_ISE_INFO,
    GET_ISE_LIST,
    RESET_DEFAULT_ISE
};

const char *api_list[]={
    "PANEL GEOMETRY GET",
    "INPUT PANEL SHOW",
    "INPUT PANEL HIDE",
    "INPUT PANEL IMDATA SET",
    "INPUT PANEL IMDATA GET",
    "INPUT PANEL LAYOUT SET",
    "INPUT PANEL LAYOUT GET",
    "INPUT PANEL STATE GET",
    "GET ACTIVE ISE",
    "GET ACTIVE ISE INFO",
    "GET ISE LIST",
    "RESET DEFAULT ISE"
};

static void test_input_panel_geometry_get (void *data, Evas_Object *obj, void *event_info)
{
    int x, y, w, h;
    if (imf_context != NULL) {
        ecore_imf_context_input_panel_geometry_get (imf_context, &x, &y, &w, &h);
        printf ("x=%d \n", x);
        printf ("y=%d \n", y);
        printf ("width=%d \n", w);
        printf ("height=%d \n", h);
    }
}

void test_input_panel_show (void *data, Evas_Object *obj, void *event_info)
{
    if (imf_context != NULL) {
        ecore_imf_context_focus_in (imf_context);
        ecore_imf_context_input_panel_show (imf_context);
    }
}

void test_input_panel_hide (void *data, Evas_Object *obj, void *event_info)
{
    if (imf_context != NULL) {
        ecore_imf_context_focus_out (imf_context);
        ecore_imf_context_input_panel_hide (imf_context);
    }
}

void test_input_panel_imdata_set (void *data, Evas_Object *obj, void *event_info)
{
    // need ISE to deal with the data
    char buf[256] = "ur imdata";
    if (imf_context != NULL)
        ecore_imf_context_input_panel_imdata_set (imf_context, buf, sizeof (buf));
}

void test_input_panel_imdata_get (void *data, Evas_Object *obj, void *event_info)
{
    int len = 256;
    char* buf = (char*) malloc (len);
    if (buf != NULL) {
        memset (buf, '\0', len);
        if (imf_context != NULL) {
            ecore_imf_context_input_panel_imdata_get (imf_context, buf, &len);
            printf ("get imdata  %s, and len is %d ...\n", (char *)buf, len);
        }
        free (buf);
    }
}

void test_input_panel_layout_set (void *data, Evas_Object *obj, void *event_info)
{
    Ecore_IMF_Input_Panel_Layout layout = ECORE_IMF_INPUT_PANEL_LAYOUT_EMAIL;
    if (imf_context != NULL)
        ecore_imf_context_input_panel_layout_set (imf_context, layout);
}

void test_input_panel_layout_get (void *data, Evas_Object *obj, void *event_info)
{
    Ecore_IMF_Input_Panel_Layout layout;
    if (imf_context != NULL) {
        layout = ecore_imf_context_input_panel_layout_get (imf_context);
        printf ("get layout : %d ...\n", (int)layout);
    }
}

void test_input_panel_state_get (void *data, Evas_Object *obj, void *event_info)
{
    Ecore_IMF_Input_Panel_State state;

    if (imf_context != NULL) {
        state = ecore_imf_context_input_panel_state_get (imf_context);
        printf ("ise state : %d \n", (int)state);
    }
}

void test_get_active_ise (void *data, Evas_Object *obj, void *event_info)
{
    char *uuid = NULL;
    int ret = isf_control_get_active_ise (&uuid);
    if (ret > 0 && uuid)
        printf (" Get active ISE: %s\n", uuid);
    if (uuid)
        free (uuid);
}

void test_get_ise_list (void *data, Evas_Object *obj, void *event_info)
{
    char **iselist = NULL;
    int count = isf_control_get_ise_list (&iselist);

    for (int i = 0; i < count; i++) {
        if (iselist[i]) {
            printf (" [%d : %s]\n", i, iselist[i]);
            free (iselist[i]);
        }
    }
    if (iselist)
        free (iselist);
}

void test_get_ise_info (void *data, Evas_Object *obj, void *event_info)
{
    char *uuid = NULL;
    int ret = isf_control_get_active_ise (&uuid);
    if (ret > 0 && uuid) {
        char *name      = NULL;
        char *language  = NULL;
        ISE_TYPE_T type = HARDWARE_KEYBOARD_ISE;
        int   option    = 0;
        ret = isf_control_get_ise_info (uuid, &name, &language, &type, &option);
        if (ret == 0 && name && language) {
            printf (" Active ISE: uuid[%s], name[%s], language[%s], type[%d], option[%d]\n", uuid, name, language, type, option);
        }
        if (name)
            free (name);
        if (language)
            free (language);
    }
    if (uuid)
        free (uuid);
}

void test_reset_default_ise (void *data, Evas_Object *obj, void *event_info)
{
    int ret = isf_control_set_active_ise_to_default ();
    if (ret == 0)
        printf (" Reset default ISE is successful!\n");
    else
        printf (" Reset default ISE is failed!!!\n");
}

char *gli_label_get (void *data, Evas_Object *obj, const char *part)
{
    int j = (int)data;
    return strdup (api_list[j]);
}

static void test_api (void *data, Evas_Object *obj, void *event_info)
{
    int j = (int)data;
    switch (j) {
    case INPUT_PANEL_GEOMETRY_GET:
        test_input_panel_geometry_get (NULL, obj, event_info);
        break;
    case INPUT_PANEL_SHOW:
        test_input_panel_show (NULL, obj, event_info);
        break;
    case INPUT_PANEL_HIDE:
        test_input_panel_hide (NULL, obj, event_info);
        break;
    case INPUT_PANEL_IMDATA_SET:
        test_input_panel_imdata_set (NULL,obj, event_info);
        break;
    case INPUT_PANEL_IMDATA_GET:
        test_input_panel_imdata_get (NULL,obj, event_info);
        break;
    case INPUT_PANEL_LAYOUT_SET:
        test_input_panel_layout_set (NULL,obj, event_info);
        break;
    case INPUT_PANEL_LAYOUT_GET:
        test_input_panel_layout_get (NULL,obj, event_info);
        break;
    case INPUT_PANEL_STATE_GET:
        test_input_panel_state_get (NULL, obj, event_info);
        break;
    case GET_ACTIVE_ISE:
        test_get_active_ise (NULL, obj, event_info);
        break;
    case GET_ISE_LIST:
        test_get_ise_list (NULL, obj, event_info);
        break;
    case GET_ISE_INFO:
        test_get_ise_info (NULL, obj, event_info);
        break;
    case RESET_DEFAULT_ISE:
        test_reset_default_ise (NULL, obj, event_info);
        break;
    default:
        break;
    }
}

static Eina_Bool _nf_back_event_cb (void *data, Elm_Object_Item *it)
{
    if (imf_context) {
        ecore_imf_context_del (imf_context);
        imf_context = NULL;
    }

    return EINA_TRUE;
}

static Evas_Object *_create_imcontrolapi_list (Evas_Object *parent)
{
    int i, num;

    Evas_Object *gl = elm_genlist_add (parent);

    itci.item_style     = "default";
    itci.func.text_get  = gli_label_get;
    itci.func.content_get  = NULL;
    itci.func.state_get = NULL;
    itci.func.del       = NULL;

    num = sizeof (api_list) / sizeof (char *);
    for (i = 0; i < num; i++) {
        elm_genlist_item_append (gl, &itci,
                                 (void *)i/* item data */, NULL/* parent */, ELM_GENLIST_ITEM_NONE, test_api/* func */,
                                 (void *)i/* func data */);
    }

    return gl;
}

void imcontrolapi_bt (void *data, Evas_Object *obj, void *event_info)
{
    const char *ctx_id = ecore_imf_context_default_id_get ();
    if (ctx_id != NULL) {
        imf_context = ecore_imf_context_add (ctx_id);
    } else {
        printf ("Cannot create imf context\n");
        return;
    }

    struct appdata *ad = (struct appdata *)data;
    Evas_Object *gl = NULL;

    gl = _create_imcontrolapi_list (ad->naviframe);

    Elm_Object_Item *navi_it = elm_naviframe_item_push (ad->naviframe, _("IM Control"), NULL, NULL, gl, NULL);
    elm_naviframe_item_pop_cb_set (navi_it, _nf_back_event_cb, ad);
}

/*
vi:ts=4:ai:nowrap:expandtab
*/
