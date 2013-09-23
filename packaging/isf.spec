%define _usrdir /usr
%define _ugdir  %{_usrdir}/ug

Name:       isf
Summary:    Input Service Framework
Version:    2.4.6923
Release:    1
Group:      System Environment/Libraries
License:    LGPL
Source0:    %{name}-%{version}.tar.gz
Source1:    scim.service
BuildRequires:  edje-bin
BuildRequires:  embryo-bin
BuildRequires:  gettext-tools
BuildRequires:  pkgconfig(appcore-efl)
BuildRequires:  pkgconfig(libprivilege-control)
BuildRequires:  pkgconfig(elementary)
BuildRequires:  pkgconfig(utilX)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(ui-gadget-1)
BuildRequires:  pkgconfig(pkgmgr-info)
BuildRequires:  pkgconfig(ecore)
BuildRequires:  pkgconfig(edje)
BuildRequires:  pkgconfig(x11)
BuildRequires:  pkgconfig(notification)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(tts)
BuildRequires:  pkgconfig(feedback)
BuildRequires:  efl-assist-devel
Requires(post): /sbin/ldconfig /usr/bin/vconftool e17 net-config libmm-sound sys-assert
Requires(postun): /sbin/ldconfig

%description
Input Service Framewok (ISF) is an input method (IM) platform, and it has been derived from SCIM.


%package devel
Summary:    ISF header files
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description devel
This package contains ISF header files for ISE development.

%package -n ug-isfsetting-efl
Summary:    ISF setting ug
Group:      Application
Requires:   %{name} = %{version}-%{release}

%description -n ug-isfsetting-efl
ISF setting UI Gadget

%prep
%setup -q

%build
export GC_SECTIONS_FLAGS="-fdata-sections -ffunction-sections -Wl,--gc-sections"
CFLAGS+=" -fvisibility=hidden ${GC_SECTIONS_FLAGS} "; export CFLAGS
CXXFLAGS+=" -fvisibility=hidden -fvisibility-inlines-hidden ${GC_SECTIONS_FLAGS} ";export CXXFLAGS

%autogen
%configure --disable-static \
		--disable-tray-icon --disable-filter-sctc
make %{?_smp_mflags}

%install
rm -rf %{buildroot}

%make_install
mkdir -p %{buildroot}%{_datadir}/license
install -m0644 %{_builddir}/%{buildsubdir}/COPYING %{buildroot}%{_datadir}/license/%{name}

install -d %{buildroot}%{_libdir}/systemd/user/core-efl.target.wants
install -m0644 %{SOURCE1} %{buildroot}%{_libdir}/systemd/user/
ln -sf ../scim.service %{buildroot}%{_libdir}/systemd/user/core-efl.target.wants/scim.service

%post
/sbin/ldconfig
mkdir -p /etc/scim/conf
mkdir -p /opt/apps/scim/lib/scim-1.0/1.4.0/Helper
mkdir -p /opt/apps/scim/lib/scim-1.0/1.4.0/SetupUI
mkdir -p /opt/apps/scim/lib/scim-1.0/1.4.0/IMEngine


/usr/bin/vconftool set -t bool file/private/isf/autocapital_allow 1 -g 6514 || :
/usr/bin/vconftool set -t bool file/private/isf/autoperiod_allow 0 -g 6514 || :
/usr/bin/vconftool set -t string db/isf/input_language "en_US" -g 5000 || :
/usr/bin/vconftool set -t int memory/isf/input_panel_state 0 -i -g 5000 || :

%postun -p /sbin/ldconfig


%files
%manifest %{name}.manifest
/etc/smack/accesses.d/%{name}.rule
%defattr(-,root,root,-)
%{_libdir}/systemd/user/core-efl.target.wants/scim.service
%{_libdir}/systemd/user/scim.service
%attr(755,root,root) %{_sysconfdir}/profile.d/isf.sh
%{_sysconfdir}/scim/global
%{_sysconfdir}/scim/config
%{_datadir}/scim/isf_candidate_theme1.edj
%{_datadir}/scim/icons/*
%{_datadir}/locale/*
%{_bindir}/isf-demo-efl
%{_bindir}/scim
%{_bindir}/isf-log
%{_bindir}/isf-panel-efl
%{_bindir}/isf-query-engines
%{_libdir}/*/immodules/*.so
%{_libdir}/scim-1.0/1.4.0/IMEngine/socket.so
%{_libdir}/scim-1.0/1.4.0/Config/simple.so
%{_libdir}/scim-1.0/1.4.0/Config/socket.so
%{_libdir}/scim-1.0/1.4.0/FrontEnd/*.so
%{_libdir}/scim-1.0/scim-launcher
%{_libdir}/scim-1.0/scim-helper-launcher
%{_libdir}/libscim-*.so*
%{_datadir}/license/%{name}

%files devel
%defattr(-,root,root,-)
%{_includedir}/scim-1.0/*
%{_libdir}/libscim-*.so
%{_libdir}/pkgconfig/isf.pc
%{_libdir}/pkgconfig/scim.pc

%post -n ug-isfsetting-efl
mkdir -p /opt/ug/bin/
ln -sf /usr/bin/ug-client /opt/ug/bin/isfsetting-efl

%files -n ug-isfsetting-efl
%manifest ug-isfsetting-efl.manifest
/etc/smack/accesses.d/ug.isfsetting-efl.include
/usr/share/packages/ug-isfsetting-efl.xml
%{_ugdir}/lib/libug-isfsetting-efl.so
%{_ugdir}/res/locale/*/LC_MESSAGES/isfsetting-efl.*
%{_datadir}/scim/isfsetting.edj
