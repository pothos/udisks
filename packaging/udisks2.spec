%global glib2_version                   2.36
%global gobject_introspection_version   1.30.0
%global polkit_version                  0.102
%global systemd_version                 208
%global libatasmart_version             0.17
%global dbus_version                    1.4.0
%global with_gtk_doc                    1
%global libblockdev_version             2.10

%define is_fedora                       0%{?rhel} == 0
%define is_git                          %(git show > /dev/null 2>&1 && echo 1 || echo 0)
%define git_hash                        %(git log -1 --pretty=format:"%h" || true)
%define build_date                      %(date '+%Y%m%d')

Name:    udisks2
Summary: Disk Manager
Version: 2.7.2
%if %{is_git} == 0
Release: 1%{?dist}
%else
Release: 0.%{build_date}git%{git_hash}%{?dist}
%endif
License: GPLv2+
Group:   System Environment/Libraries
URL:     https://github.com/storaged-project/udisks
Source0: https://github.com/storaged-project/udisks/releases/download/udisks-%{version}/udisks-%{version}.tar.bz2

BuildRequires: glib2-devel >= %{glib2_version}
BuildRequires: gobject-introspection-devel >= %{gobject_introspection_version}
BuildRequires: libgudev1-devel >= %{systemd_version}
BuildRequires: libatasmart-devel >= %{libatasmart_version}
BuildRequires: polkit-devel >= %{polkit_version}
BuildRequires: systemd-devel >= %{systemd_version}
BuildRequires: gnome-common
BuildRequires: libacl-devel
BuildRequires: chrpath
BuildRequires: gtk-doc
BuildRequires: intltool
BuildRequires: redhat-rpm-config
BuildRequires: libblockdev-devel        >= %{libblockdev_version}
BuildRequires: libblockdev-part-devel   >= %{libblockdev_version}
BuildRequires: libblockdev-loop-devel   >= %{libblockdev_version}
BuildRequires: libblockdev-swap-devel   >= %{libblockdev_version}
BuildRequires: libblockdev-mdraid-devel >= %{libblockdev_version}
BuildRequires: libblockdev-fs-devel     >= %{libblockdev_version}
BuildRequires: libblockdev-crypto-devel >= %{libblockdev_version}

Requires: libblockdev        >= %{libblockdev_version}
Requires: libblockdev-part   >= %{libblockdev_version}
Requires: libblockdev-loop   >= %{libblockdev_version}
Requires: libblockdev-swap   >= %{libblockdev_version}
Requires: libblockdev-mdraid >= %{libblockdev_version}
Requires: libblockdev-fs     >= %{libblockdev_version}
Requires: libblockdev-crypto >= %{libblockdev_version}

# Needed for the systemd-related macros used in this file
%{?systemd_requires}
BuildRequires: systemd

# Needed to pull in the system bus daemon
Requires: dbus >= %{dbus_version}
# Needed to pull in the udev daemon
Requires: udev >= %{systemd_version}
# We need at least this version for bugfixes/features etc.
Requires: libatasmart >= %{libatasmart_version}
# For mount, umount, mkswap
Requires: util-linux
# For mkfs.ext3, mkfs.ext3, e2label
Requires: e2fsprogs
# For mkfs.xfs, xfs_admin
Requires: xfsprogs
# For mkfs.vfat
Requires: dosfstools
Requires: gdisk
# For ejecting removable disks
Requires: eject

Requires: lib%{name}%{?_isa} = %{version}-%{release}

# For mkntfs (not available on rhel or on ppc/ppc64)
%if ! 0%{?rhel}
%ifnarch ppc ppc64
Requires: ntfsprogs
%endif
%endif

# For /proc/self/mountinfo, only available in 2.6.26 or higher
Conflicts: kernel < 2.6.26

Provides:  storaged = %{version}-%{release}
Obsoletes: storaged

%description
The Udisks project provides a daemon, tools and libraries to access and
manipulate disks, storage devices and technologies.

%package -n lib%{name}
Summary: Dynamic library to access the udisksd daemon
Group: System Environment/Libraries
License: LGPLv2+
Provides:  libstoraged = %{version}-%{release}
Obsoletes: libstoraged

%description -n lib%{name}
This package contains the dynamic library, which provides
access to the udisksd daemon.

