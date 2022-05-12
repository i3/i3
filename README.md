## PKGBUILD for Arch testing
1. Create MAKEPKG file in an empty folder 
2. `makepkg -si`
```bash
# Maintainer: Jan-Erik Rediger <badboy at archlinux dot us>
# Contributor: Thorsten Toepper <atsutane at freethoughts dot de>
# Contributor: William Giokas <1007380@gmail.com>

# This PKGBUILD was prepared for pacman 4.1 by William. Thank you. :-)

pkgname=i3-cmprmsd
pkgver=4.14.r1502.g7c20b2c7
pkgrel=1
pkgdesc='An improved dynamic tiling window manager'
arch=('i686' 'x86_64')
url='http://i3wm.org/'
license=('BSD')
provides=('i3-wm')
conflicts=('i3-wm' 'i3-gaps' 'i3-gaps-next-git')
groups=('i3' 'i3-vcs')
depends=('xcb-util-keysyms' 'xcb-util-wm' 'libev' 'yajl'
         'startup-notification' 'pango' 'perl' 'xcb-util-cursor' 'xcb-util-xrm'
         'libxkbcommon-x11')
makedepends=('git' 'bison' 'flex' 'asciidoc' 'xmlto' 'meson')
optdepends=('i3lock: For locking your screen.'
            'i3status: To display system information with a bar.')
options=('docs')
source=('git+https://github.com/cmprmsd/i3#branch=1412-bspwm-resize')
sha1sums=('SKIP')

pkgver() {
  cd "$srcdir/i3"
  git describe --long | sed 's/\([^-]*-g\)/r\1/;s/-/./g'
}


build() {
  cd "i3"
  arch-meson \
    -Ddocs=true \
    -Dmans=true \
  ../build
  meson compile -C ../build
}

package() {
  cd "i3"
  DESTDIR="${pkgdir}" meson install -C ../build

  install -Dt "${pkgdir}/usr/share/licenses/${pkgname}" -m644 LICENSE
}

# vim:set ts=2 sw=2 et:
```


![Logo](docs/logo-30.png) i3: A tiling window manager
=====================================================

[![Build Status](https://github.com/i3/i3/actions/workflows/main.yml/badge.svg)](https://github.com/i3/i3/actions/workflows/main.yml)
[![Issue Stats](https://img.shields.io/github/issues/i3/i3.svg)](https://github.com/i3/i3/issues)
[![Pull Request Stats](https://img.shields.io/github/issues-pr/i3/i3.svg)](https://github.com/i3/i3/pulls)

[![Packages](https://repology.org/badge/latest-versions/i3.svg)](https://repology.org/metapackage/i3/versions)
[![Packages](https://repology.org/badge/tiny-repos/i3.svg)](https://repology.org/metapackage/i3/versions)

i3 is a tiling window manager for X11.

For more information about i3, please see [the project's website](https://i3wm.org/) and [online documentation](https://i3wm.org/docs/).

For information about contributing to i3, please see [CONTRIBUTING.md](.github/CONTRIBUTING.md).
