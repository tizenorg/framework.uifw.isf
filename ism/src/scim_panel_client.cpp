/** @file scim_panel_client.cpp
 *  @brief Implementation of class PanelClient.
 */

/* ISF is based on SCIM 1.4.7 and extended for supporting more mobile fitable. */

/*
 * Smart Common Input Method
 *
 * Copyright (c) 2005 James Su <suzhe@tsinghua.org.cn>
 *
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA  02111-1307  USA
 *
 * $Id: scim_panel_client.cpp,v 1.6 2005/06/26 16:35:33 suzhe Exp $
 *
 */

#define Uses_SCIM_TRANSACTION
#define Uses_SCIM_TRANS_COMMANDS
#define Uses_SCIM_PANEL_CLIENT
#define Uses_SCIM_SOCKET
#define Uses_SCIM_EVENT

#include "scim_private.h"
#include "scim.h"

namespace scim {

typedef Signal1<void, int>
        PanelClientSignalVoid;

typedef Signal2<void, int, int>
        PanelClientSignalInt;

typedef Signal3<void, int, int, int>
        PanelClientSignalIntInt;

typedef Signal2<void, int, const String &>
        PanelClientSignalString;

typedef Signal2<void, int, const WideString &>
        PanelClientSignalWideString;

typedef Signal4<void, int, const String &, const String &, const Transaction &>
        PanelClientSignalStringStringTransaction;

typedef Signal2<void, int, const KeyEvent &>
        PanelClientSignalKeyEvent;

typedef Signal3<void, int, const WideString &, const AttributeList &>
        PanelClientSignalStringAttrs;

class PanelClient::PanelClientImpl
{
    SocketClient                                m_socket;
    int                                         m_socket_timeout;
    uint32                                      m_socket_magic_key;
    Transaction                                 m_send_trans;
    int                                         m_current_icid;
    int                                         m_send_refcount;

    PanelClientSignalVoid                       m_signal_reload_config;
    PanelClientSignalVoid                       m_signal_exit;
    PanelClientSignalInt                        m_signal_update_lookup_table_page_size;
    PanelClientSignalVoid                       m_signal_lookup_table_page_up;
    PanelClientSignalVoid                       m_signal_lookup_table_page_down;
    PanelClientSignalVoid                       m_signal_reset_option;
    PanelClientSignalString                     m_signal_trigger_property;
    PanelClientSignalStringStringTransaction    m_signal_process_helper_event;
    PanelClientSignalInt                        m_signal_move_preedit_caret;
    PanelClientSignalInt                        m_signal_update_preedit_caret;
    PanelClientSignalInt                        m_signal_select_aux;
    PanelClientSignalInt                        m_signal_select_candidate;
    PanelClientSignalKeyEvent                   m_signal_process_key_event;
    PanelClientSignalWideString                 m_signal_commit_string;
    PanelClientSignalKeyEvent                   m_signal_forward_key_event;
    PanelClientSignalVoid                       m_signal_request_help;
    PanelClientSignalVoid                       m_signal_request_factory_menu;
    PanelClientSignalString                     m_signal_change_factory;
    PanelClientSignalVoid                       m_signal_reset_keyboard_ise;
    PanelClientSignalVoid                       m_signal_update_keyboard_ise;
    PanelClientSignalVoid                       m_signal_show_preedit_string;
    PanelClientSignalVoid                       m_signal_hide_preedit_string;
    PanelClientSignalStringAttrs                m_signal_update_preedit_string;
    PanelClientSignalIntInt                     m_signal_get_surrounding_text;
    PanelClientSignalIntInt                     m_signal_delete_surrounding_text;

public:
    PanelClientImpl ()
        : m_socket_timeout (scim_get_default_socket_timeout ()),
          m_socket_magic_key (0),
          m_current_icid (-1),
          m_send_refcount (0)
    {
    }

    int  open_connection        (const String &config, const String &display)
    {
        String panel_address = scim_get_default_panel_socket_address (display);
        SocketAddress addr (panel_address);

        if (m_socket.is_connected ()) close_connection ();

        bool ret;
        int  i, count = 0;

        /* Try three times. */
        while (1) {
            ret = m_socket.connect (addr);
            if (ret == false) {
                scim_usleep (100000);
                launch_panel (config, display);
                std::cerr << " Re-connecting to PanelAgent server.";
                ISF_LOG (" Re-connecting to PanelAgent server.\n");
                for (i = 0; i < 200; ++i) {
                    if (m_socket.connect (addr)) {
                        ret = true;
                        break;
                    }
                    std::cerr << ".";
                    scim_usleep (200000);
                }
                std::cerr << " Connected :" << i << "\n";
                ISF_LOG ("  Connected :%d\n", i);
                if (m_socket.connect (addr) == false && count >= 2)
                    break;
            }

            if (ret && scim_socket_open_connection (m_socket_magic_key, String ("FrontEnd"), String ("Panel"), m_socket, m_socket_timeout)) {
                ISF_LOG (" PID=%d connect to PanelAgent (%s), connected:%d.\n", getpid (), panel_address.c_str (), count);
                break;
            } else {
                std::cerr << " PID=" << getpid () << " cannot connect to PanelAgent (" << panel_address << "), connected:" << count << ".\n";
                ISF_LOG (" PID=%d cannot connect to PanelAgent (%s), connected:%d.\n", getpid (), panel_address.c_str (), count);
            }

            m_socket.close ();

            if (count++ >= 400) break;

            scim_usleep (300000);
        }

        return m_socket.get_id ();
    }

