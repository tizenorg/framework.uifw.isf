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

#ifndef __ISF_IMF_CONTROL_H
#define __ISF_IMF_CONTROL_H

#include <Ecore_IMF.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /* non UI related works */
    void _isf_imf_control_finalize (void);

    int _isf_imf_context_input_panel_show (int client_id, int context, void *data, int length, bool &input_panel_show);
    int _isf_imf_context_input_panel_hide (int client_id, int context);
    int _isf_imf_context_control_panel_show (void);
    int _isf_imf_context_control_panel_hide (void);

    int _isf_imf_context_input_panel_language_set (Ecore_IMF_Input_Panel_Lang lang);
    int _isf_imf_context_input_panel_language_locale_get (char **locale);

    int _isf_imf_context_input_panel_imdata_set (const void *data, int len);
    int _isf_imf_context_input_panel_imdata_get (void *data, int *len);
    int _isf_imf_context_input_panel_geometry_get (int *x, int *y, int *w, int *h);
    int _isf_imf_context_input_panel_layout_set (Ecore_IMF_Input_Panel_Layout layout);
    int _isf_imf_context_input_panel_layout_get (Ecore_IMF_Input_Panel_Layout *layout);
    int _isf_imf_context_input_panel_return_key_type_set (Ecore_IMF_Input_Panel_Return_Key_Type type);
    int _isf_imf_context_input_panel_return_key_type_get (Ecore_IMF_Input_Panel_Return_Key_Type &type);
    int _isf_imf_context_input_panel_return_key_disabled_set (Eina_Bool disabled);
    int _isf_imf_context_input_panel_return_key_disabled_get (Eina_Bool &disabled);
    int _isf_imf_context_input_panel_caps_mode_set (unsigned int mode);

    int _isf_imf_context_candidate_window_geometry_get (int *x, int *y, int *w, int *h);

    int _isf_imf_context_control_focus_in (void);
    int _isf_imf_context_control_focus_out (void);

    int _isf_imf_context_input_panel_send_will_show_ack (void);
    int _isf_imf_context_input_panel_send_will_hide_ack (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __ISF_IMF_CONTROL_H */