%package -n %{name}-iscsi
Summary: Module for iSCSI
Group: System Environment/Libraries
Requires: %{name}%{?_isa} = %{version}-%{release}
License: LGPLv2+
Requires: iscsi-initiator-utils
BuildRequires: iscsi-initiator-utils-devel
Provides:  storaged-iscsi = %{version}-%{release}
Obsoletes: storaged-iscsi

%description -n %{name}-iscsi
This package contains module for iSCSI configuration.

%package -n %{name}-lvm2
Summary: Module for LVM2
Group: System Environment/Libraries
Requires: %{name}%{?_isa} = %{version}-%{release}
License: LGPLv2+
Requires: lvm2
Requires: libblockdev-lvm >= %{libblockdev_version}
BuildRequires: lvm2-devel
BuildRequires: libblockdev-lvm-devel >= %{libblockdev_version}
Provides:  storaged-lvm2 = %{version}-%{release}
Obsoletes: storaged-lvm2

%description -n %{name}-lvm2
This package contains module for LVM2 configuration.

%package -n lib%{name}-devel
Summary: Development files for lib%{name}
Group: Development/Libraries
Requires: lib%{name}%{?_isa} = %{version}-%{release}
License: LGPLv2+
Provides:  libstoraged-devel = %{version}-%{release}
Obsoletes: libstoraged-devel

%description -n lib%{name}-devel
This package contains the development files for the library lib%{name}, a
dynamic library, which provides access to the udisksd daemon.

%if %{is_fedora}
%package -n %{name}-bcache
Summary: Module for Bcache
Group: System Environment/Libraries
Requires: %{name}%{?_isa} = %{version}-%{release}
License: LGPLv2+
Requires: libblockdev-kbd >= %{libblockdev_version}
BuildRequires: libblockdev-kbd-devel >= %{libblockdev_version}
Provides:  storaged-bcache = %{version}-%{release}
Obsoletes: storaged-bcache

%description -n %{name}-bcache
This package contains module for Bcache configuration.

%package -n %{name}-btrfs
Summary: Module for BTRFS
Group: System Environment/Libraries
Requires: %{name}%{?_isa} = %{version}-%{release}
License: LGPLv2+
Requires: libblockdev-btrfs >= %{libblockdev_version}
BuildRequires: libblockdev-btrfs-devel >= %{libblockdev_version}
Provides:  storaged-btrfs = %{version}-%{release}
Obsoletes: storaged-btrfs

%description -n %{name}-btrfs
This package contains module for BTRFS configuration.

%package -n %{name}-lsm
Summary: Module for LSM
Group: System Environment/Libraries
Requires: %{name}%{?_isa} = %{version}-%{release}
License: LGPLv2+
Requires: libstoragemgmt
BuildRequires: libstoragemgmt-devel
BuildRequires: libconfig-devel
Provides:  storaged-lsm = %{version}-%{release}
Obsoletes: storaged-lsm

%description -n %{name}-lsm
This package contains module for LSM configuration.

%package -n %{name}-zram
Summary: Module for ZRAM
Group: System Environment/Libraries
Requires: %{name}%{?_isa} = %{version}-%{release}
License: LGPLv2+
Requires: libblockdev-kbd >= %{libblockdev_version}
Requires: libblockdev-swap >= %{libblockdev_version}
BuildRequires: libblockdev-kbd-devel >= %{libblockdev_version}
BuildRequires: libblockdev-swap-devel
Provides:  storaged-zram = %{version}-%{release}
Obsoletes: storaged-zram

%description -n %{name}-zram
This package contains module for ZRAM configuration.
%endif

%prep
%setup -q -n udisks-%{version}

%build
autoreconf -ivf
%configure            \
    --sysconfdir=/etc \
%if %{with_gtk_doc}
    --enable-gtk-doc  \
%else
    --disable-gtk-doc \
%endif
%if %{is_fedora}
    --enable-modules
%else
    --enable-iscsi    \
    --enable-lvm2
%endif
make %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot}
%if %{with_gtk_doc} == 0
rm -fr %{buildroot}/%{_datadir}/gtk-doc/html/udisks2
%endif

find %{buildroot} -name \*.la -o -name \*.a | xargs rm

chrpath --delete %{buildroot}/%{_sbindir}/umount.udisks2
chrpath --delete %{buildroot}/%{_bindir}/udisksctl
chrpath --delete %{buildroot}/%{_libexecdir}/udisks2/udisksd

%find_lang udisks2