    void close_connection       ()
    {
        m_socket.close ();
        m_socket_magic_key = 0;
    }

    int  get_connection_number  () const
    {
        return m_socket.get_id ();
    }

    bool is_connected           () const
    {
        return m_socket.is_connected ();
    }

    bool has_pending_event      () const
    {
        return m_socket.is_connected () && m_socket.wait_for_data (0) > 0;
    }

    bool filter_event           ()
    {
        Transaction recv;

        if (!m_socket.is_connected () || !recv.read_from_socket (m_socket, m_socket_timeout))
            return false;

        int cmd;
        uint32 context = (uint32)(-1);

        if (!recv.get_command (cmd) || cmd != SCIM_TRANS_CMD_REPLY)
            return true;

        /* No context id available, so there will be some global command. */
        if (recv.get_data_type () == SCIM_TRANS_DATA_COMMAND) {
            while (recv.get_command (cmd)) {
                switch (cmd) {
                    case SCIM_TRANS_CMD_RELOAD_CONFIG:
                        m_signal_reload_config ((int)context);
                        break;
                    case SCIM_TRANS_CMD_EXIT:
                        m_signal_exit ((int)context);
                        break;
                    default:
                        break;
                }
            }
            return true;
        }

        /* Now for context related command. */
        if (!recv.get_data (context))
            return true;

        while (recv.get_command (cmd)) {
            switch (cmd) {
                case SCIM_TRANS_CMD_UPDATE_LOOKUP_TABLE_PAGE_SIZE:
                    {
                        uint32 size;
                        if (recv.get_data (size))
                            m_signal_update_lookup_table_page_size ((int) context, (int) size);
                    }
                    break;
                case SCIM_TRANS_CMD_LOOKUP_TABLE_PAGE_UP:
                    {
                        m_signal_lookup_table_page_up ((int) context);
                    }
                    break;
                case SCIM_TRANS_CMD_LOOKUP_TABLE_PAGE_DOWN:
                    {
                        m_signal_lookup_table_page_down ((int) context);
                    }
                    break;
                case ISM_TRANS_CMD_RESET_ISE_OPTION:
                    {
                        m_signal_reset_option ((int) context);
                    }
                    break;
                case SCIM_TRANS_CMD_TRIGGER_PROPERTY:
                    {
                        String property;
                        if (recv.get_data (property))
                            m_signal_trigger_property ((int) context, property);
                    }
                    break;
                case SCIM_TRANS_CMD_PROCESS_HELPER_EVENT:
                    {
                        String target_uuid;
                        String helper_uuid;
                        Transaction trans;
                        if (recv.get_data (target_uuid) && recv.get_data (helper_uuid) && recv.get_data (trans))
                            m_signal_process_helper_event ((int) context, target_uuid, helper_uuid, trans);
                    }
                    break;
                case SCIM_TRANS_CMD_MOVE_PREEDIT_CARET:
                    {
                        uint32 caret;
                        if (recv.get_data (caret))
                            m_signal_move_preedit_caret ((int) context, (int) caret);
                    }
                    break;
                case ISM_TRANS_CMD_SELECT_AUX:
                    {
                        uint32 item;
                        if (recv.get_data (item))
                            m_signal_select_aux ((int) context, (int) item);
                    }
                    break;
                case SCIM_TRANS_CMD_SELECT_CANDIDATE:
                    {
                        uint32 item;
                        if (recv.get_data (item))
                            m_signal_select_candidate ((int) context, (int) item);
                    }
                    break;
                case SCIM_TRANS_CMD_PROCESS_KEY_EVENT:
                    {
                        KeyEvent key;
                        if (recv.get_data (key))
                            m_signal_process_key_event ((int) context, key);
                    }
                    break;
                case SCIM_TRANS_CMD_COMMIT_STRING:
                    {
                        WideString wstr;
                        if (recv.get_data (wstr))
                            m_signal_commit_string ((int) context, wstr);
                    }
                    break;
                case SCIM_TRANS_CMD_FORWARD_KEY_EVENT:
                    {
                        KeyEvent key;
                        if (recv.get_data (key))
                            m_signal_forward_key_event ((int) context, key);
                    }
                    break;
                case SCIM_TRANS_CMD_PANEL_REQUEST_HELP:
                    {
                        m_signal_request_help ((int) context);
                    }
                    break;
                case SCIM_TRANS_CMD_PANEL_REQUEST_FACTORY_MENU:
                    {
                        m_signal_request_factory_menu ((int) context);
                    }
                    break;
                case SCIM_TRANS_CMD_PANEL_CHANGE_FACTORY:
                    {
                        String sfid;
                        if (recv.get_data (sfid))
                            m_signal_change_factory ((int) context, sfid);
                    }
                    break;
                case ISM_TRANS_CMD_PANEL_REQUEST_RESET_ISE:
                    {
                        m_signal_reset_keyboard_ise ((int) context);
                    }
                    break;
                case ISM_TRANS_CMD_PANEL_UPDATE_KEYBOARD_ISE:
                    {
                        m_signal_update_keyboard_ise ((int) context);
                    }
                    break;
                case SCIM_TRANS_CMD_SHOW_PREEDIT_STRING:
                    {
                        m_signal_show_preedit_string ((int) context);
                    }
                    break;
                case SCIM_TRANS_CMD_HIDE_PREEDIT_STRING:
                    {
                        m_signal_hide_preedit_string ((int) context);
                    }
                    break;
                case SCIM_TRANS_CMD_UPDATE_PREEDIT_STRING:
                    {
                        WideString wstr;
                        AttributeList attrs;
                        if (recv.get_data (wstr) && recv.get_data (attrs))
                            m_signal_update_preedit_string ((int) context, wstr, attrs);
                    }
                    break;
                case SCIM_TRANS_CMD_UPDATE_PREEDIT_CARET:
                    {
                        uint32 caret;
                        if (recv.get_data (caret))
                            m_signal_update_preedit_caret ((int) context, (int) caret);
                    }
                    break;
                case ISM_TRANS_CMD_TURN_ON_LOG:
                    {
                        printf ("<%s:%d>receive ISM_TRANS_CMD_TURN_ON_LOG\n", __FUNCTION__, __LINE__);
                        uint32 isOn;
                        if (recv.get_data (isOn)) {
                            if (isOn) {
                                printf ("<%s:%d>turn on log\n", __FUNCTION__, __LINE__);
                                DebugOutput::enable_debug (SCIM_DEBUG_AllMask);
                                DebugOutput::set_verbose_level (7);
                            } else {
                                printf ("<%s:%d>turn off log\n", __FUNCTION__, __LINE__);
                                DebugOutput::disable_debug (SCIM_DEBUG_AllMask);
                                DebugOutput::set_verbose_level (0);
                            }
                        }
                    }
                    break;
                case SCIM_TRANS_CMD_GET_SURROUNDING_TEXT:
                    {
                        uint32 maxlen_before;
                        uint32 maxlen_after;
                        if (recv.get_data (maxlen_before) && recv.get_data (maxlen_after))
                            m_signal_get_surrounding_text ((int) context, (int)maxlen_before, (int)maxlen_after);
                    }
                    break;
                case SCIM_TRANS_CMD_DELETE_SURROUNDING_TEXT:
                    {
                        uint32 offset;
                        uint32 len;
                        if (recv.get_data (offset) && recv.get_data (len))
                            m_signal_delete_surrounding_text ((int) context, (int)offset, (int)len);
                    }
                    break;
                default:
                    break;
            }
        }
        return true;
    }

