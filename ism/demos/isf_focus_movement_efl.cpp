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

static void
_entry_activated_cb (void *data, Evas_Object *obj, void *event_info)
{
    Evas_Object *parent = (Evas_Object *)data;
    elm_object_focus_next (parent, ELM_FOCUS_NEXT);
}

static Evas_Object *create_entry (Evas_Object *parent, const char *text, Elm_Input_Panel_Layout layout)
{
    Evas_Object *en;
    en = elm_entry_add (parent);
    evas_object_size_hint_weight_set (en, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set (en, EVAS_HINT_FILL, 0.5);
    elm_entry_input_panel_layout_set (en, layout);
    elm_entry_single_line_set (en, EINA_TRUE);
    elm_entry_input_panel_return_key_type_set (en, ELM_INPUT_PANEL_RETURN_KEY_TYPE_NEXT);
    elm_object_text_set (en, text);
    evas_object_smart_callback_add (en, "activated", _entry_activated_cb, parent);
    evas_object_show (en);

    return en;
}

static Evas_Object * create_inner_layout (void *data)
{
    struct appdata *ad = (struct appdata *)data;
    Evas_Object *bx = NULL;
    Evas_Object *parent = ad->naviframe;
    Evas_Object *en = NULL;

    bx = elm_box_add (parent);
    evas_object_size_hint_weight_set (bx, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set (bx, EVAS_HINT_FILL, 0.0);
    evas_object_show (bx);

    /* First Name */
    en = create_entry (bx, "James", ELM_INPUT_PANEL_LAYOUT_NORMAL);
    elm_box_pack_end (bx, en);

    /* Last Name */
    en = create_entry (bx, "Bond", ELM_INPUT_PANEL_LAYOUT_NORMAL);
    elm_box_pack_end (bx, en);

    /* Phonenumber */
    en = create_entry (bx, "010-1234-5678", ELM_INPUT_PANEL_LAYOUT_PHONENUMBER);
    elm_box_pack_end (bx, en);

    /* Email */
    en = create_entry (bx, "hell@hello.com", ELM_INPUT_PANEL_LAYOUT_EMAIL);
    elm_box_pack_end (bx, en);

    /* Birthday */
    en = create_entry (bx, "801225", ELM_INPUT_PANEL_LAYOUT_NUMBER);
    elm_box_pack_end (bx, en);

    /* Homepage */
    en = create_entry (bx, "hello.com", ELM_INPUT_PANEL_LAYOUT_URL);
    elm_box_pack_end (bx, en);

    return bx;
}

void isf_focus_movement_bt (void *data, Evas_Object *obj, void *event_info)
{
    Evas_Object *lay_inner = create_inner_layout (data);
    add_layout_to_naviframe (data, lay_inner, _("Focus Movement"));
}

/*
vi:ts=4:ai:nowrap:expandtab
*/
