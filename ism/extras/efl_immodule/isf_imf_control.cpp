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

#define Uses_SCIM_TRANSACTION
#define Uses_ISF_IMCONTROL_CLIENT

#include <stdio.h>
#include <Ecore.h>
#include <errno.h>
#include "isf_imf_control.h"
#include "scim.h"


using namespace scim;


//#define IMFCONTROLDBG(str...) printf(str)
#define IMFCONTROLDBG(str...)
#define IMFCONTROLERR(str...) printf(str)


static Ecore_Fd_Handler *_read_handler = 0;
static IMControlClient   _imcontrol_client;


extern void ecore_ise_process_data (Transaction &trans, int cmd);


static int isf_socket_wait_for_data_internal (int socket_id, int timeout)
{
    fd_set fds;
    struct timeval tv;
    struct timeval begin_tv;
    int ret;

    int m_id = socket_id;

    if (timeout >= 0) {
        gettimeofday (&begin_tv, 0);
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
    }

    while (1) {
        FD_ZERO (&fds);
        FD_SET (m_id, &fds);

        ret = select (m_id + 1, &fds, NULL, NULL, (timeout >= 0) ? &tv : NULL);
        if (timeout > 0) {
            int elapsed;
            struct timeval cur_tv;
            gettimeofday (&cur_tv, 0);
            elapsed = (cur_tv.tv_sec - begin_tv.tv_sec) * 1000 +
                      (cur_tv.tv_usec - begin_tv.tv_usec) / 1000;
            timeout = timeout - elapsed;
            if (timeout > 0) {
                tv.tv_sec = timeout / 1000;
                tv.tv_usec = (timeout % 1000) * 1000;
            } else {
                tv.tv_sec = 0;
                tv.tv_usec = 0;
                timeout = 0;
            }
        }
        if (ret > 0) {
            return ret;
        } else if (ret == 0) {
            if (timeout == 0)
                return ret;
            else
                continue;
        }

        if (errno == EINTR)
            continue;

        return ret;
    }
}

static Eina_Bool ecore_ise_input_handler (void *data, Ecore_Fd_Handler *fd_handler)
{
    int cmd;
    int timeout = 0;
    Transaction trans;

    if (fd_handler == NULL)
        return ECORE_CALLBACK_RENEW;

    int fd = ecore_main_fd_handler_fd_get (fd_handler);
    if (_imcontrol_client.is_connected () &&
        isf_socket_wait_for_data_internal (fd, timeout) > 0) {
        trans.clear ();
        if (!trans.read_from_socket (fd, timeout)) {
            IMFCONTROLERR ("%s:: read_from_socket() may be timeout \n", __FUNCTION__);
            _isf_imf_control_finalize ();
            return ECORE_CALLBACK_CANCEL;
        }

        if (trans.get_command (cmd) && cmd == SCIM_TRANS_CMD_REQUEST) {
            while (trans.get_command (cmd)) {
                ecore_ise_process_data (trans, cmd);
            }
        }
        return ECORE_CALLBACK_RENEW;
    }
    IMFCONTROLERR ("ecore_ise_input_handler is failed!!!\n");
    _isf_imf_control_finalize ();
    return ECORE_CALLBACK_CANCEL;
}

static void connect_panel (void)
{
    if (!_imcontrol_client.is_connected ()) {
        _imcontrol_client.open_connection ();
        int fd = _imcontrol_client.get_panel2imclient_connection_number ();
        if (fd > 0) {
            _read_handler = ecore_main_fd_handler_add (fd, ECORE_FD_READ, ecore_ise_input_handler, NULL, NULL, NULL);
        }
    }
}

void _isf_imf_control_finalize (void)
{
    IMFCONTROLDBG ("%s ...\n", __FUNCTION__);

    _imcontrol_client.close_connection ();

    if (_read_handler) {
        ecore_main_fd_handler_del (_read_handler);
        _read_handler = 0;
    }
}

int _isf_imf_context_input_panel_show (int client_id, int context, void *data, int length, bool &input_panel_show)
{
    int temp = 0;
    connect_panel ();
    _imcontrol_client.prepare ();
    _imcontrol_client.show_ise (client_id, context, data, length, &temp);
    input_panel_show = (bool)temp;
    return 0;
}

int _isf_imf_context_input_panel_hide (int client_id, int context)
{
    connect_panel ();
    _imcontrol_client.prepare ();
    _imcontrol_client.hide_ise (client_id, context);
    _imcontrol_client.send ();
    return 0;
}

int _isf_imf_context_control_panel_show (void)
{
    connect_panel ();
    _imcontrol_client.prepare ();
    _imcontrol_client.show_control_panel ();
    _imcontrol_client.send ();
    return 0;
}

int _isf_imf_context_control_panel_hide (void)
{
    connect_panel ();
    _imcontrol_client.prepare ();
    _imcontrol_client.hide_control_panel ();
    _imcontrol_client.send ();
    return 0;
}