    bool prepare                (int icid)
    {
        if (!m_socket.is_connected ()) return false;

        int cmd;
        uint32 data;

        if (m_send_refcount <= 0) {
            m_current_icid = icid;
            m_send_trans.clear ();
            m_send_trans.put_command (SCIM_TRANS_CMD_REQUEST);
            m_send_trans.put_data (m_socket_magic_key);
            m_send_trans.put_data ((uint32) icid);

            if (m_send_trans.get_command (cmd) &&
                m_send_trans.get_data (data) &&
                m_send_trans.get_data (data))
                m_send_refcount = 0;
            else
                return false;
        }

        if (m_current_icid == icid) {
            m_send_refcount ++;
            return true;
        }
        return false;
    }

    bool send                   ()
    {
        if (!m_socket.is_connected ()) return false;

        if (m_send_refcount <= 0) return false;

        m_send_refcount --;

        if (m_send_refcount > 0) return false;

        if (m_send_trans.get_data_type () != SCIM_TRANS_DATA_UNKNOWN)
            return m_send_trans.write_to_socket (m_socket, 0x4d494353);

        return false;
    }

public:
    void turn_on                (int icid)
    {
        if (m_send_refcount > 0 && m_current_icid == icid)
            m_send_trans.put_command (SCIM_TRANS_CMD_PANEL_TURN_ON);
    }
    void turn_off               (int icid)
    {
        if (m_send_refcount > 0 && m_current_icid == icid)
            m_send_trans.put_command (SCIM_TRANS_CMD_PANEL_TURN_OFF);
    }
    void update_screen          (int icid, int screen)
    {
        if (m_send_refcount > 0 && m_current_icid == icid) {
            m_send_trans.put_command (SCIM_TRANS_CMD_UPDATE_SCREEN);
            m_send_trans.put_data ((uint32) screen);
        }
    }
    void show_help              (int icid, const String &help)
    {
        if (m_send_refcount > 0 && m_current_icid == icid) {
            m_send_trans.put_command (SCIM_TRANS_CMD_PANEL_SHOW_HELP);
            m_send_trans.put_data (help);
        }
    }
    void show_factory_menu      (int icid, const std::vector <PanelFactoryInfo> &menu)
    {
        if (m_send_refcount > 0 && m_current_icid == icid) {
            m_send_trans.put_command (SCIM_TRANS_CMD_PANEL_SHOW_FACTORY_MENU);
            for (size_t i = 0; i < menu.size (); ++ i) {
                m_send_trans.put_data (menu [i].uuid);
                m_send_trans.put_data (menu [i].name);
                m_send_trans.put_data (menu [i].lang);
                m_send_trans.put_data (menu [i].icon);
            }
        }
    }
    void focus_in               (int icid, const String &uuid)
    {
        if (m_send_refcount > 0 && m_current_icid == icid) {
            m_send_trans.put_command (SCIM_TRANS_CMD_FOCUS_IN);
            m_send_trans.put_data (uuid);
        }
    }
    void focus_out              (int icid)
    {
        if (m_send_refcount > 0 && m_current_icid == icid)
            m_send_trans.put_command (SCIM_TRANS_CMD_FOCUS_OUT);
    }
    void update_factory_info    (int icid, const PanelFactoryInfo &info)
    {
        if (m_send_refcount > 0 && m_current_icid == icid) {
            m_send_trans.put_command (SCIM_TRANS_CMD_PANEL_UPDATE_FACTORY_INFO);
            m_send_trans.put_data (info.uuid);
            m_send_trans.put_data (info.name);
            m_send_trans.put_data (info.lang);
            m_send_trans.put_data (info.icon);
        }
    }
    void update_spot_location   (int icid, int x, int y, int top_y)
    {
        if (m_send_refcount > 0 && m_current_icid == icid) {
            m_send_trans.put_command (SCIM_TRANS_CMD_UPDATE_SPOT_LOCATION);
            m_send_trans.put_data ((uint32) x);
            m_send_trans.put_data ((uint32) y);
            m_send_trans.put_data ((uint32) top_y);
        }
    }
    void update_cursor_position (int icid, int cursor_pos)
    {
        if (m_send_refcount > 0 && m_current_icid == icid) {
            m_send_trans.put_command (ISM_TRANS_CMD_UPDATE_CURSOR_POSITION);
            m_send_trans.put_data ((uint32) cursor_pos);
        }
    }
    void update_surrounding_text (int icid, const WideString &str, int cursor)
    {
        if (m_send_refcount > 0 && m_current_icid == icid) {
            m_send_trans.put_command (ISM_TRANS_CMD_UPDATE_SURROUNDING_TEXT);
            m_send_trans.put_data (utf8_wcstombs (str));
            m_send_trans.put_data ((uint32) cursor);
        }
    }
    void show_preedit_string    (int icid)
    {
        if (m_send_refcount > 0 && m_current_icid == icid)
            m_send_trans.put_command (SCIM_TRANS_CMD_SHOW_PREEDIT_STRING);
    }
    void show_aux_string        (int icid)
    {
        if (m_send_refcount > 0 && m_current_icid == icid)
            m_send_trans.put_command (SCIM_TRANS_CMD_SHOW_AUX_STRING);
    }
    void show_lookup_table      (int icid)
    {
        if (m_send_refcount > 0 && m_current_icid == icid)
            m_send_trans.put_command (SCIM_TRANS_CMD_SHOW_LOOKUP_TABLE);
    }
    void hide_preedit_string    (int icid)
    {
        if (m_send_refcount > 0 && m_current_icid == icid)
            m_send_trans.put_command (SCIM_TRANS_CMD_HIDE_PREEDIT_STRING);
    }
    void hide_aux_string        (int icid)
    {
        if (m_send_refcount > 0 && m_current_icid == icid)
            m_send_trans.put_command (SCIM_TRANS_CMD_HIDE_AUX_STRING);
    }
    void hide_lookup_table      (int icid)
    {
        if (m_send_refcount > 0 && m_current_icid == icid)
            m_send_trans.put_command (SCIM_TRANS_CMD_HIDE_LOOKUP_TABLE);
    }
    void update_preedit_string  (int icid, const WideString &str, const AttributeList &attrs)
    {
        if (m_send_refcount > 0 && m_current_icid == icid) {
            m_send_trans.put_command (SCIM_TRANS_CMD_UPDATE_PREEDIT_STRING);
            m_send_trans.put_data (utf8_wcstombs (str));
            m_send_trans.put_data (attrs);
        }
    }
    void update_preedit_caret   (int icid, int caret)
    {
        if (m_send_refcount > 0 && m_current_icid == icid) {
            m_send_trans.put_command (SCIM_TRANS_CMD_UPDATE_PREEDIT_CARET);
            m_send_trans.put_data ((uint32) caret);
        }
    }
    void update_aux_string      (int icid, const WideString &str, const AttributeList &attrs)
    {
        if (m_send_refcount > 0 && m_current_icid == icid) {
            m_send_trans.put_command (SCIM_TRANS_CMD_UPDATE_AUX_STRING);
            m_send_trans.put_data (utf8_wcstombs (str));
            m_send_trans.put_data (attrs);
        }
    }
    void update_lookup_table    (int icid, const LookupTable &table)
    {
        if (m_send_refcount > 0 && m_current_icid == icid) {
            m_send_trans.put_command (SCIM_TRANS_CMD_UPDATE_LOOKUP_TABLE);
            m_send_trans.put_data (table);
        }
    }
    void register_properties    (int icid, const PropertyList &properties)
    {
        if (m_send_refcount > 0 && m_current_icid == icid) {
            m_send_trans.put_command (SCIM_TRANS_CMD_REGISTER_PROPERTIES);
            m_send_trans.put_data (properties);
        }
    }
    void update_property        (int icid, const Property &property)
    {
        if (m_send_refcount > 0 && m_current_icid == icid) {
            m_send_trans.put_command (SCIM_TRANS_CMD_UPDATE_PROPERTY);
            m_send_trans.put_data (property);
        }
    }
    void start_default_ise      (int icid)
    {
        if (m_send_refcount > 0 && m_current_icid == icid) {
            m_send_trans.put_command (ISM_TRANS_CMD_PANEL_START_DEFAULT_ISE);
        }
    }
    void start_helper           (int icid, const String &helper_uuid)
    {
        if (m_send_refcount > 0 && m_current_icid == icid) {
            m_send_trans.put_command (SCIM_TRANS_CMD_START_HELPER);
            m_send_trans.put_data (helper_uuid);
        }
    }

