%define ver 0.4.1


Name: grandr_applet
Summary: GNOME Applet to control the X screen size
Version: %{ver}
Release: 1.f%{fedora}
License: GPL
Group: Multimedia
Packager: Kevin DeKorte <kdekorte@gmail.com>
Source0: http://dekorte.homeip.net/download/grandr_applet/grandr_applet-%{ver}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
BuildRequires: gnome-panel-devel gnome-desktop-devel gettext

%description
Grandr is a GNOME Applet to control the X screen size and orientation

%prep
%setup -q

%build
%configure
make %{?_smp_mflags}

%install
rm -rf %buildroot
make install DESTDIR=%buildroot

%clean
rm -rf $buildroot

%post
update-desktop-database

%files
%defattr(-,root,root,-)
%{_datadir}/locale
%{_docdir}/%{name}
%{_libdir}/bonobo/servers/GrandrApplet.server
%{_prefix}/libexec/grandr
%{_datadir}/pixmaps/grandr.png