%post -n %{name}
%systemd_post udisks2.service
udevadm control --reload
udevadm trigger

%preun -n %{name}
%systemd_preun udisks2.service

%postun -n %{name}
%systemd_postun_with_restart udisks2.service

%post -n lib%{name} -p /sbin/ldconfig

%postun -n lib%{name} -p /sbin/ldconfig

%if %{is_fedora}
%post -n %{name}-zram
%systemd_post zram-setup@.service

%preun -n %{name}-zram
%systemd_preun zram-setup@.service

%postun -n %{name}-zram
%systemd_postun zram-setup@.service
%endif

%files -f udisks2.lang
%doc README.md AUTHORS NEWS HACKING
%license COPYING

%dir %{_sysconfdir}/udisks2
%if %{is_fedora}
%dir %{_sysconfdir}/udisks2/modules.conf.d
%endif
%{_sysconfdir}/udisks2/udisks2.conf

%{_sysconfdir}/dbus-1/system.d/org.freedesktop.UDisks2.conf
%{_datadir}/bash-completion/completions/udisksctl
%{_unitdir}/udisks2.service
%{_udevrulesdir}/80-udisks2.rules
%{_sbindir}/umount.udisks2


%dir %{_libdir}/udisks2
%dir %{_libdir}/udisks2/modules
%{_libexecdir}/udisks2/udisksd

%{_bindir}/udisksctl

%{_mandir}/man1/udisksctl.1*
%{_mandir}/man5/udisks2.conf.5*
%{_mandir}/man8/udisksd.8*
%{_mandir}/man8/udisks.8*
%{_mandir}/man8/umount.udisks2.8*

%{_datadir}/polkit-1/actions/org.freedesktop.UDisks2.policy
%{_datadir}/dbus-1/system-services/org.freedesktop.UDisks2.service

# Permissions for local state data are 0700 to avoid leaking information
# about e.g. mounts to unprivileged users
%attr(0700,root,root) %dir %{_localstatedir}/lib/udisks2

%files -n lib%{name}
%{_libdir}/libudisks2.so.*
%{_libdir}/girepository-1.0/UDisks-2.0.typelib

%files -n %{name}-lvm2
%{_libdir}/udisks2/modules/libudisks2_lvm2.so
%{_datadir}/polkit-1/actions/org.freedesktop.UDisks2.lvm2.policy

%files -n %{name}-iscsi
%{_libdir}/udisks2/modules/libudisks2_iscsi.so
%{_datadir}/polkit-1/actions/org.freedesktop.UDisks2.iscsi.policy

