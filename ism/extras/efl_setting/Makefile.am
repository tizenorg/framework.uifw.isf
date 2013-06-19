MAINTAINERCLEANFILES = Makefile.in
CLEANFILES           = *.bak

INCLUDES = -I$(top_builddir) \
           -I$(top_builddir)/ism/src \
           -I$(top_srcdir) \
           -I$(top_srcdir)/ism/src \
           -I$(top_srcdir)/ism/intl \
           -I$(top_srcdir)/ism/data \
           -I$(top_srcdir)/ism/utils \
           -I$(top_srcdir)/ism/extras/efl_setting/include \
           -I$(includedir) \
           -DSCIM_DATADIR=\"@SCIM_DATADIR@\" \
           -DSCIM_LOCALEDIR=\"@SCIM_LOCALEDIR@\" \
           -DSCIM_SYSCONFDIR=\"@SCIM_SYSCONFDIR@\" \
           -DSCIM_LIBEXECDIR=\"@SCIM_LIBEXECDIR@\" \
           -DSCIM_ICONDIR=\"@SCIM_ICONDIR@\" \
           -DSCIM_MODULE_PATH=\"@SCIM_MODULE_PATH@\" \
           -DSCIM_TEMPDIR=\"@SCIM_TEMPDIR@\"


noinst_HEADERS =

moduledir                       = /usr/ug/lib
module_LTLIBRARIES              = libug-isfsetting-efl.la
libug_isfsetting_efl_la_SOURCES = isf_setting_efl.cpp \
                                  scim_setup_module_efl.cpp \
                                  ../efl_panel/isf_panel_utility.cpp

libug_isfsetting_efl_la_CXXFLAGS = @EFL_CFLAGS@ \
                                   @VCONF_CFLAGS@ \
                                   @UIGADGET_CFLAGS@ \
                                   @PKGMGR_INFO_CFLAGS@ \
                                   @EFL_ASSIST_CFLAGS@

libug_isfsetting_efl_la_LDFLAGS  = -avoid-version \
                                   -export-dynamic \
                                   -rpath $(moduledir)

libug_isfsetting_efl_la_LIBADD  =  @LIBTOOL_EXPORT_OPTIONS@ \
                                   @LTLIBINTL@ \
                                   @EFL_LIBS@ \
                                   @VCONF_LIBS@ \
                                   @UIGADGET_LIBS@ \
                                   @PKGMGR_INFO_LIBS@ \
                                   @EFL_ASSIST_LIBS@ \
                                   $(top_builddir)/ism/src/libscim@SCIM_EPOCH@.la \
                                   $(top_builddir)/ism/extras/efl_immodule/libisf-imf-module.la

