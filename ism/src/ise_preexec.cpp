/*
 * ISF(Input Service Framework)
 *
 * ISF is based on SCIM 1.4.7 and extended for supporting more mobile fitable.
 * Copyright (c) 2000 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
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

#include "scim.h"

#include <dlfcn.h>
#include <list>
#include <fcntl.h>
#include <string.h>
#include <sys/xattr.h>
#include <sys/types.h>
#include <sys/prctl.h>

#include <ail.h>
#include <privilege-control.h>

#define Uses_SCIM_HELPER
#include "scim.h"

#define MAX_LOCAL_BUFSZ 128
#define LABEL_LEN       23
#define AUL_PR_NAME     16

#define PREEXEC_FILE "/usr/share/aul/preexec_list.txt"

#define DEFAULT_PACKAGE_TYPE "rpm"
#define DEFAULT_PACKAGE_NAME "libisf-bin"
#define DEFAULT_APPLICATION_PATH "/usr/lib/scim-1.0/scim-helper-launcher"

#define _E(fmt, arg...) printf("[%s,%d] "fmt, __FUNCTION__, __LINE__, ##arg)
#define _D(fmt, arg...) printf("[%s,%d] "fmt, __FUNCTION__, __LINE__, ##arg)

namespace scim
{

static int preexec_initialized = 0;

typedef struct _preexec_list_t {
    char *pkg_type;
    char *so_path;
    int (*dl_do_pre_exe) (char *, char *);
} preexec_list_t;

static std::list<preexec_list_t*> preexec_list;

static void __preexec_list_free()
{
    preexec_list_t *type_t;

    for(std::list<preexec_list_t*>::iterator iter = preexec_list.begin();
        iter != preexec_list.end();std::advance(iter, 1)) {
        type_t = *iter;
        if (type_t) {
            if (type_t->pkg_type)
                free(type_t->pkg_type);
            if (type_t->so_path)
                free(type_t->so_path);
            free(type_t);
        }
    }
    preexec_list.clear();

    return;
}

static inline void __preexec_init()
{
    void *handle = NULL;
    FILE *preexec_file;
    char *saveptr = NULL;
    char line[MAX_LOCAL_BUFSZ];
    char *type = NULL;
    char *sopath = NULL;
    char *symbol = NULL;
    int (*func) (char *, char *) = NULL;
    preexec_list_t *type_t = NULL;

    preexec_file = fopen(PREEXEC_FILE, "rt");
    if (preexec_file == NULL) {
        _E("no preexec\n");
        return;
    }

    _D("preexec start\n");

    while (fgets(line, MAX_LOCAL_BUFSZ, preexec_file) > 0) {
        /* Parse each line */
        if (line[0] == '#' || line[0] == '\0')
            continue;

        type = strtok_r(line, ":\f\n\r\t\v ", &saveptr);
        if (type == NULL)
            continue;
        sopath = strtok_r(NULL, ",\f\n\r\t\v ", &saveptr);
        if (sopath == NULL)
            continue;
        symbol = strtok_r(NULL, ",\f\n\r\t\v ", &saveptr);
        if (symbol == NULL)
            continue;

        type_t = (preexec_list_t *) calloc(1, sizeof(preexec_list_t));
        if (type_t == NULL) {
            _E("no available memory\n");
            __preexec_list_free();
            return;
        }

        handle = dlopen(sopath, RTLD_NOW);
        if (handle == NULL) {
            free(type_t);
            continue;
        }
        _D("preexec %s %s# - handle : %x\n", type, sopath, handle);

        func = (int (*)(char*, char*))dlsym(handle, symbol);
        if (func == NULL) {
            _E("failed to get symbol type:%s path:%s\n",
               type, sopath);
            free(type_t);
            dlclose(handle);
            continue;
        }

        type_t->pkg_type = strdup(type);
        if (type_t->pkg_type == NULL) {
            _E("no available memory\n");
            free(type_t);
            __preexec_list_free();
            return;
        }
        type_t->so_path = strdup(sopath);
        if (type_t->so_path == NULL) {
            _E("no available memory\n");
            free(type_t->pkg_type);
            free(type_t);
            __preexec_list_free();
            return;
        }
        type_t->dl_do_pre_exe = func;

        preexec_list.insert(preexec_list.end(), type_t);
    }

    fclose(preexec_file);
    preexec_initialized = 1;
}

