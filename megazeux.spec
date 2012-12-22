Name:		megazeux
Version:	2.84c
Release:	1%{?dist}

Summary:	A simple game creation system (GCS)

Group:		Amusements/Games
License:	GPLv2+
URL:		http://digitalmzx.net/
Source:		megazeux-2.84c.tar.bz2
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root

BuildRequires:	SDL-devel
BuildRequires:	libvorbis-devel
BuildRequires:	libpng-devel
BuildRequires:	zlib-devel

%description

MegaZeux is a Game Creation System (GCS), inspired by Epic MegaGames' ZZT,
first released by Gregory Janson in 1994. It was recently ported from 16bit
MSDOS C/ASM to portable SDL C/C++, improving its platform support and
hardware compatibility.

Many features have been added since the original DOS version, and with the
help of an intuitive editor and a simple but powerful object-oriented
programming language, MegaZeux allows you to create your own ANSI-esque games
regardless of genre. It is actively maintained by a thriving community.

See http://digitalmzx.net/ for more information.

%prep
%setup -q

%build
./config.sh --platform unix --as-needed-hack \
            --enable-release
%{__make}

%install
rm -rf "$RPM_BUILD_ROOT"
make install DESTDIR=${RPM_BUILD_ROOT}

%clean
rm -rf "$RPM_BUILD_ROOT"

%files
%defattr(-,root,root,-)
%doc docs/changelog.txt docs/port.txt docs/macro.txt
%{_bindir}/
%{_prefix}/games
%{_libdir}/megazeux
%{_datadir}/megazeux
%{_datadir}/icons/megazeux.png
%{_sysconfdir}/megazeux-config

%changelog
* Sat Dec 22 2012 Alistair John Strachan <alistair@devzero.co.uk> 2.84c-1
- new upstream version

* Sat Jun 02 2012 Alistair John Strachan <alistair@devzero.co.uk> 2.84-1
- new upstream version

* Sat Nov 26 2009 Alistair John Strachan <alistair@devzero.co.uk> 2.83-1
- new upstream version

* Thu Nov 27 2008 Alistair John Strachan <alistair@devzero.co.uk> 2.82b-1
- initial RPM release