int _isf_imf_context_input_panel_language_set (Ecore_IMF_Input_Panel_Lang lang)
{
    connect_panel ();
    _imcontrol_client.prepare ();
    _imcontrol_client.set_ise_language (lang);
    _imcontrol_client.send ();
    return 0;
}

int _isf_imf_context_input_panel_language_locale_get (char **locale)
{
    connect_panel ();
    _imcontrol_client.prepare ();
    _imcontrol_client.get_ise_language_locale (locale);
    return 0;
}

int _isf_imf_context_input_panel_imdata_set (const void *data, int len)
{
    connect_panel ();
    _imcontrol_client.prepare ();
    _imcontrol_client.set_imdata ((const char *)data, len);
    _imcontrol_client.send ();
    return 0;
}

int _isf_imf_context_input_panel_imdata_get (void *data, int *len)
{
    connect_panel ();
    _imcontrol_client.prepare ();
    _imcontrol_client.get_imdata ((char *)data, len);
    return 0;
}

int _isf_imf_context_input_panel_geometry_get (int *x, int *y, int *w, int *h)
{
    connect_panel ();
    _imcontrol_client.prepare ();
    _imcontrol_client.get_ise_window_geometry (x, y, w, h);
    return 0;
}

int _isf_imf_context_input_panel_return_key_type_set (Ecore_IMF_Input_Panel_Return_Key_Type type)
{
    connect_panel ();
    _imcontrol_client.prepare ();
    _imcontrol_client.set_return_key_type ((int)type);
    _imcontrol_client.send ();
    return 0;
}

int _isf_imf_context_input_panel_return_key_type_get (Ecore_IMF_Input_Panel_Return_Key_Type &type)
{
    int temp = 0;
    connect_panel ();
    _imcontrol_client.prepare ();
    _imcontrol_client.get_return_key_type (temp);
    type = (Ecore_IMF_Input_Panel_Return_Key_Type)temp;
    return 0;
}

int _isf_imf_context_input_panel_return_key_disabled_set (Eina_Bool disabled)
{
    connect_panel ();
    _imcontrol_client.prepare ();
    _imcontrol_client.set_return_key_disable ((int)disabled);
    _imcontrol_client.send ();
    return 0;
}

int _isf_imf_context_input_panel_return_key_disabled_get (Eina_Bool &disabled)
{
    int temp = 0;
    connect_panel ();
    _imcontrol_client.prepare ();
    _imcontrol_client.get_return_key_disable (temp);
    disabled = (Eina_Bool)temp;
    return 0;
}

int _isf_imf_context_input_panel_layout_set (Ecore_IMF_Input_Panel_Layout layout)
{
    int layout_temp = layout;

    connect_panel ();
    _imcontrol_client.prepare ();
    _imcontrol_client.set_layout (layout_temp);
    _imcontrol_client.send ();
    return 0;
}

int _isf_imf_context_input_panel_layout_get (Ecore_IMF_Input_Panel_Layout *layout)
{
    int layout_temp;

    connect_panel ();
    _imcontrol_client.prepare ();
    _imcontrol_client.get_layout (&layout_temp);

    *layout = (Ecore_IMF_Input_Panel_Layout)layout_temp;
    return 0;
}

int _isf_imf_context_input_panel_caps_mode_set (unsigned int mode)
{
    connect_panel ();

    _imcontrol_client.prepare ();
    _imcontrol_client.set_caps_mode (mode);
    _imcontrol_client.send ();
    return 0;
}

int _isf_imf_context_candidate_window_geometry_get (int *x, int *y, int *w, int *h)
{
    connect_panel ();
    _imcontrol_client.prepare ();
    _imcontrol_client.get_candidate_window_geometry (x, y, w, h);
    return 0;
}

int _isf_imf_context_control_focus_in (void)
{
    connect_panel ();
    _imcontrol_client.prepare ();
    _imcontrol_client.focus_in ();
    _imcontrol_client.send ();
    return 0;
}

int _isf_imf_context_control_focus_out (void)
{
    connect_panel ();
    _imcontrol_client.prepare ();
    _imcontrol_client.focus_out ();
    _imcontrol_client.send ();
    return 0;
}

int _isf_imf_context_input_panel_send_will_show_ack (void)
{
    connect_panel ();
    _imcontrol_client.prepare ();
    _imcontrol_client.send_will_show_ack ();
    _imcontrol_client.send ();
    return 0;
}

int _isf_imf_context_input_panel_send_will_hide_ack (void)
{
    connect_panel ();
    _imcontrol_client.prepare ();
    _imcontrol_client.send_will_hide_ack ();
    _imcontrol_client.send ();
    return 0;
}

/*
vi:ts=4:expandtab:nowrap
*/
