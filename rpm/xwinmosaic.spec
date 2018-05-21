Name:           xwinmosaic
Version:        0.4.2.1
License:        BSD-3-Clause
Release:        2%{?dist}
Summary:        X11 window switcher that draws windows list as colour mosaic
Url:            https://github.com/soulthreads/xwinmosaic
Group:          Applications/Productivity
Source:         https://github.com/ZaWertun/xwinmosaic/archive/%{version}.tar.gz
BuildRequires:  xz
BuildRequires:  sed
BuildRequires:  cmake
BuildRequires:  gtk2-devel
BuildRequires:  xorg-x11-server-devel

%description
Inspired by XMonad.Actions.GridSelect, but written in C + GTK+2, uses
nice-looking colours and has some set of helpful features.

%prep
%setup -q

%build
%__sed -i 's|set (CMAKE_C_FLAGS "-std=c99 -Wall")|set (CMAKE_C_FLAGS "-std=c99 -Wall \${CMAKE_C_FLAGS}")|' CMakeLists.txt
%cmake .
%make_build

%install
%make_install

%files
%defattr(-,root,root)
%doc %{_mandir}/man1/xwinmosaic.1.gz
%doc LICENSE README.md
%{_bindir}/xwinmosaic

%changelog
* Mon May 21 2018 Yaroslav Sidlovsky <zawertun@gmail.com> - 0.4.2.1-2
- version 0.4.2.1

* Sun Mar 01 2015 Yaroslav Sidlovsky <zawertun@gmail.com> - 0.4.1.1-1
- initial version

