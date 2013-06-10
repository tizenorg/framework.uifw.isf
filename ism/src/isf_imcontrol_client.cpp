/*
 * ISF(Input Service Framework)
 *
 * ISF is based on SCIM 1.4.7 and extended for supporting more mobile fitable.
 * Copyright (c) 2012-2013 Samsung Electronics Co., Ltd.
 *
 * Contact: Haifeng Deng <haifeng.deng@samsung.com>, Hengliang Luo <hl.luo@samsung.com>
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


#include <string.h>
#include <dlog.h>
#include "scim.h"

#define LOG_TAG "immodule"

#define IMCONTROLDBG(str...)
#define IMCONTROLERR(str...) LOGW(str)

namespace scim
{

typedef Signal1<void, int> IMControlClientSignalVoid;

class IMControlClient::IMControlClientImpl
{
    SocketClient                                m_socket_imclient2panel;
    SocketClient                                m_socket_panel2imclient;
    int                                         m_socket_timeout;
    uint32                                      m_socket_i2p_magic_key;
    uint32                                      m_socket_p2i_magic_key;
    Transaction                                 m_trans;

    IMControlClientSignalVoid                   m_signal_show_ise;
    IMControlClientSignalVoid                   m_signal_hide_ise;

public:
    IMControlClientImpl ()
          : m_socket_timeout (scim_get_default_socket_timeout ()),
            m_socket_i2p_magic_key (0),
            m_socket_p2i_magic_key (0) {
    }

    int open_connection (void) {
        String config = "";
        String display = String(getenv ("DISPLAY"));

        SocketAddress addr (scim_get_default_panel_socket_address (display));

        if (m_socket_imclient2panel.is_connected ()) close_connection ();

        bool ret=false, ret2=false;
        int count = 0;

        /* Try three times. */
        while (1) {
            ret = m_socket_imclient2panel.connect (addr);
            ret2 = m_socket_panel2imclient.connect (addr);
            if (!ret) {
                scim_usleep (100000);
                scim_launch_panel (true, config, display, NULL);
                for (int i = 0; i < 200; ++i) {
                    if (m_socket_imclient2panel.connect (addr)) {
                        ret = true;
                        break;
                    }
                    scim_usleep (100000);
                }
            }

            if (ret && scim_socket_open_connection (m_socket_i2p_magic_key, String ("IMControl_Active"), String ("Panel"), m_socket_imclient2panel, m_socket_timeout)) {
                if (ret2 && scim_socket_open_connection (m_socket_p2i_magic_key, String ("IMControl_Passive"), String ("Panel"), m_socket_panel2imclient, m_socket_timeout))
                    break;
            }
            m_socket_imclient2panel.close ();
            m_socket_panel2imclient.close ();

            if (count++ >= 3) break;

            scim_usleep (100000);
        }

        return m_socket_imclient2panel.get_id ();
    }

    void close_connection (void) {
        m_socket_imclient2panel.close ();
        m_socket_panel2imclient.close ();
        m_socket_i2p_magic_key = 0;
        m_socket_p2i_magic_key = 0;
    }

    bool is_connected (void) const {
        return (m_socket_imclient2panel.is_connected () && m_socket_panel2imclient.is_connected ());
    }

    int  get_panel2imclient_connection_number  (void) const {
        return m_socket_panel2imclient.get_id ();
    }

    bool prepare (void) {
        if (!m_socket_imclient2panel.is_connected ()) return false;

        m_trans.clear ();
        m_trans.put_command (SCIM_TRANS_CMD_REQUEST);
        m_trans.put_data (m_socket_i2p_magic_key);

        return true;
    }

    bool send (void) {
        if (!m_socket_imclient2panel.is_connected ()) return false;
        if (m_trans.get_data_type () != SCIM_TRANS_DATA_UNKNOWN)
            return m_trans.write_to_socket (m_socket_imclient2panel, 0x4d494353);
        return false;
    }


    void show_ise (int client_id, int context, void *data, int length, int *input_panel_show) {
        int cmd;
        uint32 temp;
        m_trans.put_command (ISM_TRANS_CMD_SHOW_ISE_PANEL);
        m_trans.put_data ((uint32)client_id);
        m_trans.put_data ((uint32)context);
        m_trans.put_data ((const char *)data, (size_t)length);
        m_trans.write_to_socket (m_socket_imclient2panel);
        if (!m_trans.read_from_socket (m_socket_imclient2panel, m_socket_timeout))
            IMCONTROLERR ("%s:: read_from_socket() may be timeout \n", __FUNCTION__);
        if (m_trans.get_command (cmd) && cmd == SCIM_TRANS_CMD_REPLY &&
                m_trans.get_command (cmd) && cmd == SCIM_TRANS_CMD_OK &&
                m_trans.get_data (temp)) {
            if (input_panel_show)
                *input_panel_show = temp;
        } else {
            IMCONTROLERR ("%s:: get_command() or get_data() may fail!!!\n", __FUNCTION__);
            if (input_panel_show)
                *input_panel_show = false;
        }
    }

    void hide_ise (int client_id, int context) {
        m_trans.put_command (ISM_TRANS_CMD_HIDE_ISE_PANEL);
        m_trans.put_data ((uint32)client_id);
        m_trans.put_data ((uint32)context);
    }

    void show_control_panel (void) {
        m_trans.put_command (ISM_TRANS_CMD_SHOW_ISF_CONTROL);
    }

    void hide_control_panel (void) {
        m_trans.put_command (ISM_TRANS_CMD_HIDE_ISF_CONTROL);
    }

    void set_mode (int mode) {
        m_trans.put_command (ISM_TRANS_CMD_SET_ISE_MODE);
        m_trans.put_data (mode);
    }

    void set_imdata (const char* data, int len) {
        m_trans.put_command (ISM_TRANS_CMD_SET_ISE_IMDATA);
        m_trans.put_data (data, len);
    }

    void get_imdata (char* data, int* len) {
        int cmd;
        size_t datalen = 0;
        char* data_temp = NULL;

        m_trans.put_command (ISM_TRANS_CMD_GET_ISE_IMDATA);
        m_trans.write_to_socket (m_socket_imclient2panel);
        if (!m_trans.read_from_socket (m_socket_imclient2panel, m_socket_timeout))
            IMCONTROLERR ("%s:: read_from_socket() may be timeout \n", __FUNCTION__);

        if (m_trans.get_command (cmd) && cmd == SCIM_TRANS_CMD_REPLY &&
                m_trans.get_command (cmd) && cmd == SCIM_TRANS_CMD_OK &&
                m_trans.get_data (&data_temp, datalen)) {
            memcpy (data, data_temp, datalen);
            *len = datalen;
        } else {
            IMCONTROLERR ("%s:: get_command() or get_data() may fail!!!\n", __FUNCTION__);
        }
        delete [] data_temp;
    }

    void get_ise_window_geometry (int* x, int* y, int* width, int* height) {
        int cmd;
        uint32 x_temp = 0;
        uint32 y_temp = 0;
        uint32 w_temp = 0;
        uint32 h_temp = 0;

        m_trans.put_command (ISM_TRANS_CMD_GET_ACTIVE_ISE_GEOMETRY);
        m_trans.write_to_socket (m_socket_imclient2panel);

        if (!m_trans.read_from_socket (m_socket_imclient2panel, m_socket_timeout))
            IMCONTROLERR ("%s:: read_from_socket() may be timeout \n", __FUNCTION__);

        if (m_trans.get_command (cmd) && cmd == SCIM_TRANS_CMD_REPLY &&
            m_trans.get_command (cmd) && cmd == SCIM_TRANS_CMD_OK &&
            m_trans.get_data (x_temp) &&
            m_trans.get_data (y_temp) &&
            m_trans.get_data (w_temp) &&
            m_trans.get_data (h_temp)) {
            *x = x_temp;
            *y = y_temp;
            *width = w_temp;
            *height = h_temp;
        } else {
            IMCONTROLERR ("%s:: get_command() or get_data() may fail!!!\n", __FUNCTION__);
        }
    }

    void get_candidate_window_geometry (int* x, int* y, int* width, int* height) {
        int cmd;
        uint32 x_temp = 0;
        uint32 y_temp = 0;
        uint32 w_temp = 0;
        uint32 h_temp = 0;

        m_trans.put_command (ISM_TRANS_CMD_GET_CANDIDATE_GEOMETRY);
        m_trans.write_to_socket (m_socket_imclient2panel);

        if (!m_trans.read_from_socket (m_socket_imclient2panel, m_socket_timeout))
            IMCONTROLERR ("%s::read_from_socket () may be timeout \n", __FUNCTION__);

        if (m_trans.get_command (cmd) && cmd == SCIM_TRANS_CMD_REPLY &&
            m_trans.get_command (cmd) && cmd == SCIM_TRANS_CMD_OK &&
            m_trans.get_data (x_temp) &&
            m_trans.get_data (y_temp) &&
            m_trans.get_data (w_temp) &&
            m_trans.get_data (h_temp)) {
            *x = x_temp;
            *y = y_temp;
            *width = w_temp;
            *height = h_temp;
        } else {
            IMCONTROLERR ("%s::get_command () or get_data () is failed!!!\n", __FUNCTION__);
        }
    }

    void get_ise_language_locale (char **locale) {
        int cmd;
        size_t datalen = 0;
        char  *data = NULL;

        m_trans.put_command (ISM_TRANS_CMD_GET_ISE_LANGUAGE_LOCALE);
        m_trans.write_to_socket (m_socket_imclient2panel);
        if (!m_trans.read_from_socket (m_socket_imclient2panel, m_socket_timeout))
            IMCONTROLERR ("%s::read_from_socket () may be timeout \n", __FUNCTION__);

        if (m_trans.get_command (cmd) && cmd == SCIM_TRANS_CMD_REPLY &&
            m_trans.get_command (cmd) && cmd == SCIM_TRANS_CMD_OK &&
            m_trans.get_data (&data, datalen)) {
            if (locale)
                *locale = strndup (data, datalen);
        } else {
            IMCONTROLERR ("%s::get_command () or get_data () is failed!!!\n", __FUNCTION__);
            if (locale)
                *locale = strdup ("");
        }
        if (data)
            delete [] data;
    }

    void set_return_key_type (int type) {
        m_trans.put_command (ISM_TRANS_CMD_SET_RETURN_KEY_TYPE);
        m_trans.put_data (type);
    }

    void get_return_key_type (int &type) {
        int cmd;
        uint32 temp;

        m_trans.put_command (ISM_TRANS_CMD_GET_RETURN_KEY_TYPE);
        m_trans.write_to_socket (m_socket_imclient2panel);
        if (!m_trans.read_from_socket (m_socket_imclient2panel, m_socket_timeout))
            IMCONTROLERR ("%s:: read_from_socket() may be timeout \n", __FUNCTION__);

        if (m_trans.get_command (cmd) && cmd == SCIM_TRANS_CMD_REPLY &&
                m_trans.get_command (cmd) && cmd == SCIM_TRANS_CMD_OK &&
                m_trans.get_data (temp)) {
            type = temp;
        } else {
            IMCONTROLERR ("%s:: get_command() or get_data() may fail!!!\n", __FUNCTION__);
        }
    }

    void set_return_key_disable (int disabled) {
        m_trans.put_command (ISM_TRANS_CMD_SET_RETURN_KEY_DISABLE);
        m_trans.put_data (disabled);
    }

    void get_return_key_disable (int &disabled) {
        int cmd;
        uint32 temp;

        m_trans.put_command (ISM_TRANS_CMD_GET_RETURN_KEY_DISABLE);
        m_trans.write_to_socket (m_socket_imclient2panel);
        if (!m_trans.read_from_socket (m_socket_imclient2panel, m_socket_timeout))
            IMCONTROLERR ("%s:: read_from_socket() may be timeout \n", __FUNCTION__);

        if (m_trans.get_command (cmd) && cmd == SCIM_TRANS_CMD_REPLY &&
                m_trans.get_command (cmd) && cmd == SCIM_TRANS_CMD_OK &&
                m_trans.get_data (temp)) {
            disabled = temp;
        } else {
            IMCONTROLERR ("%s:: get_command() or get_data() may fail!!!\n", __FUNCTION__);
        }
    }

    void set_layout (int layout) {
        m_trans.put_command (ISM_TRANS_CMD_SET_LAYOUT);
        m_trans.put_data (layout);
    }

    void get_layout (int* layout) {
        int cmd;
        uint32 layout_temp;

        m_trans.put_command (ISM_TRANS_CMD_GET_LAYOUT);
        m_trans.write_to_socket (m_socket_imclient2panel);
        if (!m_trans.read_from_socket (m_socket_imclient2panel, m_socket_timeout))
            IMCONTROLERR ("%s:: read_from_socket() may be timeout \n", __FUNCTION__);

        if (m_trans.get_command (cmd) && cmd == SCIM_TRANS_CMD_REPLY &&
                m_trans.get_command (cmd) && cmd == SCIM_TRANS_CMD_OK &&
                m_trans.get_data (layout_temp)) {
            *layout = layout_temp;
        } else {
            IMCONTROLERR ("%s:: get_command() or get_data() may fail!!!\n", __FUNCTION__);
        }
    }

    void set_ise_language (int language) {
        m_trans.put_command (ISM_TRANS_CMD_SET_ISE_LANGUAGE);
        m_trans.put_data (language);
    }

    void set_active_ise_by_uuid (const char* uuid) {
        int cmd;
        m_trans.put_command (ISM_TRANS_CMD_SET_ACTIVE_ISE_BY_UUID);
        m_trans.put_data (uuid, strlen(uuid)+1);
        m_trans.write_to_socket (m_socket_imclient2panel);
        if (!m_trans.read_from_socket (m_socket_imclient2panel, m_socket_timeout))
            IMCONTROLERR ("%s:: read_from_socket() may be timeout \n", __FUNCTION__);

        if (m_trans.get_command (cmd) && cmd == SCIM_TRANS_CMD_REPLY &&
                m_trans.get_command (cmd) && cmd == SCIM_TRANS_CMD_OK) {
        } else {
            IMCONTROLERR ("%s:: get_command() or get_data() may fail!!!\n", __FUNCTION__);
        }
    }

    void get_active_ise (String &uuid) {
        int    cmd;
        String strTemp;

        m_trans.put_command (ISM_TRANS_CMD_GET_ACTIVE_ISE);
        m_trans.write_to_socket (m_socket_imclient2panel);
        if (!m_trans.read_from_socket (m_socket_imclient2panel, m_socket_timeout))
            IMCONTROLERR ("%s:: read_from_socket() may be timeout \n", __FUNCTION__);

        if (m_trans.get_command (cmd) && cmd == SCIM_TRANS_CMD_REPLY &&
                m_trans.get_command (cmd) && cmd == SCIM_TRANS_CMD_OK &&
                m_trans.get_data (strTemp)) {
            uuid = strTemp;
        } else {
            IMCONTROLERR ("%s:: get_command() or get_data() may fail!!!\n", __FUNCTION__);
        }
    }

    void get_ise_list (int* count, char*** iselist) {
        int cmd;
        uint32 count_temp = 0;
        char **buf = NULL;
        size_t len;
        char * buf_temp = NULL;

        m_trans.put_command (ISM_TRANS_CMD_GET_ISE_LIST);
        m_trans.write_to_socket (m_socket_imclient2panel);
        if (!m_trans.read_from_socket (m_socket_imclient2panel, m_socket_timeout))
            IMCONTROLERR ("%s:: read_from_socket() may be timeout \n", __FUNCTION__);

        if (m_trans.get_command (cmd) && cmd == SCIM_TRANS_CMD_REPLY &&
                m_trans.get_command (cmd) && cmd == SCIM_TRANS_CMD_OK &&
                m_trans.get_data (count_temp) ) {
            *count = count_temp;
        } else {
            *count = 0;
            IMCONTROLERR ("%s:: get_command() or get_data() may fail!!!\n", __FUNCTION__);
        }

        if (count_temp > 0) {
            buf = (char**)malloc (count_temp * sizeof (char*));
            if (buf) {
                memset (buf, 0, count_temp * sizeof (char*));
                for (uint32 i = 0; i < count_temp; i++) {
                    if (m_trans.get_data (&buf_temp, len))
                        buf[i] = buf_temp;
                }
            }
        }
        *iselist = buf;
    }

    void get_ise_info (const char* uuid, String &name, String &language, int &type, int &option)
    {
        int    cmd;
        uint32 tmp_type, tmp_option;
        String tmp_name, tmp_language;

        m_trans.put_command (ISM_TRANS_CMD_GET_ISE_INFORMATION);
        m_trans.put_data (String (uuid));
        m_trans.write_to_socket (m_socket_imclient2panel);
        if (!m_trans.read_from_socket (m_socket_imclient2panel, m_socket_timeout))
            IMCONTROLERR ("%s:: read_from_socket() may be timeout \n", __FUNCTION__);

        if (m_trans.get_command (cmd) && cmd == SCIM_TRANS_CMD_REPLY &&
                m_trans.get_command (cmd) && cmd == SCIM_TRANS_CMD_OK &&
                m_trans.get_data (tmp_name) && m_trans.get_data (tmp_language) &&
                m_trans.get_data (tmp_type) && m_trans.get_data (tmp_option)) {
            name     = tmp_name;
            language = tmp_language;
            type     = tmp_type;
            option   = tmp_option;
        } else {
            IMCONTROLERR ("%s:: get_command() or get_data() may fail!!!\n", __FUNCTION__);
        }
    }

    void reset_ise_option (void) {
        int cmd;

        m_trans.put_command (ISM_TRANS_CMD_RESET_ISE_OPTION);
        m_trans.write_to_socket (m_socket_imclient2panel);
        if (!m_trans.read_from_socket (m_socket_imclient2panel, m_socket_timeout))
            IMCONTROLERR ("%s:: read_from_socket() may be timeout \n", __FUNCTION__);

        if (m_trans.get_command (cmd) && cmd == SCIM_TRANS_CMD_REPLY &&
                m_trans.get_command (cmd) && cmd == SCIM_TRANS_CMD_OK) {
            ;
        } else {
            IMCONTROLERR ("%s:: get_command() is failed!!!\n", __FUNCTION__);
        }
    }

    void set_caps_mode (int mode) {
        m_trans.put_command (ISM_TRANS_CMD_SET_CAPS_MODE);
        m_trans.put_data (mode);
    }

    void focus_in (void) {
        m_trans.put_command (SCIM_TRANS_CMD_FOCUS_IN);
    }

    void focus_out (void) {
        m_trans.put_command (SCIM_TRANS_CMD_FOCUS_OUT);
    }

    void send_will_show_ack (void) {
        m_trans.put_command (ISM_TRANS_CMD_SEND_WILL_SHOW_ACK);
    }

    void send_will_hide_ack (void) {
        m_trans.put_command (ISM_TRANS_CMD_SEND_WILL_HIDE_ACK);
    }

    void set_active_ise_to_default (void) {
        m_trans.put_command (ISM_TRANS_CMD_RESET_DEFAULT_ISE);
    }
};

