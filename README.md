![Logo](docs/logo-30.png) i3: A tiling window manager
=====================================================

[![Build Status](https://travis-ci.org/i3/i3.svg?branch=next)](https://travis-ci.org/i3/i3)
[![Issue Stats](http://www.issuestats.com/github/i3/i3/badge/issue?style=flat)](http://www.issuestats.com/github/i3/i3)
[![Pull Request Stats](http://www.issuestats.com/github/i3/i3/badge/pr?style=flat)](http://www.issuestats.com/github/i3/i3)

## Description

i3 is a tiling window manager for X11.

For more information about i3, please see [the project's website](https://i3wm.org/) and [online documentation](https://i3wm.org/docs/).

For information about contributing to i3, please see [CONTRIBUTING.md](.github/CONTRIBUTING.md).

## Development

On debian-based systems, the following line will install all requirements:
```bash
apt install autoconf libev-dev libstartup-notification0-dev libxcb-xkb-dev libxcb-xinerama0-dev libxcb-randr0-dev libxcb-util0-dev libxcb-cursor-dev libxcb-keysyms1-dev libxcb-icccm4-dev libxcb-xrm-dev libxkbcommon-dev libxkbcommon-x11-dev libpango1.0-dev libyajl-dev
```

## Compilation

Compiling is done with the following commands:
```bash
autoreconf -fi
mkdir -p build && cd build
../configure
make -j8
```

