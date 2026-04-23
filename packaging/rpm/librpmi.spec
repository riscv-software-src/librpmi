Name:           librpmi
Version:        %{?version}%{!?version:1.0.0}
Release:        1%{?dist}
Summary:        RISC-V RPMI helper library

License:        BSD-2-Clause
URL:            https://github.com/riscv-software-src/librpmi
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  make

%description
librpmi provides core RPMI context, transport and service-group helpers for
RISC-V platform firmware and emulation integrations.

%package devel
Summary:        Development files for librpmi
Requires:       %{name}%{?_isa} = %{version}-%{release}

%description devel
Headers, static/shared linker artifacts and pkg-config metadata for building
software against librpmi.

%package source
Summary:        Source tree for librpmi
BuildArch:      noarch

%description source
Complete librpmi source tree installed under /usr/src/librpmi.

%prep
%autosetup

%build
make V=1 \
     CC=%{__cc} AR=%{__ar} AS=%{__as} LD=%{__ld} \
     LIBRPMI_VERSION=%{version} LIBRPMI_SOVERSION=%{?soversion}%{!?soversion:0}

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr
make install \
     I=%{buildroot}/usr \
     CC=%{__cc} AR=%{__ar} AS=%{__as} LD=%{__ld} \
     LIBRPMI_VERSION=%{version} LIBRPMI_SOVERSION=%{?soversion}%{!?soversion:0}

rm -f %{buildroot}/usr/COPYING.BSD
# Keep only one real runtime ELF to avoid duplicate build-id warnings.
rm -f %{buildroot}/usr/lib/librpmi.so.%{?soversion}%{!?soversion:0}
mv %{buildroot}/usr/lib/librpmi.so.%{version} %{buildroot}/usr/lib/librpmi.so.%{?soversion}%{!?soversion:0}
rm -f %{buildroot}/usr/lib/librpmi.so
ln -sf librpmi.so.%{?soversion}%{!?soversion:0} %{buildroot}/usr/lib/librpmi.so

mkdir -p %{buildroot}/usr/src/librpmi
cp -a Makefile README.md COPYING.BSD include lib test docs %{buildroot}/usr/src/librpmi/
if [ -f 0001-lib-Add-Logging-service-group.patch ]; then
  cp -a 0001-lib-Add-Logging-service-group.patch %{buildroot}/usr/src/librpmi/
fi

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%license COPYING.BSD
/usr/lib/librpmi.so.%{?soversion}%{!?soversion:0}

%files devel
/usr/include/librpmi.h
/usr/include/librpmi_env.h
/usr/include/librpmi_mm_efi.h
/usr/lib/librpmi.a
/usr/lib/librpmi.so
/usr/lib/pkgconfig/librpmi.pc

%files source
/usr/src/librpmi

%changelog
* Thu Apr 23 2026 Subrahmanya Lingappa <subrahmanya.lingappa@qualcomm.com> - %{version}-1
- Initial RPM packaging for librpmi (runtime, devel, source subpackages)
