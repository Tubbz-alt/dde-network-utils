%global repo dde-network-utils

%if 0%{?fedora}
Name:           deepin-network-utils
%else
Name:           %{repo}
%endif
Version:        5.3.0.5
Release:        1%{?dist}
Summary:        Deepin desktop-environment - network utils
License:        GPLv3
URL:            https://github.com/linuxdeepin/dde-network-utils
%if 0%{?fedora}
Source0:        %{url}/archive/%{version}/%{repo}-%{version}.tar.gz
%else
Source0:        %{name}_%{version}.orig.tar.xz
%endif

BuildRequires:  gcc-c++
BuildRequires:  pkgconfig(dframeworkdbus)
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  qt5-linguist
BuildRequires:  pkgconfig(gsettings-qt)

%description
Deepin desktop-environment - network utils.

%package devel
Summary:        Development package for %{name}
Requires:       %{name}%{?_isa} = %{version}-%{release}

%description devel
Header files and libraries for %{name}.

%prep
%autosetup -n %{repo}-%{version}
sed -i 's|/lib$|/%{_lib}|' dde-network-utils.pro

%build
# help find (and prefer) qt5 utilities, e.g. qmake, lrelease
export PATH=%{_qt5_bindir}:$PATH
%qmake_qt5 PREFIX=%{_prefix}
%make_build

%install
%make_install INSTALL_ROOT=%{buildroot}

%files
%doc README.md
%{_libdir}/lib*.so.1
%{_libdir}/lib*.so.1.*
%{_datadir}/%{repo}/

%files devel
%{_includedir}/libddenetworkutils/
%{_libdir}/pkgconfig/*.pc
%{_libdir}/lib*.so

%changelog
* Thu Jan 31 2019 mosquito <sensor.wen@gmail.com> - 0.0.9-1
- Update to 0.0.9

* Thu Jan 31 2019 Fedora Release Engineering <releng@fedoraproject.org> - 0.0.8.1-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_30_Mass_Rebuild

* Wed Dec 12 2018 mosquito <sensor.wen@gmail.com> - 0.0.8.1-1
- Update to 0.0.8.1

* Fri Nov  9 2018 mosquito <sensor.wen@gmail.com> - 0.0.7-1
- Update to 0.0.7

* Thu Aug  2 2018 mosquito <sensor.wen@gmail.com> - 0.0.4-1
- Update to 0.0.4

* Mon Jul 23 2018 mosquito <sensor.wen@gmail.com> - 0.0.3-1
- Initial package build

