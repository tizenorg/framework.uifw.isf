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

#ifndef __ISF_IMCONTROL_CLIENT_H
#define __ISF_IMCONTROL_CLIENT_H

namespace scim
{

typedef Slot1<void, int> IMControlClientSlotVoid;


class EAPI IMControlClient
{
    class IMControlClientImpl;
    IMControlClientImpl *m_impl;

    IMControlClient (const IMControlClient &);
    const IMControlClient & operator = (const IMControlClient &);

public:
    IMControlClient ();
    ~IMControlClient ();

    int  open_connection        (void);
    void close_connection       (void);
    bool is_connected           (void) const;
    int  get_panel2imclient_connection_number (void) const;
    bool prepare                (void);
    bool send                   (void);

    void show_ise (int client_id, int context, void *data, int length, int *input_panel_show);
    void hide_ise (int client_id, int context);
    void show_control_panel (void);
    void hide_control_panel (void);
    void set_mode (int mode);

    void set_imdata (const char* data, int len);
    void get_imdata (char* data, int* len);
    void get_ise_window_geometry (int* x, int* y, int* width, int* height);
    void get_candidate_window_geometry (int* x, int* y, int* width, int* height);
    void get_ise_language_locale (char **locale);
    void set_return_key_type (int type);
    void get_return_key_type (int &type);
    void set_return_key_disable (int disabled);
    void get_return_key_disable (int &disabled);
    void set_layout (int layout);
    void get_layout (int* layout);
    void set_ise_language (int language);
    void set_active_ise_by_uuid (const char* uuid);
    void get_active_ise (String &uuid);
    void get_ise_list (int* count, char*** iselist);
    void get_ise_info (const char* uuid, String &name, String &language, int &type, int &option);
    void reset_ise_option (void);
    void set_caps_mode (int mode);
    void focus_in (void);
    void focus_out (void);
    void send_will_show_ack (void);
    void send_will_hide_ack (void);
    void set_active_ise_to_default (void);
    void send_candidate_will_hide_ack (void);
};

}

#endif /* __ISF_IMCONTROL_CLIENT_H */

/*
vi:ts=4:nowrap:ai:expandtab
*/