    void stop_helper            (int icid, const String &helper_uuid)
    {
        if (m_send_refcount > 0 && m_current_icid == icid) {
            m_send_trans.put_command (SCIM_TRANS_CMD_STOP_HELPER);
            m_send_trans.put_data (helper_uuid);
        }
    }
    void send_helper_event      (int icid, const String &helper_uuid, const Transaction &trans)
    {
        if (m_send_refcount > 0 && m_current_icid == icid) {
            m_send_trans.put_command (SCIM_TRANS_CMD_SEND_HELPER_EVENT);
            m_send_trans.put_data (helper_uuid);
            m_send_trans.put_data (trans);
        }
    }
    void register_input_context (int icid, const String &uuid)
    {
        if (m_send_refcount > 0 && m_current_icid == icid) {
            m_send_trans.put_command (SCIM_TRANS_CMD_PANEL_REGISTER_INPUT_CONTEXT);
            m_send_trans.put_data (uuid);
        }
    }
    void remove_input_context   (int icid)
    {
        if (m_send_refcount > 0 && m_current_icid == icid)
            m_send_trans.put_command (SCIM_TRANS_CMD_PANEL_REMOVE_INPUT_CONTEXT);
    }
    void reset_input_context    (int icid)
    {
        if (m_send_refcount > 0 && m_current_icid == icid)
            m_send_trans.put_command (SCIM_TRANS_CMD_PANEL_RESET_INPUT_CONTEXT);
    }
    void turn_on_log            (int icid, uint32 isOn)
    {
        m_send_trans.put_command (ISM_TRANS_CMD_TURN_ON_LOG);
        m_send_trans.put_data (isOn);
    }

public:
    void reset_signal_handler                               (void)
    {
        m_signal_reload_config.reset();
        m_signal_exit.reset();
        m_signal_update_lookup_table_page_size.reset();
        m_signal_lookup_table_page_up.reset();
        m_signal_lookup_table_page_down.reset();
        m_signal_reset_option.reset();
        m_signal_trigger_property.reset();
        m_signal_process_helper_event.reset();
        m_signal_move_preedit_caret.reset();
        m_signal_update_preedit_caret.reset ();
        m_signal_select_aux.reset();
        m_signal_select_candidate.reset();
        m_signal_process_key_event.reset();
        m_signal_commit_string.reset();
        m_signal_forward_key_event.reset();
        m_signal_request_help.reset();
        m_signal_request_factory_menu.reset();
        m_signal_change_factory.reset();
        m_signal_reset_keyboard_ise.reset();
        m_signal_update_keyboard_ise.reset();
        m_signal_show_preedit_string.reset();
        m_signal_hide_preedit_string.reset();
        m_signal_update_preedit_string.reset();
        m_signal_get_surrounding_text.reset();
        m_signal_delete_surrounding_text.reset();
    }

