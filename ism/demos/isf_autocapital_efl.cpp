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
#include "isf_autocapital_efl.h"

static Evas_Object *_create_ef_layout (Evas_Object *parent, const char *label, const char *guide_text, Elm_Autocapital_Type autocap, Eina_Bool caps_lock_mode)
{
    Evas_Object *ef;
    ef = _create_ef (parent, label, guide_text);
    Evas_Object *en = elm_object_part_content_get (ef, "elm.swallow.content");
    elm_entry_autocapital_type_set (en, autocap);

    Ecore_IMF_Context *ic = (Ecore_IMF_Context *)elm_entry_imf_context_get (en);
    if (ic) {
        ecore_imf_context_input_panel_caps_lock_mode_set (ic, caps_lock_mode);
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

    /* Caps lock mode off */

    /* NONE type */
    ef = _create_ef_layout (parent, _("NONE type"), _("click to enter"), ELM_AUTOCAPITAL_TYPE_NONE, EINA_FALSE);
    elm_box_pack_end (bx, ef);

    /* WORD type */
    ef = _create_ef_layout (parent, _("WORD type"), _("click to enter"), ELM_AUTOCAPITAL_TYPE_WORD, EINA_FALSE);
    elm_box_pack_end (bx, ef);

    /* Sentence type */
    ef = _create_ef_layout (parent, _("SENTENCE type"), _("click to enter"), ELM_AUTOCAPITAL_TYPE_SENTENCE, EINA_FALSE);
    elm_box_pack_end (bx, ef);

    /* ALLCHARACTER type */
    ef = _create_ef_layout (parent, _("ALLCHARACTER type"), _("click to enter"), ELM_AUTOCAPITAL_TYPE_ALLCHARACTER, EINA_FALSE);
    elm_box_pack_end (bx, ef);

    /* Caps lock mode ON */

    /* NONE type */
    ef = _create_ef_layout (parent, _("NONE type - CAPS LOCK"), _("click to enter"), ELM_AUTOCAPITAL_TYPE_NONE, EINA_TRUE);
    elm_box_pack_end (bx, ef);

    /* WORD type */
    ef = _create_ef_layout (parent, _("WORD type - CAPS LOCK"), _("click to enter"), ELM_AUTOCAPITAL_TYPE_WORD, EINA_TRUE);
    elm_box_pack_end (bx, ef);

    /* Sentence type */
    ef = _create_ef_layout (parent, _("SENTENCE type - CAPS LOCK"), _("click to enter"), ELM_AUTOCAPITAL_TYPE_SENTENCE, EINA_TRUE);
    elm_box_pack_end (bx, ef);

    /* ALLCHARACTER type */
    ef = _create_ef_layout (parent, _("ALLCHARACTER type - CAPS LOCK"), _("click to enter"), ELM_AUTOCAPITAL_TYPE_ALLCHARACTER, EINA_TRUE);
    elm_box_pack_end (bx, ef);

    return bx;
}

void ise_autocapital_bt (void *data, Evas_Object *obj, void *event_info)
{
    Evas_Object *lay_inner = create_inner_layout (data);
    add_layout_to_conformant (data, lay_inner, _("Autocapital"));
}

/*
vi:ts=4:ai:nowrap:expandtab
*/
