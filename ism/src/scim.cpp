/* ISF is based on SCIM 1.4.7 and extended for supporting more mobile fitable. */

/*
 * Smart Common Input Method
 *
 * Copyright (c) 2002-2005 James Su <suzhe@tsinghua.org.cn>
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
 * $Id: scim.cpp,v 1.51 2005/06/15 00:19:08 suzhe Exp $
 *
 */

#define Uses_SCIM_FRONTEND_MODULE
#define Uses_SCIM_IMENGINE_MODULE
#define Uses_SCIM_HELPER_MODULE
#define Uses_SCIM_BACKEND
#define Uses_SCIM_CONFIG_PATH
#define Uses_SCIM_TRANSACTION
#define Uses_C_LOCALE
#define Uses_SCIM_UTILITY
#include "scim_private.h"
#include <scim.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/times.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <privilege-control.h>
#include <sys/resource.h>
#include <sched.h>
#include <pkgmgr-info.h>
#include <Ecore_File.h>
#include <sys/smack.h>
#include <scim_panel_common.h>
#include "isf_query_utility.h"
#include "isf_pkg.h"
#include <dlog.h>

#ifdef LOG_TAG
# undef LOG_TAG
#endif
#define LOG_TAG             "SCIM"

#define WAIT_WM
#define ISF_SYSTEM_WM_READY_FILE    "/tmp/.wm_ready"
#define ISF_SYSTEM_WM_WAIT_COUNT    200
#define ISF_SYSTEM_WAIT_DELAY       100 * 1000
#define MIN_RETRY_TIME              8.0
#define IME_DB_PATH         "/opt/dbspace/.ime_info.db"
#define IME_DB_JOURNAL_PATH "/opt/dbspace/.ime_info.db-journal"
#define USER_ROOT   0
#define GROUP_APP   5000

using namespace scim;
using std::cout;
using std::cerr;
using std::endl;

static bool check_socket_frontend (void)
{
    SocketAddress address;
    SocketClient client;

    uint32 magic;

    address.set_address (scim_get_default_socket_frontend_address ());

    if (!client.connect (address))
        return false;

    if (!scim_socket_open_connection (magic,
                                      String ("ConnectionTester"),
                                      String ("SocketFrontEnd"),
                                      client,
                                      1000)) {
        return false;
    }

    return true;
}

static bool check_panel (const String &display)
{
    SocketAddress address;
    SocketClient client;
    uint32 magic;
    struct tms     tiks_buf;
    static bool check = true;   /* Added; 'start_tiks' can't be used to check if it's the first time because 'curr_tiks' could be also zero. */
    static clock_t start_tiks = (clock_t)0;
    static double  clock_tiks = (double)sysconf (_SC_CLK_TCK);
    clock_t curr_tiks = times (&tiks_buf);  /* This can be a negative number or zero at boot up */
    double  secs = (double)(curr_tiks - start_tiks) / clock_tiks;

    if (secs > MIN_RETRY_TIME ||
            (start_tiks == (clock_t)0 && check == true) /* Make sure it goes through at first time */
            ) {
        address.set_address (scim_get_default_panel_socket_address (display));

        if (!client.connect (address)) {
            start_tiks = curr_tiks;
            check = false;
            return false;
        }

        if (!scim_socket_open_connection (magic,
            String ("ConnectionTester"),
            String ("Panel"),
            client,
            1000)) {
                start_tiks = curr_tiks;
                check = false;
                return false;
        }
    }

    if (check == true)
        start_tiks = curr_tiks;
    check = false;
    return true;
}

/**
 * @brief Check the specific file.
 *
 * @param strFile The file path.
 *
 * @return true if the file is existed, otherwise false.
 */
static bool check_file (const char* strFile)
{
    struct stat st;

    /* Workaround so that "/" returns a true, otherwise we can't monitor "/" in ecore_file_monitor */
    if (stat (strFile, &st) < 0 && strcmp (strFile, "/"))
        return false;
    else
        return true;
}