    Connection signal_connect_reload_config                 (PanelClientSlotVoid                    *slot)
    {
        return m_signal_reload_config.connect (slot);
    }
    Connection signal_connect_exit                          (PanelClientSlotVoid                    *slot)
    {
        return m_signal_exit.connect (slot);
    }
    Connection signal_connect_update_lookup_table_page_size (PanelClientSlotInt                     *slot)
    {
        return m_signal_update_lookup_table_page_size.connect (slot);
    }
    Connection signal_connect_lookup_table_page_up          (PanelClientSlotVoid                    *slot)
    {
        return m_signal_lookup_table_page_up.connect (slot);
    }
    Connection signal_connect_lookup_table_page_down        (PanelClientSlotVoid                    *slot)
    {
        return m_signal_lookup_table_page_down.connect (slot);
    }
    Connection signal_connect_reset_option                  (PanelClientSlotVoid                    *slot)
    {
        return m_signal_reset_option.connect (slot);
    }
    Connection signal_connect_trigger_property              (PanelClientSlotString                  *slot)
    {
        return m_signal_trigger_property.connect (slot);
    }
    Connection signal_connect_process_helper_event          (PanelClientSlotStringStringTransaction *slot)
    {
        return m_signal_process_helper_event.connect (slot);
    }
    Connection signal_connect_move_preedit_caret            (PanelClientSlotInt                     *slot)
    {
        return m_signal_move_preedit_caret.connect (slot);
    }
    Connection signal_connect_update_preedit_caret          (PanelClientSlotInt                     *slot)
    {
        return m_signal_update_preedit_caret.connect (slot);
    }
    Connection signal_connect_select_aux                    (PanelClientSlotInt                     *slot)
    {
        return m_signal_select_aux.connect (slot);
    }
    Connection signal_connect_select_candidate              (PanelClientSlotInt                     *slot)
    {
        return m_signal_select_candidate.connect (slot);
    }
    Connection signal_connect_process_key_event             (PanelClientSlotKeyEvent                *slot)
    {
        return m_signal_process_key_event.connect (slot);
    }
    Connection signal_connect_commit_string                 (PanelClientSlotWideString              *slot)
    {
        return m_signal_commit_string.connect (slot);
    }
    Connection signal_connect_forward_key_event             (PanelClientSlotKeyEvent                *slot)
    {
        return m_signal_forward_key_event.connect (slot);
    }
    Connection signal_connect_request_help                  (PanelClientSlotVoid                    *slot)
    {
        return m_signal_request_help.connect (slot);
    }
    Connection signal_connect_request_factory_menu          (PanelClientSlotVoid                    *slot)
    {
        return m_signal_request_factory_menu.connect (slot);
    }
    Connection signal_connect_change_factory                (PanelClientSlotString                  *slot)
    {
        return m_signal_change_factory.connect (slot);
    }

