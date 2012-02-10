%define _optdir	/opt
%define _ugdir	%{_optdir}/ug

Name:       isf
Summary:    Input Service Framework
Version:    2.2.4418
Release:    1
Group:      TO_BE/FILLED_IN
License:    TO_BE/FILLED_IN
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  edje-bin
BuildRequires:  embryo-bin
BuildRequires:  gettext-tools
BuildRequires:  vconf-keys-devel
BuildRequires:  pkgconfig(appcore-efl)
BuildRequires:  pkgconfig(elementary)
BuildRequires:  pkgconfig(utilX)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(ui-gadget)
BuildRequires:  pkgconfig(heynoti)
BuildRequires:  pkgconfig(ecore)
BuildRequires:  pkgconfig(edje)
BuildRequires:  pkgconfig(x11)
Requires(post): /sbin/ldconfig /usr/bin/vconftool
Requires(postun): /sbin/ldconfig


%description
Input Service Framewok (ISF) is an input method (IM) platform, and it has been derived from SCIM.


%package devel
Summary:    ISF header files
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description devel
This package contains ISF header files for ISE development.



%prep
%setup -q

%build

./bootstrap
%configure --disable-static \
		--disable-tray-icon --disable-filter-sctc
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

%post 
/sbin/ldconfig

/usr/bin/vconftool set -t string db/isf/input_lang "Automatic" -g 6514
/usr/bin/vconftool set -t string db/setting/autocapital_allow 0 -g 6514
/usr/bin/vconftool set -t string db/setting/autoperiod_allow 0 -g 6514

ln -sf /etc/init.d/isf-panel-efl /etc/rc.d/rc3.d/S42isf-panel-efl
ln -sf /etc/init.d/isf-panel-efl /etc/rc.d/rc4.d/S81isf-panel-efl


%postun -p /sbin/ldconfig



%files
%defattr(-,root,root,-)
%attr(755,root,root) %{_sysconfdir}/init.d/isf-panel-efl
%attr(755,root,root) %{_sysconfdir}/profile.d/isf.sh
%{_sysconfdir}/scim/global
%{_sysconfdir}/scim/config
%{_datadir}/scim/isf_candidate_theme2.edj
%{_datadir}/scim/isf_candidate_theme1.edj
%{_datadir}/scim/icons/*
%{_datadir}/applications/isf-panel-efl.desktop
%{_datadir}/locale/*
%{_bindir}/isf-demo-efl
%{_bindir}/scim
%{_bindir}/isf-log
%{_bindir}/isf-panel-efl
%{_bindir}/idm_core
%{_bindir}/isf-query-engines
%{_libdir}/libidm_context.so.*
%{_libdir}/libidm_capture.so
%{_libdir}/ecore/immodules/libisf-imf-module.so
%{_libdir}/scim-1.0/1.4.0/IMEngine/socket.so
%{_libdir}/scim-1.0/1.4.0/Config/simple.so
%{_libdir}/scim-1.0/1.4.0/Config/socket.so
%{_libdir}/scim-1.0/1.4.0/FrontEnd/socket.so
%{_libdir}/scim-1.0/scim-launcher
%{_libdir}/scim-1.0/scim-helper-launcher
%{_libdir}/libscim-1.0.so.*
%{_ugdir}/res/locale/*
%{_ugdir}/lib/libug-keyboard-setting-wizard-efl.so
%{_ugdir}/lib/libug-isfsetting-efl.so

%files devel
%defattr(-,root,root,-)
%{_includedir}/scim-1.0/*
%{_libdir}/libscim-1.0.so
%{_libdir}/pkgconfig/isf.pc
%{_libdir}/pkgconfig/scim.pc
%{_libdir}/libidm_context.so