%files -n lib%{name}-devel
%{_libdir}/libudisks2.so
%dir %{_includedir}/udisks2
%dir %{_includedir}/udisks2/udisks
%{_includedir}/udisks2/udisks/*.h
%{_datadir}/gir-1.0/UDisks-2.0.gir
%if %{with_gtk_doc}
%dir %{_datadir}/gtk-doc/html/udisks2
%{_datadir}/gtk-doc/html/udisks2/*
%endif
%{_libdir}/pkgconfig/udisks2.pc

%if %{is_fedora}
%files -n %{name}-bcache
%{_libdir}/udisks2/modules/libudisks2_bcache.so
%{_datadir}/polkit-1/actions/org.freedesktop.UDisks2.bcache.policy

%files -n %{name}-btrfs
%{_libdir}/udisks2/modules/libudisks2_btrfs.so
%{_datadir}/polkit-1/actions/org.freedesktop.UDisks2.btrfs.policy

%files -n %{name}-lsm
%dir %{_sysconfdir}/udisks2/modules.conf.d
%{_libdir}/udisks2/modules/libudisks2_lsm.so
%{_mandir}/man5/udisks2_lsm.conf.*
%{_datadir}/polkit-1/actions/org.freedesktop.UDisks2.lsm.policy
%attr(0600,root,root) %{_sysconfdir}/udisks2/modules.conf.d/udisks2_lsm.conf

%files -n %{name}-zram
%dir %{_sysconfdir}/udisks2/modules.conf.d
%{_libdir}/udisks2/modules/libudisks2_zram.so
%{_datadir}/polkit-1/actions/org.freedesktop.UDisks2.zram.policy
%{_unitdir}/zram-setup@.service
%endif

%changelog
* Mon Jul 03 2017 Vojtech Trefny <vtrefny@redhat.com> - 2.7.1-1
- Version 2.7.1

* Fri Jun 02 2017 Vojtech Trefny <vtrefny@redhat.com> - 2.7.0-1
- Version 2.7.0

* Mon May 15 2017 Vojtech Trefny <vtrefny@redhat.com> - 2.6.5-1
- Version 2.6.5

* Tue Mar 14 2017 Vojtech Trefny <vtrefny@redhat.com> - 2.6.4-1
- Version 2.6.4

* Mon Nov 14 2016 Tomas Smetana <tsmetana@redhat.com> - 2.6.3-1
- Version 2.6.3

* Thu Jun 16 2016 Tomas Smetana <tsmetana@redhat.com> - 2.6.2-1
- Version 2.6.2; aimed to replace udisks2

* Wed Apr 27 2016 Peter Hatina <phatina@redhat.com> - 2.6.0-3
- Add support for libblockdev-part plugin which replaces
  parted calls

* Wed Mar 16 2016 Peter Hatina <phatina@redhat.com> - 2.6.0-2
- Fix permissions set for storaged_lsm.conf so it is readable only by root

* Mon Mar 14 2016 Peter Hatina <phatina@redhat.com> - 2.6.0-1
- Upgrade to 2.6.0

* Wed Feb 10 2016 Peter Hatina <phatina@redhat.com> - 2.5.0-3
- Package template zram-setup@.service file

* Wed Feb 10 2016 Peter Hatina <phatina@redhat.com> - 2.5.0-2
- Add udisksd configuration file and its man page

* Thu Jan 28 2016 Peter Hatina <phatina@redhat.com> - 2.5.0-1
- UDisks2 drop-in replacement

* Thu Jan 21 2016 Peter Hatina <phatina@redhat.com> - 2.4.0-3
- Redesign subpackage dependencies
- Make GTK documentation generation configurable

* Wed Jan 20 2016 Peter Hatina <phatina@redhat.com> - 2.4.0-2
- Reload udev rules and trigger events when installed

* Wed Jan 13 2016 Peter Hatina <phatina@redhat.com> - 2.4.0-1
- Upgrade to 2.4.0

* Wed Sep 30 2015 Peter Hatina <phatina@redhat.com> - 2.3.0-2
- Add Fedora/RHEL package configuration options

* Mon Sep 14 2015 Peter Hatina <phatina@redhat.com> - 2.3.0-1
- Change BuildRequires from pkgconfig macro to -devel packages
- Upgrade to 2.3.0

* Mon Aug 24 2015 Peter Hatina <phatina@redhat.com> - 2.2.0-1
- Upgrade to 2.2.0

* Fri Jul  3 2015 Peter Hatina <phatina@redhat.com> - 2.1.1-1
- Upgrade to 2.1.1

* Wed Jun 24 2015 Peter Hatina <phatina@redhat.com> - 2.1.0-4
- Add Requires for storaged modules

* Wed Jun 24 2015 Peter Hatina <phatina@redhat.com> - 2.1.0-3
- Changes for EPEL-7
  - Lower systemd required version to 208
  - Rewrite BuildRequires for systemd-devel

* Fri Jun 19 2015 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.1.0-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_23_Mass_Rebuild

* Thu Jun 11 2015 Peter Hatina <phatina@redhat.com> - 2.1.0-1
- Update to upstream 2.1.0

* Thu Apr 02 2015 Peter Hatina <phatina@redhat.com> - 2.0.0-1
- Rebase to the new Storaged implementation
- Upstream: https://storaged.org

* Tue Sep 16 2014 Stef Walter <stefw@redhat.com> - 0.3.1-1
- Update to upstream 0.3.1

* Mon Aug 18 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.3.0-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_22_Mass_Rebuild

* Sun Jun 08 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.3.0-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_Mass_Rebuild

* Tue Apr 08 2014 Patrick Uiterwijk <puiterwijk@redhat.com> - 0.3.0-1
- Update to upstream 0.3.0

* Fri Jan 31 2014 Patrick Uiterwijk <puiterwijk@redhat.com> - 0.2.0-1
- Update to upstream 0.2.0

* Thu Jan 16 2014 Patrick Uiterwijk <puiterwijk@redhat.com> - 0.1.0-2
- Removed double systemd BuildRequire
- Rewritten summary and description

* Sun Jan 12 2014 Patrick Uiterwijk <puiterwijk@redhat.com> - 0.1.0-1
- Rename from udisks2-lvm