static bool check_lfile (const char* strFile)
{
    struct stat st;

    if (lstat (strFile, &st) < 0)
        return false;
    else
        return true;
}

/**
 * @brief : Checks whether the window manager is launched or not
 * @return true if window manager launched, else false
 */
static bool check_wm_ready (void)
{
    SCIM_DEBUG_MAIN (3) << __FUNCTION__ << "...\n";

#ifdef WAIT_WM
    int try_count = 0;
    while (check_file (ISF_SYSTEM_WM_READY_FILE) == false) {
        if (ISF_SYSTEM_WM_WAIT_COUNT <= (try_count++)) return false;
        usleep (ISF_SYSTEM_WAIT_DELAY);
    }
#endif

    return true;
}


/* The broker for launching 3rd party IMEs */
// {

#include <Ecore.h>
#include <Ecore_Ipc.h>

#ifndef SCIM_HELPER_LAUNCHER_PROGRAM
#define SCIM_HELPER_LAUNCHER_PROGRAM  (SCIM_LIBEXECDIR "/scim-helper-launcher")
#endif

static Ecore_Ipc_Server *server = NULL;
static Ecore_Event_Handler *exit_handler = NULL;
static Ecore_Event_Handler *data_handler = NULL;

static void launch_helper (const char *exe, const char *name, const char *appid)
{
    if (exe && name && appid) {
        int pid = fork ();

        if (pid < 0) return;

        if (pid == 0) {
            if (exit_handler) {
                ecore_event_handler_del (exit_handler);
                exit_handler = NULL;
            }

            if (data_handler) {
                ecore_event_handler_del (data_handler);
                data_handler = NULL;
            }

            if (server) {
                ecore_ipc_server_del (server);
                server = NULL;
            }

            ecore_ipc_shutdown ();

            const char *argv [] = { exe,
                "--daemon",
                "--config", "socket",
                "--display", ":0",
                name,
                appid,
                0};

            ISF_SAVE_LOG ("Exec scim_helper_launcher(%s %s)\n", name, appid);

            setsid ();
            execv (SCIM_HELPER_LAUNCHER_PROGRAM, const_cast<char **>(argv));
            exit (-1);
        }
    }
    else {
        ISF_SAVE_LOG ("Invalid parameter\n");
    }
}

