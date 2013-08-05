MAINTAINERCLEANFILES = Makefile.in
CLEANFILES           = *.bak *.edj

INCLUDES = -I$(top_builddir) \
           -I$(top_builddir)/ism/src \
           -I$(top_srcdir) \
           -I$(top_srcdir)/ism/src \
           -I$(top_srcdir)/ism/intl \
           -I$(top_srcdir)/ism/data \
           -I$(top_srcdir)/ism/utils \
           -I$(includedir) \
           -DSCIM_DATADIR=\"@SCIM_DATADIR@\" \
           -DSCIM_LOCALEDIR=\"@SCIM_LOCALEDIR@\" \
           -DSCIM_SYSCONFDIR=\"@SCIM_SYSCONFDIR@\" \
           -DSCIM_LIBEXECDIR=\"@SCIM_LIBEXECDIR@\" \
           -DSCIM_ICONDIR=\"@SCIM_ICONDIR@\" \
           -DSCIM_MODULE_PATH=\"@SCIM_MODULE_PATH@\" \
           -DSCIM_TEMPDIR=\"@SCIM_TEMPDIR@\"

noinst_HEADERS = isf_panel_utility.h

if ISF_BUILD_PANEL_EFL
CONFIG_SCIM_PANEL_EFL  = isf-panel-efl
endif

bin_PROGRAMS           = $(CONFIG_SCIM_PANEL_EFL)

isf_panel_efl_SOURCES  = isf_panel_efl.cpp \
                         isf_panel_utility.cpp

isf_panel_efl_CXXFLAGS = @EFL_CFLAGS@ @VCONF_CFLAGS@ @PRIVILEGE_CONTROL_CFLAGS@ @DLOG_CFLAGS@ @TTS_CFLAGS@ @FEEDBACK_CFLAGS@

isf_panel_efl_LDFLAGS  = @EFL_LIBS@ @LTLIBINTL@ -rpath $(libdir) \
                         @VCONF_LIBS@ \
                         @X11_LIBS@ \
                         @PRIVILEGE_CONTROL_LIBS@ \
                         @DLOG_LIBS@ \
                         @TTS_LIBS@ \
                         @FEEDBACK_LIBS@

isf_panel_efl_LDADD    = $(top_builddir)/ism/src/libscim@SCIM_EPOCH@.la