IMControlClient::IMControlClient ()
        : m_impl (new IMControlClientImpl ())
{
}

IMControlClient::~IMControlClient ()
{
    delete m_impl;
}

int
IMControlClient::open_connection (void)
{
    return m_impl->open_connection ();
}

void
IMControlClient::close_connection (void)
{
    m_impl->close_connection ();
}

bool IMControlClient::is_connected (void) const
{
    return m_impl->is_connected ();
}

int IMControlClient::get_panel2imclient_connection_number (void) const
{
    return m_impl->get_panel2imclient_connection_number ();
}

bool
IMControlClient::prepare (void)
{
    return m_impl->prepare ();
}

bool
IMControlClient::send (void)
{
    return m_impl->send ();
}

void IMControlClient::show_ise (int client_id, int context, void *data, int length, int *input_panel_show)
{
    m_impl->show_ise (client_id, context, data,length, input_panel_show);
}

void IMControlClient::hide_ise (int client_id, int context)
{
    m_impl->hide_ise (client_id, context);
}

void IMControlClient::show_control_panel (void)
{
    m_impl->show_control_panel ();
}

void IMControlClient::hide_control_panel (void)
{
    m_impl->hide_control_panel ();
}

void IMControlClient::set_mode (int mode)
{
    m_impl->set_mode (mode);
}