    Connection signal_connect_reset_keyboard_ise            (PanelClientSlotVoid                    *slot)
    {
        return m_signal_reset_keyboard_ise.connect (slot);
    }

    Connection signal_connect_update_keyboard_ise           (PanelClientSlotVoid                    *slot)
    {
        return m_signal_update_keyboard_ise.connect (slot);
    }

    Connection signal_connect_show_preedit_string           (PanelClientSlotVoid                    *slot)
    {
        return m_signal_show_preedit_string.connect (slot);
    }

    Connection signal_connect_hide_preedit_string           (PanelClientSlotVoid                    *slot)
    {
        return m_signal_hide_preedit_string.connect (slot);
    }

    Connection signal_connect_update_preedit_string         (PanelClientSlotStringAttrs             *slot)
    {
        return m_signal_update_preedit_string.connect (slot);
    }

    Connection signal_connect_get_surrounding_text          (PanelClientSlotIntInt                  *slot)
    {
        return m_signal_get_surrounding_text.connect (slot);
    }

    Connection signal_connect_delete_surrounding_text       (PanelClientSlotIntInt                  *slot)
    {
        return m_signal_delete_surrounding_text.connect (slot);
    }

private:
    void launch_panel (const String &config, const String &display) const
    {
        scim_launch_panel (true, config, display, NULL);
    }
};

PanelClient::PanelClient ()
    : m_impl (new PanelClientImpl ())
{
}

PanelClient::~PanelClient ()
{
    delete m_impl;
}

int
PanelClient::open_connection (const String &config, const String &display)
{
    return m_impl->open_connection (config, display);
}

void
PanelClient::close_connection ()
{
    m_impl->close_connection ();
}

int
PanelClient::get_connection_number () const
{
    return m_impl->get_connection_number ();
}

bool
PanelClient::is_connected () const
{
    return m_impl->is_connected ();
}

bool
PanelClient::has_pending_event () const
{
    return m_impl->has_pending_event ();
}

bool
PanelClient::filter_event ()
{
    return m_impl->filter_event ();
}

bool
PanelClient::prepare (int icid)
{
    return m_impl->prepare (icid);
}

bool
PanelClient::send ()
{
    return m_impl->send ();
}

void
PanelClient::turn_on                (int icid)
{
    m_impl->turn_on (icid);
}
void
PanelClient::turn_off               (int icid)
{
    m_impl->turn_off (icid);
}
void
PanelClient::update_screen          (int icid, int screen)
{
    m_impl->update_screen (icid, screen);
}
void
PanelClient::show_help              (int icid, const String &help)
{
    m_impl->show_help (icid, help);
}
void
PanelClient::show_factory_menu      (int icid, const std::vector <PanelFactoryInfo> &menu)
{
    m_impl->show_factory_menu (icid, menu);
}
void
PanelClient::focus_in               (int icid, const String &uuid)
{
    m_impl->focus_in (icid, uuid);
}
void
PanelClient::focus_out              (int icid)
{
    m_impl->focus_out (icid);
}
void
PanelClient::update_factory_info    (int icid, const PanelFactoryInfo &info)
{
    m_impl->update_factory_info (icid, info);
}
void
PanelClient::update_spot_location   (int icid, int x, int y, int top_y)
{
    m_impl->update_spot_location (icid, x, y, top_y);
}
void
PanelClient::update_cursor_position (int icid, int cursor_pos)
{
    m_impl->update_cursor_position (icid, cursor_pos);
}
void
PanelClient::update_surrounding_text (int icid, const WideString &str, int cursor)
{
    m_impl->update_surrounding_text (icid, str, cursor);
}
void
PanelClient::show_preedit_string    (int icid)
{
    m_impl->show_preedit_string (icid);
}
void
PanelClient::show_aux_string        (int icid)
{
    m_impl->show_aux_string (icid);
}
void
PanelClient::show_lookup_table      (int icid)
{
    m_impl->show_lookup_table (icid);
}
void
PanelClient::hide_preedit_string    (int icid)
{
    m_impl->hide_preedit_string (icid);
}
void
PanelClient::hide_aux_string        (int icid)
{
    m_impl->hide_aux_string (icid);
}
void
PanelClient::hide_lookup_table      (int icid)
{
    m_impl->hide_lookup_table (icid);
}
void
PanelClient::update_preedit_string  (int icid, const WideString &str, const AttributeList &attrs)
{
    m_impl->update_preedit_string (icid, str, attrs);
}
void
PanelClient::update_preedit_caret   (int icid, int caret)
{
    m_impl->update_preedit_caret (icid, caret);
}
void
PanelClient::update_aux_string      (int icid, const WideString &str, const AttributeList &attrs)
{
    m_impl->update_aux_string (icid, str, attrs);
}
void
PanelClient::update_lookup_table    (int icid, const LookupTable &table)
{
    m_impl->update_lookup_table (icid, table);
}
void
PanelClient::register_properties    (int icid, const PropertyList &properties)
{
    m_impl->register_properties (icid, properties);
}
void
PanelClient::update_property        (int icid, const Property &property)
{
    m_impl->update_property (icid, property);
}
void
PanelClient::start_default_ise      (int icid)
{
    m_impl->start_default_ise (icid);
}
void
PanelClient::start_helper           (int icid, const String &helper_uuid)
{
    m_impl->start_helper (icid, helper_uuid);
}

void
PanelClient::stop_helper            (int icid, const String &helper_uuid)
{
    m_impl->stop_helper (icid, helper_uuid);
}
void
PanelClient::send_helper_event      (int icid, const String &helper_uuid, const Transaction &trans)
{
    m_impl->send_helper_event (icid, helper_uuid, trans);
}
void
PanelClient::register_input_context (int icid, const String &uuid)
{
    m_impl->register_input_context (icid, uuid);
}
void
PanelClient::remove_input_context   (int icid)
{
    m_impl->remove_input_context (icid);
}
void
PanelClient::reset_input_context    (int icid)
{
    m_impl->reset_input_context (icid);
}

void
PanelClient::turn_on_log            (int icid, uint32 isOn)
{
    m_impl->turn_on_log (icid, isOn);
}

void
PanelClient::reset_signal_handler                         (void)
{
    m_impl->reset_signal_handler ();
}

Connection
PanelClient::signal_connect_reload_config                 (PanelClientSlotVoid                    *slot)
{
    return m_impl->signal_connect_reload_config (slot);
}
Connection
PanelClient::signal_connect_exit                          (PanelClientSlotVoid                    *slot)
{
    return m_impl->signal_connect_exit (slot);
}
Connection
PanelClient::signal_connect_update_lookup_table_page_size (PanelClientSlotInt                     *slot)
{
    return m_impl->signal_connect_update_lookup_table_page_size (slot);
}
Connection
PanelClient::signal_connect_lookup_table_page_up          (PanelClientSlotVoid                    *slot)
{
    return m_impl->signal_connect_lookup_table_page_up (slot);
}
Connection
PanelClient::signal_connect_lookup_table_page_down        (PanelClientSlotVoid                    *slot)
{
    return m_impl->signal_connect_lookup_table_page_down (slot);
}
Connection
PanelClient::signal_connect_reset_option                  (PanelClientSlotVoid                    *slot)
{
    return m_impl->signal_connect_reset_option (slot);
}
Connection
PanelClient::signal_connect_trigger_property              (PanelClientSlotString                  *slot)
{
    return m_impl->signal_connect_trigger_property (slot);
}
Connection
PanelClient::signal_connect_process_helper_event          (PanelClientSlotStringStringTransaction *slot)
{
    return m_impl->signal_connect_process_helper_event (slot);
}
Connection
PanelClient::signal_connect_move_preedit_caret            (PanelClientSlotInt                     *slot)
{
    return m_impl->signal_connect_move_preedit_caret (slot);
}
Connection
PanelClient::signal_connect_update_preedit_caret          (PanelClientSlotInt                     *slot)
{
    return m_impl->signal_connect_update_preedit_caret (slot);
}
Connection
PanelClient::signal_connect_select_aux                    (PanelClientSlotInt                     *slot)
{
    return m_impl->signal_connect_select_aux (slot);
}
Connection
PanelClient::signal_connect_select_candidate              (PanelClientSlotInt                     *slot)
{
    return m_impl->signal_connect_select_candidate (slot);
}
Connection
PanelClient::signal_connect_process_key_event             (PanelClientSlotKeyEvent                *slot)
{
    return m_impl->signal_connect_process_key_event (slot);
}
Connection
PanelClient::signal_connect_commit_string                 (PanelClientSlotWideString              *slot)
{
    return m_impl->signal_connect_commit_string (slot);
}
Connection
PanelClient::signal_connect_forward_key_event             (PanelClientSlotKeyEvent                *slot)
{
    return m_impl->signal_connect_forward_key_event (slot);
}
Connection
PanelClient::signal_connect_request_help                  (PanelClientSlotVoid                    *slot)
{
    return m_impl->signal_connect_request_help (slot);
}
Connection
PanelClient::signal_connect_request_factory_menu          (PanelClientSlotVoid                    *slot)
{
    return m_impl->signal_connect_request_factory_menu (slot);
}
Connection
PanelClient::signal_connect_change_factory                (PanelClientSlotString                  *slot)
{
    return m_impl->signal_connect_change_factory (slot);
}

Connection
PanelClient::signal_connect_reset_keyboard_ise            (PanelClientSlotVoid                    *slot)
{
    return m_impl->signal_connect_reset_keyboard_ise (slot);
}

Connection
PanelClient::signal_connect_update_keyboard_ise           (PanelClientSlotVoid                    *slot)
{
    return m_impl->signal_connect_update_keyboard_ise (slot);
}

Connection
PanelClient::signal_connect_show_preedit_string           (PanelClientSlotVoid                    *slot)
{
    return m_impl->signal_connect_show_preedit_string (slot);
}

Connection
PanelClient::signal_connect_hide_preedit_string           (PanelClientSlotVoid                    *slot)
{
    return m_impl->signal_connect_hide_preedit_string (slot);
}

Connection
PanelClient::signal_connect_update_preedit_string         (PanelClientSlotStringAttrs             *slot)
{
    return m_impl->signal_connect_update_preedit_string (slot);
}

Connection
PanelClient::signal_connect_get_surrounding_text          (PanelClientSlotIntInt                  *slot)
{
    return m_impl->signal_connect_get_surrounding_text (slot);
}

Connection
PanelClient::signal_connect_delete_surrounding_text       (PanelClientSlotIntInt                  *slot)
{
    return m_impl->signal_connect_delete_surrounding_text (slot);
}

} /* namespace scim */

/*
vi:ts=4:nowrap:ai:expandtab
*/