static Eina_Bool sig_exit_cb (void *data, int ev_type, void *ev)
{
    ecore_main_loop_quit ();
    return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool handler_client_data (void *data, int ev_type, void *ev)
{
    Ecore_Ipc_Event_Client_Data *e = (Ecore_Ipc_Event_Client_Data *)ev;
    if (!e) return ECORE_CALLBACK_PASS_ON;

    LOGD ("client %p sent [%i] [%i] [%i]", e->client, e->major, e->minor, e->size);

    char buffer[_POSIX_PATH_MAX + 1] = {0};
    strncpy (buffer, (char*)(e->data), (e->size > _POSIX_PATH_MAX) ? _POSIX_PATH_MAX : e->size);

#if defined(HAVE_SYSTEMD)
    if (e->major == 2 && e->minor == 4) {
        if (strcmp (buffer, "request_to_terminate_scim") == 0) {
            ISF_SAVE_LOG ("Panel requested to terminate scim.\n");

            /* This will terminate all scim-launcher, isf-panel-eflm and scim-helper-laucnher processes.
               And scim process will be started again by scim.service systemd.*/
            ecore_main_loop_quit ();
            return ECORE_CALLBACK_DONE;
        }
        else if (strcmp (buffer, "request_to_launch_panel") == 0) {
            LOGD ("App requested to launch panel.");
            try {
                if (!check_panel ("")) {
                    cerr << "Launching Panel...\n";
                    LOGD ("ppid:%d Launching panel process....", getppid ());

                    scim_launch_panel (true, "socket", "", NULL);
                }
            } catch (scim::Exception & e) {
                cerr << e.what () << "\n";
                ISF_SAVE_LOG ("Fail to launch panel. error: %s\n", e.what ());
            }
            return ECORE_CALLBACK_PASS_ON;
        }
    }
#endif

    const char *message = "Done";
    if (ecore_ipc_client_send (e->client, 0, 0, 0, 0, 0, message, strlen (message)) == 0) {
        LOGW ("ecore_ipc_client_send FAILED!!");
    }
    ecore_ipc_client_flush (e->client);

    int blank_index = 0;
    for (int loop = 0; loop < (int)strlen (buffer); loop++) {
        if (buffer[loop] == ' ') {
            blank_index = loop;
        }
    }
    buffer[blank_index] = '\0';

    char ise_name[_POSIX_PATH_MAX + 1] = {0};
    char ise_appid[_POSIX_PATH_MAX + 1] = {0};
    strncpy (ise_name, buffer, _POSIX_PATH_MAX);
    strncpy (ise_appid, (char*)(buffer) + blank_index + 1, _POSIX_PATH_MAX);

    ImeInfoDB imeInfo;
    if (isf_db_select_ime_info_by_appid (ise_appid, &imeInfo)) {
        /* Only for Native IME, execute applicaiton's exec */
        if (imeInfo.pkgtype.compare("tpk") == 0) {
            if (!check_lfile (imeInfo.exec.c_str ())) {
                // Make bin directory and set smack label
                String path = imeInfo.exec.substr (0, imeInfo.exec.find_last_of (SCIM_PATH_DELIM)).c_str ();
                if (path.length () > 0) {
                    ecore_file_mkpath (path.c_str ());  // If path exists, this function returns EINA_TRUE.
                    if (smack_setlabel (path.c_str (), imeInfo.pkgid.c_str (), SMACK_LABEL_ACCESS) < 0) {
                        ISF_SAVE_LOG ("Fail to set smack label (%s) for %s\n", imeInfo.pkgid.c_str (), path.c_str ());
                    }
                }

                // Make symbolic link file
                if (EINA_FALSE == ecore_file_symlink (SCIM_HELPER_LAUNCHER_PROGRAM, imeInfo.exec.c_str ())) {
                    ISF_SAVE_LOG ("Fail to make symlink file: %s\n", imeInfo.exec.c_str ());
                }
                // Set smack label
                if (smack_lsetlabel (imeInfo.exec.c_str (), imeInfo.pkgid.c_str (), SMACK_LABEL_ACCESS) < 0) {
                    ISF_SAVE_LOG ("Fail to set smack label (%s) for %s\n", imeInfo.pkgid.c_str (), imeInfo.exec.c_str ());
                }
            }
            launch_helper (imeInfo.exec.c_str (), ise_name, ise_appid);
            return ECORE_CALLBACK_PASS_ON;
        }
    }

    launch_helper (SCIM_HELPER_LAUNCHER_PROGRAM, ise_name, ise_appid);

    return ECORE_CALLBACK_PASS_ON;
}

static void run_broker (int argc, char *argv [])
{
    if (!ecore_init ()) {
        LOGE ("Failed to init ecore!!");
        return;
    }

    if (!ecore_ipc_init ()) {
        LOGE ("Failed to init ecore_ipc!!");
        ecore_shutdown ();
        return;
    }

    exit_handler = ecore_event_handler_add (ECORE_EVENT_SIGNAL_EXIT, sig_exit_cb, NULL);
    data_handler = ecore_event_handler_add (ECORE_IPC_EVENT_CLIENT_DATA, handler_client_data, NULL);

    server = ecore_ipc_server_add (ECORE_IPC_LOCAL_SYSTEM, "scim-helper-broker", 0, NULL);

    if (server == NULL) {
        LOGW ("ecore_ipc_server_add returned NULL!!");
    }

    ecore_main_loop_begin ();

    if (exit_handler) {
        ecore_event_handler_del (exit_handler);
        exit_handler = NULL;
    }

    if (data_handler) {
        ecore_event_handler_del (data_handler);
        data_handler = NULL;
    }

    if (server) {
        ecore_ipc_server_del (server);
        server = NULL;
    }

    ecore_ipc_shutdown ();
    ecore_shutdown ();
}
// }

int main (int argc, char *argv [])
{
    struct tms tiks_buf;
    clock_t clock_start = times (&tiks_buf);

    BackEndPointer       backend;

    std::vector<String>  frontend_list;
    std::vector<String>  config_list;
    std::vector<String>  engine_list;
    std::vector<String>  helper_list;
    std::vector<String>  exclude_engine_list;
    std::vector<String>  load_engine_list;
    std::vector<String>  all_engine_list;

    std::vector<String>::iterator it;

    String def_frontend ("socket");
    String def_config ("simple");

    int  i;
    bool daemon = false;
    bool socket = true;
    bool manual = false;
    bool db_exist = false;

    int   new_argc = 0;
    char *new_argv [80];

    LOGD ("ppid : %d Waiting for wm_ready", getppid ());

    if (!check_wm_ready ()) {
        std::cerr << "[ISF-PANEL-EFL] WM ready timeout\n";
        ISF_SAVE_LOG ("ppid:%d Window Manager ready timeout\n", getppid ());
    }

    /* Display version info */
    cout << "Input Service Manager " << ISF_VERSION << "\n\n";

    /* Get modules list */
    scim_get_frontend_module_list (frontend_list);
    config_list.push_back ("simple");
    config_list.push_back ("socket");

    db_exist = check_file (IME_DB_PATH);

    scim_get_helper_module_list (helper_list);

    /* If IME_DB_PATH didn't exist, it would be made by scim_get_helper_module_list(). Need to change owner/group and mode. */
    if (db_exist == false) {
        char buf[512] = {0};
        int j = 0, ret;
        char *files[3];
        files[0] = (char *)IME_DB_PATH;
        files[1] = (char *)IME_DB_JOURNAL_PATH;
        files[2] = NULL;

        while (files[j]) {
            ret = chown (files[j], USER_ROOT, GROUP_APP);
            if (ret < 0) {
                strerror_r (errno, buf, sizeof(buf));
                LOGE ("chown(%s %d %d) failed. %s", files[j], USER_ROOT, GROUP_APP, buf);
            }

            ret = chmod (files[j], S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
            if (ret < 0) {
                strerror_r (errno, buf, sizeof(buf));
                LOGE ("chmod(%s 0660) failed. %s", files[j], buf);
            }
            j++;
        }
    }

    if (helper_list.size () < 1) {  // If there is no IME, that is, if ime_info DB is empty, try to reload it.
        isf_pkg_reload_ime_info_db();
        scim_get_helper_module_list (helper_list);  // Assuming ime_info DB is initialized, try again.
    }

    scim_get_imengine_module_list (engine_list);

    for (it = engine_list.begin (); it != engine_list.end (); it++) {
        all_engine_list.push_back (*it);
        if (*it != "socket")
            load_engine_list.push_back (*it);
    }
    for (it = helper_list.begin (); it != helper_list.end (); it++) {
        all_engine_list.push_back (*it);
        load_engine_list.push_back (*it);
    }

    /* Unless the target is no-keyboard model, we have a problem. */
    if (load_engine_list.size () < 1) {
        LOGW ("There is no helper/imeengine!");
    }

    /* Use x11 FrontEnd as default if available. */
    if (frontend_list.size ()) {
        def_frontend = String ("x11");
        if (std::find (frontend_list.begin (),
                       frontend_list.end (),
                       def_frontend) == frontend_list.end ())
            def_frontend = frontend_list [0];
    }

    /* Use simple Config module as default if available. */
    def_config = scim_global_config_read (SCIM_GLOBAL_CONFIG_DEFAULT_CONFIG_MODULE, String ("simple"));
    if (std::find (config_list.begin (),
                   config_list.end (),
                   def_config) == config_list.end ())
        def_config = config_list [0];

    /* If no Socket Config/IMEngine/FrontEnd modules */
    /* then do not try to start a SocketFrontEnd. */
    if (std::find (frontend_list.begin (), frontend_list.end (), "socket") == frontend_list.end () ||
        std::find (config_list.begin (), config_list.end (), "socket") == config_list.end () ||
        std::find (engine_list.begin (), engine_list.end (), "socket") == engine_list.end ())
        socket = false;

    /* parse command options */
    i = 0;
    while (i < argc) {
        if (++i >= argc) break;

        if (String ("-l") == argv [i] ||
            String ("--list") == argv [i]) {

            cout << endl;
            cout << "Available FrontEnd module:\n";
            for (it = frontend_list.begin (); it != frontend_list.end (); it++)
                cout << "    " << *it << endl;

            cout << endl;
            cout << "Available Config module:\n";
            for (it = config_list.begin (); it != config_list.end (); it++) {
                if (*it != "dummy")
                    cout << "    " << *it << endl;
            }

            cout << endl;
            cout << "Available ISEngine module:\n";
            for (it = all_engine_list.begin (); it != all_engine_list.end (); it++)
                cout << "    " << *it << endl;

            return 0;
        }

        if (String ("-f") == argv [i] ||
            String ("--frontend") == argv [i]) {
            if (++i >= argc) {
                cerr << "No argument for option " << argv [i-1] << endl;
                return -1;
            }
            def_frontend = argv [i];
            continue;
        }

        if (String ("-c") == argv [i] ||
            String ("--config") == argv [i]) {
            if (++i >= argc) {
                cerr << "No argument for option " << argv [i-1] << endl;
                return -1;
            }
            def_config = argv [i];
            continue;
        }

        if (String ("-h") == argv [i] ||
            String ("--help") == argv [i]) {
            cout << "Usage: " << argv [0] << " [option]...\n\n"
                 << "The options are: \n"
                 << "  -l, --list              List all of available modules.\n"
                 << "  -f, --frontend name     Use specified FrontEnd module.\n"
                 << "  -c, --config name       Use specified Config module.\n"
                 << "  -e, --engines name      Load specified set of ISEngines.\n"
                 << "  -ne,--no-engines name   Do not load those set of ISEngines.\n"
                 << "  -d, --daemon            Run as a daemon.\n"
                 << "  --no-socket             Do not try to start a SocketFrontEnd daemon.\n"
                 << "  -h, --help              Show this help message.\n";
            return 0;
        }

        if (String ("-d") == argv [i] ||
            String ("--daemon") == argv [i]) {
            daemon = true;
            continue;
        }

        if (String ("-e") == argv [i] || String ("-s") == argv [i] ||
            String ("--engines") == argv [i] || String ("--servers") == argv [i]) {
            if (++i >= argc) {
                cerr << "No argument for option " << argv [i-1] << endl;
                return -1;
            }
            load_engine_list.clear ();
            scim_split_string_list (load_engine_list, String (argv [i]), ',');
            manual = true;
            continue;
        }

        if (String ("-ne") == argv [i] || String ("-ns") == argv [i] ||
            String ("--no-engines") == argv [i] || String ("-no-servers") == argv [i]) {
            if (++i >= argc) {
                cerr << "No argument for option " << argv [i-1] << endl;
                return -1;
            }
            scim_split_string_list (exclude_engine_list, String (argv [i]), ',');
            manual = true;
            continue;
        }

        if (String ("--no-socket") == argv [i]) {
            socket = false;
            continue;
        }

        if (String ("--") == argv [i])
            break;

        cerr << "Invalid command line option: " << argv [i] << "\n";
        return -1;
    } /* End of command line parsing. */

    /* Store the rest argvs into new_argv. */
    for (++i; i < argc; ++i) {
        new_argv [new_argc ++] = argv [i];
    }

    new_argv [new_argc] = 0;

    ISF_SAVE_LOG ("ppid:%d Starting SCIM......\n", getppid ());

    /* Get the imengine module list which should be loaded. */
    if (exclude_engine_list.size ()) {
        load_engine_list.clear ();
        for (i = 0; i < (int)all_engine_list.size (); ++i) {
            if (std::find (exclude_engine_list.begin (),
                           exclude_engine_list.end (),
                           all_engine_list [i]) == exclude_engine_list.end () &&
                all_engine_list [i] != "socket")
                load_engine_list.push_back (all_engine_list [i]);
        }
    }

    if (!def_frontend.length ()) {
        cerr << "No FrontEnd module is available!\n";
        return -1;
    }

    if (!def_config.length ()) {
        cerr << "No Config module is available!\n";
        return -1;
    }

    /* If you try to use the socket feature manually,
       then let you do it by yourself. */
    if (def_frontend == "socket" || def_config == "socket" ||
        std::find (load_engine_list.begin (), load_engine_list.end (), "socket") != load_engine_list.end ())
        socket = false;

    /* If the socket address of SocketFrontEnd and SocketIMEngine/SocketConfig are different,
       then do not try to start the SocketFrontEnd instance automatically. */
    if (scim_get_default_socket_frontend_address () != scim_get_default_socket_imengine_address () ||
        scim_get_default_socket_frontend_address () != scim_get_default_socket_config_address ())
        socket = false;

    /* Try to start a SocketFrontEnd daemon first. */
    if (socket) {
        LOGD ("ppid:%d Now socket frontend....", getppid ());

        /* If no Socket FrontEnd is running, then launch one.
           And set manual to false. */
        try {
            if (!check_socket_frontend ()) {
                cerr << "Launching a daemon with Socket FrontEnd...\n";
                char *l_argv [] = { const_cast<char *> ("--stay"), 0 };
                scim_launch (true,
                            def_config,
                            (load_engine_list.size () ? scim_combine_string_list (load_engine_list, ',') : "none"),
                            "socket",
                            l_argv);
                manual = false;
            }
        } catch (scim::Exception &e) {
            cerr << e.what () << "\n";
            ISF_SAVE_LOG ("catch: %s\n", e.what ());
        }

        /* If there is one Socket FrontEnd running and it's not manual mode,
           then just use this Socket Frontend. */
        try {
            if (!manual) {
                for (int j = 0; j < 100; ++j) {
                    if (check_socket_frontend ()) {
                        def_config = "socket";
                        load_engine_list.clear ();
                        load_engine_list.push_back ("socket");
                        break;
                    }
                    scim_usleep (100000);
                }
            }
        } catch (scim::Exception &e) {
            cerr << e.what () << "\n";
            ISF_SAVE_LOG ("catch: %s\n", e.what ());
        }
    }

    cerr << "Launching a process with " << def_frontend << " FrontEnd...\n";

    LOGD ("ppid:%d Now default frontend....", getppid ());

    /* Launch the scim process. */
    if (scim_launch (daemon,
                     def_config,
                     (load_engine_list.size () ? scim_combine_string_list (load_engine_list, ',') : "none"),
                     def_frontend,
                     new_argv) == 0) {
        if (daemon)
            cerr << "ISM has been successfully launched.\n";
        else
            cerr << "ISM has exited successfully.\n";

        gettime (clock_start, "ISM launch time");

        LOGD ("ppid:%d Now checking panel process....", getppid ());

        /* When finished launching scim-launcher, let's create panel process also, for the default display :0 */
        try {
            if (!check_panel ("")) {
               cerr << "Launching Panel...\n";
               LOGD ("ppid:%d Launching panel process....%s", getppid (), def_config.c_str ());

               scim_launch_panel (true, "socket", "", NULL);
            }
        } catch (scim::Exception & e) {
            cerr << e.what () << "\n";
            ISF_SAVE_LOG ("Fail to launch panel. error: %s\n", e.what ());
        }

        run_broker (argc, argv);

        return 0;
    }

    if (daemon)
        cerr << "Failed to launch ISM.\n";
    else
        cerr << "ISM has exited abnormally.\n";

    run_broker (argc, argv);

    return 1;
}

/*
vi:ts=4:ai:nowrap:expandtab
*/