void IMControlClient::set_imdata (const char* data, int len)
{
    m_impl->set_imdata (data, len);
}

void IMControlClient::get_imdata (char* data, int* len)
{
    m_impl->get_imdata (data, len);
}

void IMControlClient::get_ise_window_geometry (int* x, int* y, int* width, int* height)
{
    m_impl->get_ise_window_geometry (x, y, width, height);
}

void IMControlClient::get_candidate_window_geometry (int* x, int* y, int* width, int* height)
{
    m_impl->get_candidate_window_geometry (x, y, width, height);
}

void IMControlClient::get_ise_language_locale (char **locale)
{
    m_impl->get_ise_language_locale (locale);
}

void IMControlClient::set_return_key_type (int type)
{
    m_impl->set_return_key_type (type);
}

void IMControlClient::get_return_key_type (int &type)
{
    m_impl->get_return_key_type (type);
}

void IMControlClient::set_return_key_disable (int disabled)
{
    m_impl->set_return_key_disable (disabled);
}

void IMControlClient::get_return_key_disable (int &disabled)
{
    m_impl->get_return_key_disable (disabled);
}

void IMControlClient::set_layout (int layout)
{
    m_impl->set_layout (layout);
}

void IMControlClient::get_layout (int* layout)
{
    m_impl->get_layout (layout);
}

