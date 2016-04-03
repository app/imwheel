#
# Define some variables here
#
%define version 0.9.9pre6
%define prefix /usr/X11
%define cfg /etc/X11
%define ins /usr/bin/install 
%define bz2 /usr/bin/bzip2

#
# imwheel.spec
# Rob Ludwick
# 06/04/00
#
Summary: A utility to make wheel mice work under X
Name: imwheel
Version: %{version}
Release: 1
Copyright: GPL
Group: User Interface/X Hardware Support
Source: http://www.jonatkins.org/imwheel/files/imwheel-%{version}.tar.gz
URL: http://www.jonatkins.org/imwheel
Packager: Rob Ludwick <rludwick@users.sourceforge.net>
BuildRoot: /tmp/imwheel
%description
This is the imwheel utility for X.  It supports a variety of 
wheel mice including the MS Intellimouse.

%prep
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT
%setup

%build
./configure --bindir=%{prefix}/bin --disable-gpm --sysconfdir=%{cfg}
make all

%install
mkdir -p $RPM_BUILD_ROOT%{prefix}/bin $RPM_BUILD_ROOT%{prefix}/man/man1 $RPM_BUILD_ROOT%{prefix}/etc
/usr/bin/install -c -d -o root -g root -m 0755 $RPM_BUILD_ROOT%{prefix}/bin
/usr/bin/install -c -o root -g root -m 0755 imwheel $RPM_BUILD_ROOT%{prefix}/bin/imwheel
/usr/bin/install -c -d -o root -g root -m 0755 $RPM_BUILD_ROOT%{prefix}/man/man1
/usr/bin/install -c -o root -g root -m 0644 imwheel.1 $RPM_BUILD_ROOT%{prefix}/man/man1/imwheel.1x
%{bz2} -9 $RPM_BUILD_ROOT%{prefix}/man/man1/imwheel.1x
/usr/bin/install -c -d -o root -g root -m 0755 $RPM_BUILD_ROOT/%{cfg}
/usr/bin/install -c -o root -g root -m 0644 imwheelrc $RPM_BUILD_ROOT/%{cfg}/imwheelrc

	
%files
%config %{cfg}/imwheelrc
%attr(0755, root, root) %{prefix}/bin/imwheel
%{prefix}/man/man1/imwheel.1x.bz2

%doc BUGS ChangeLog M-BA47 NEWS COPYING EMACS README TODO AUTHORS INSTALL

%clean

%post

%postun

%Changelog

*  Sun Jun 04 2000 <rludwick@users.sourceforge.net>
--  Initial Version (based off the Mandrake 6 spec file by Peter Putzer)
