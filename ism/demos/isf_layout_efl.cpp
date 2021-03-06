/*
 * ISF(Input Service Framework)
 *
 * ISF is based on SCIM 1.4.7 and extended for supporting more mobile fitable.
 * Copyright (c) 2000 - 2012 Samsung Electronics Co., Ltd. All rights reserved.
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
#include "isf_layout_efl.h"

static void _rotate_cb (void *data, Evas_Object *obj, void *event_info)
{
    struct appdata *ad = (struct appdata *)data;

    int angle = elm_win_rotation_get (ad->win_main);
    if (angle == 0) {
        elm_win_rotation_with_resize_set (ad->win_main, 270);
    } else if (angle == 270) {
        elm_win_rotation_with_resize_set (ad->win_main, 0);
    }
}

static void _input_panel_state_cb (void *data, Ecore_IMF_Context *ctx, int value)
{
    int x, y, w, h;

    if (value == ECORE_IMF_INPUT_PANEL_STATE_SHOW) {
        ecore_imf_context_input_panel_geometry_get (ctx, &x, &y, &w, &h);
        printf ("[%s] Input panel is shown. ctx : %p\n", __func__, ctx);
        printf ("x : %d, y : %d, w : %d, h : %d\n", x, y, w, h);
    } else if (value == ECORE_IMF_INPUT_PANEL_STATE_HIDE) {
        printf ("[%s] Input panel is hidden. ctx : %p\n", __func__, ctx);
    } else if (value == ECORE_IMF_INPUT_PANEL_STATE_WILL_SHOW) {
        printf ("[%s] Input panel will be shown. ctx : %p\n", __func__, ctx);
    }
}

static void _input_panel_resize_cb (void *data, Ecore_IMF_Context *ctx, int value)
{
    int x, y, w, h;

    ecore_imf_context_input_panel_geometry_get (ctx, &x, &y, &w, &h);
    printf ("[%s] x : %d, y : %d, w : %d, h : %d\n", __func__, x, y, w, h);
}

static void _shift_mode_cb (void *data, Ecore_IMF_Context *ctx, int value)
{
    if (value == ECORE_IMF_INPUT_PANEL_SHIFT_MODE_OFF) {
        printf ("[%s] Shift Mode : OFF\n", __func__);
    } else if (value == ECORE_IMF_INPUT_PANEL_SHIFT_MODE_ON) {
        printf ("[%s] Shift Mode : ON\n", __func__);
    }
}

static void _language_changed_cb (void *data, Ecore_IMF_Context *ctx, int value)
{
    char *locale;

    ecore_imf_context_input_panel_language_locale_get(ctx, &locale);

    printf ("[%s] language : %s\n", __func__, locale);

    if (locale)
        free (locale);
}

static void _candidate_panel_state_cb (void *data, Ecore_IMF_Context *ctx, int value)
{
    int x, y, w, h;

    if (value == ECORE_IMF_CANDIDATE_PANEL_SHOW) {
        ecore_imf_context_candidate_panel_geometry_get (ctx, &x, &y, &w, &h);
        printf ("Candidate window is shown\n");
        printf ("[%s] x : %d, y : %d, w : %d, h : %d\n", __func__, x, y, w, h);
    } else if (value == ECORE_IMF_CANDIDATE_PANEL_HIDE) {
        printf ("Candidate window is hidden\n");
    }
}

static void _candidate_panel_geometry_changed_cb (void *data, Ecore_IMF_Context *ctx, int value)
{
    int x, y, w, h;

    ecore_imf_context_candidate_panel_geometry_get (ctx, &x, &y, &w, &h);
    printf ("[%s] ctx : %p, x : %d, y : %d, w : %d, h : %d\n", __func__, ctx, x, y, w, h);
}

static void
_key_down_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
    Evas_Event_Key_Down *ev = (Evas_Event_Key_Down *)event_info;
    printf ("[evas key down] keyname : '%s', key : '%s', string : '%s', compose : '%s'\n", ev->keyname, ev->key, ev->string, ev->compose);
}

static void
_key_up_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
    Evas_Event_Key_Up *ev = (Evas_Event_Key_Up *)event_info;
    printf ("[evas key up] keyname : '%s', key : '%s', string : '%s', compose : '%s'\n", ev->keyname, ev->key, ev->string, ev->compose);
}

static Evas_Object *_create_ef_layout(Evas_Object *parent, const char *label, const char *guide_text,Elm_Input_Panel_Layout layout)
{
    Evas_Object *ef =  _create_ef (parent, label, guide_text);
    Ecore_IMF_Context *ic = NULL;
    Evas_Object *en = elm_object_part_content_get (ef, "elm.swallow.content");
    elm_entry_input_panel_layout_set (en, layout);
    evas_object_event_callback_add (en, EVAS_CALLBACK_KEY_DOWN, _key_down_cb, NULL);
    evas_object_event_callback_add (en, EVAS_CALLBACK_KEY_UP, _key_up_cb, NULL);

    ic = (Ecore_IMF_Context *)elm_entry_imf_context_get (en);

    if (ic != NULL) {
        ecore_imf_context_input_panel_event_callback_add (ic, ECORE_IMF_INPUT_PANEL_STATE_EVENT, _input_panel_state_cb, NULL);
        ecore_imf_context_input_panel_event_callback_add (ic, ECORE_IMF_INPUT_PANEL_GEOMETRY_EVENT, _input_panel_resize_cb, NULL);
        ecore_imf_context_input_panel_event_callback_add (ic, ECORE_IMF_INPUT_PANEL_SHIFT_MODE_EVENT, _shift_mode_cb, NULL);
        ecore_imf_context_input_panel_event_callback_add (ic, ECORE_IMF_INPUT_PANEL_LANGUAGE_EVENT, _language_changed_cb, NULL);

        ecore_imf_context_input_panel_event_callback_add (ic, ECORE_IMF_CANDIDATE_PANEL_STATE_EVENT, _candidate_panel_state_cb, NULL);
        ecore_imf_context_input_panel_event_callback_add (ic, ECORE_IMF_CANDIDATE_PANEL_GEOMETRY_EVENT, _candidate_panel_geometry_changed_cb, NULL);
    }

    return ef;
}

static void add_layout_to_conformant (void *data, Evas_Object *lay_in, const char *title)
{
   Evas_Object *scroller = NULL;
   Evas_Object *win = NULL;
   Evas_Object *conform = NULL;
   struct appdata *ad = NULL;

   ad = (struct appdata *) data;

   win = ad->win_main;
   // Enabling illume notification property for window
   elm_win_conformant_set (win, EINA_TRUE);

   // Creating conformant widget
   conform = elm_conformant_add (win);
   evas_object_size_hint_weight_set (conform, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show (conform);

   scroller = elm_scroller_add (ad->naviframe);

   elm_scroller_bounce_set (scroller, EINA_FALSE, EINA_TRUE);
   evas_object_show (scroller);

   elm_object_content_set (scroller, lay_in);
   elm_object_content_set (conform, scroller);
   elm_naviframe_item_push (ad->naviframe, title, NULL, NULL, conform, NULL);
}

static Evas_Object * create_inner_layout (void *data)
{
    struct appdata *ad = (struct appdata *)data;
    Evas_Object *bx = NULL ;
    Evas_Object *ef = NULL ;
    Evas_Object *parent = ad->naviframe;

    bx = elm_box_add (parent);
    evas_object_size_hint_weight_set (bx, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set (bx, EVAS_HINT_FILL, 0.0);
    evas_object_show (bx);

    /* Normal Layout */
    ef = _create_ef_layout (parent, _("NORMAL LAYOUT"), _("click to enter TEXT"), ELM_INPUT_PANEL_LAYOUT_NORMAL);
    elm_box_pack_end (bx, ef);

    /* Number Layout */
    ef = _create_ef_layout (parent, _("NUMBER LAYOUT"), _("click to enter NUMBER"), ELM_INPUT_PANEL_LAYOUT_NUMBER);
    elm_box_pack_end (bx, ef);

    /* Email Layout */
    ef = _create_ef_layout (parent, _("EMAIL LAYOUT"), _("click to enter EMAIL"), ELM_INPUT_PANEL_LAYOUT_EMAIL);
    elm_box_pack_end (bx, ef);

    /* URL Layout */
    ef = _create_ef_layout (parent, _("URL LAYOUT"), _("click to enter URL"), ELM_INPUT_PANEL_LAYOUT_URL);
    elm_box_pack_end (bx, ef);

    /* Phonenumber Layout */
    ef = _create_ef_layout (parent, _("PHONENUMBER LAYOUT"), _("click to enter PHONENUMBER"), ELM_INPUT_PANEL_LAYOUT_PHONENUMBER);
    elm_box_pack_end (bx, ef);

    /* IP Layout */
    ef = _create_ef_layout (parent, _("IP LAYOUT"), _("click to enter IP"), ELM_INPUT_PANEL_LAYOUT_IP);
    elm_box_pack_end (bx, ef);

    /* Month Layout */
    ef = _create_ef_layout (parent, _("MONTH LAYOUT"), _("click to enter MONTH"), ELM_INPUT_PANEL_LAYOUT_MONTH);
    elm_box_pack_end (bx, ef);

    /* Number Only Layout */
    ef = _create_ef_layout (parent, _("NUMBERONLY LAYOUT"), _("click to enter NUMBERONLY"), ELM_INPUT_PANEL_LAYOUT_NUMBERONLY);
    elm_box_pack_end (bx, ef);

    /* Password Layout */
    ef = _create_ef_layout (parent, _("PASSWORD LAYOUT"), _("click to enter PASSWORD"), ELM_INPUT_PANEL_LAYOUT_PASSWORD);
    elm_box_pack_end (bx, ef);

    /* Click to rotate button */
    Evas_Object *rotate_btn = elm_button_add (parent);
    elm_object_text_set (rotate_btn, "rotate");
    evas_object_smart_callback_add (rotate_btn, "clicked", _rotate_cb, (void *)ad);
    evas_object_size_hint_weight_set (rotate_btn, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set (rotate_btn, EVAS_HINT_FILL, 0);
    evas_object_show (rotate_btn);
    elm_box_pack_end (bx, rotate_btn);

    return bx;
}

void ise_layout_bt (void *data, Evas_Object *obj, void *event_info)
{
    Evas_Object *lay_inner = create_inner_layout (data);
    add_layout_to_conformant (data, lay_inner, _("Layout"));
}

/*
vi:ts=4:ai:nowrap:expandtab
*/