static inline void __preexec_run(const char *pkg_type, const char *pkg_name,
                                 const char *app_path)
{
    preexec_list_t *type_t;

    if (!preexec_initialized)
        return;

    for(std::list<preexec_list_t*>::iterator iter = preexec_list.begin();
        iter != preexec_list.end();std::advance(iter, 1)) {
        type_t = *iter;
        if (type_t) {
            if (!strcmp(pkg_type, type_t->pkg_type)) {
                if (type_t->dl_do_pre_exe != NULL) {
                    type_t->dl_do_pre_exe((char *)pkg_name,
                        (char *)app_path);
                    _D("called dl_do_pre_exe() type: %s, %s %s\n", pkg_type, pkg_name, app_path);
                } else {
                    _E("no symbol for this type: %s",
                        pkg_type);
                }
            }
        }
    }

}

static void __set_oom()
{
    char buf[MAX_LOCAL_BUFSZ];
    FILE *fp;

    /* we should reset oomadj value as default because child
    inherits from parent oom_adj*/
    snprintf(buf, MAX_LOCAL_BUFSZ, "/proc/%d/oom_adj", getpid());
    fp = fopen(buf, "w");
    if (fp == NULL)
        return;
    fprintf(fp, "%d", -16);
    fclose(fp);
}

static inline int __set_dac()
{
    return control_privilege();
}

static inline int __set_smack(char* path)
{
    /*
     * This is additional option.
     * Though such a application fails in this function, that error is ignored.
     */
    char label[LABEL_LEN + 1] = {0, };
    int fd = 0;
    int result = -1;

    result = getxattr(path, "security.SMACK64EXEC", label, LABEL_LEN);
    if(result < 0)  // fail to get extended attribute
        return 0;   // ignore error

    fd = open("/proc/self/attr/current", O_RDWR);
    if(fd < 0)      // fail to open file
        return 0;   // ignore error

    result = write(fd, label, strlen(label));
    if(result < 0) {    // fail to write label
        close(fd);
        return 0;   // ignore error
    }

    close(fd);
    return 0;
}

typedef struct {
    std::string package_type;
    std::string package_name;
    std::string app_path;
} PKGINFO;

static void get_pkginfo(const char *helper, PKGINFO *info)
{
    if (helper && info ) {
        ail_appinfo_h handle;
        ail_error_e r;
        char *value;
        r = ail_package_get_appinfo(helper, &handle);
        if (r != AIL_ERROR_OK) {
            /* Let's try with appid once more */
            r = ail_get_appinfo(helper, &handle);
            if (r != AIL_ERROR_OK) {
                return;
            }
        }

        info->package_name = helper;

        r = ail_appinfo_get_str(handle, AIL_PROP_X_SLP_PACKAGETYPE_STR, &value);
        if (r != AIL_ERROR_OK) {
            printf("ail_appinfo_get_str() failed : %s %s\n", helper, AIL_PROP_X_SLP_PACKAGETYPE_STR);
        } else {
            info->package_type = value;
        }

        r = ail_appinfo_get_str(handle, AIL_PROP_EXEC_STR, &value);
        if (r != AIL_ERROR_OK) {
            printf("ail_appinfo_get_str() failed : %s %s\n", helper, AIL_PROP_EXEC_STR);
        } else {
            info->app_path = value;
        }

        ail_package_destroy_appinfo(handle);
    }
}

int ise_preexec(const char *helper)
{
    const char *file_name;
    char process_name[AUL_PR_NAME];

    PKGINFO info;
    info.package_name = DEFAULT_PACKAGE_NAME;
    info.package_type = DEFAULT_PACKAGE_TYPE;
    info.app_path = DEFAULT_APPLICATION_PATH;

    get_pkginfo(helper, &info);

    /* Set new session ID & new process group ID*/
    /* In linux, child can set new session ID without check permission */
    /* TODO : should be add to check permission in the kernel*/
    setsid();

    __preexec_init();
    __preexec_run(info.package_type.c_str(), info.package_name.c_str(), info.app_path.c_str());
    __preexec_list_free();

    /* SET OOM*/
    __set_oom();

    /* SET SMACK LABEL */
    __set_smack((char*)(info.app_path.c_str()));

    /* SET DAC*/
    if (__set_dac() < 0) {
        _D("fail to set DAC - check your package's credential\n");
        return -1;
    }
    /* SET DUMPABLE - for coredump*/
    prctl(PR_SET_DUMPABLE, 1);

    /* SET PROCESS NAME*/
    if (info.app_path.empty()) {
        _D("app_path should not be NULL - check menu db");
        return -1;
    }
    file_name = strrchr(info.app_path.c_str(), '/') + 1;
    if (file_name == NULL) {
        _D("can't locate file name to execute");
        return -1;
    }
    memset(process_name, '\0', AUL_PR_NAME);
    snprintf(process_name, AUL_PR_NAME, "%s", file_name);
    prctl(PR_SET_NAME, process_name);

    return 0;
}

};

/*
vi:ts=4:nowrap:ai:expandtab
*/