void IMControlClient::set_ise_language (int language)
{
    m_impl->set_ise_language (language);
}

void IMControlClient::set_active_ise_by_uuid (const char* uuid)
{
    m_impl->set_active_ise_by_uuid (uuid);
}

void IMControlClient::get_active_ise (String &uuid)
{
    m_impl->get_active_ise (uuid);
}

void IMControlClient::get_ise_list (int* count, char*** iselist)
{
    m_impl->get_ise_list (count, iselist);
}

void IMControlClient::get_ise_info (const char* uuid, String &name, String &language, int &type, int &option)
{
    m_impl->get_ise_info (uuid, name, language, type, option);
}

void IMControlClient::reset_ise_option (void)
{
    m_impl->reset_ise_option ();
}

void IMControlClient::set_caps_mode (int mode)
{
    m_impl->set_caps_mode (mode);
}

void IMControlClient::focus_in (void)
{
    m_impl->focus_in ();
}

void IMControlClient::focus_out (void)
{
    m_impl->focus_out ();
}

void IMControlClient::send_will_show_ack (void)
{
    m_impl->send_will_show_ack ();
}

void IMControlClient::send_will_hide_ack (void)
{
    m_impl->send_will_hide_ack ();
}

void IMControlClient::set_active_ise_to_default (void)
{
    m_impl->set_active_ise_to_default ();
}

};

/*
vi:ts=4:nowrap:ai:expandtab
*/
