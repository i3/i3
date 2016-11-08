# ===========================================================================
#      http://www.gnu.org/software/autoconf-archive/ax_sanitizers.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_SANITIZERS([SANITIZERS], [ENABLED-BY-DEFAULT], [ACTION-SUCCESS])
#
# DESCRIPTION
#
#   Offers users to enable one or more sanitizers (see
#   https://github.com/google/sanitizers) with the corresponding
#   --enable-<sanitizer>-sanitizer option.
#
#   SANITIZERS is a whitespace-separated list of sanitizers to offer via
#   --enable-<sanitizer>-sanitizer options, e.g. "address memory" for the
#   address sanitizer and the memory sanitizer. If SANITIZERS is not specified,
#   all known sanitizers to AX_SANITIZERS will be offered, which at the time of
#   writing are "address memory undefined".
#   NOTE that SANITIZERS is expanded at autoconf time, not at configure time,
#   i.e. you cannot use shell variables in SANITIZERS.
#
#   ENABLED-BY-DEFAULT is a whitespace-separated list of sanitizers which
#   should be enabled by default, e.g. "memory undefined". Note that not all
#   sanitizers can be combined, e.g. memory sanitizer cannot be enabled when
#   address sanitizer is already enabled.
#   Set ENABLED-BY-DEFAULT to a single whitespace in order to disable all
#   sanitizers by default.
#   ENABLED-BY-DEFAULT is expanded at configure time, so you can use shell
#   variables.
#
#   ACTION-SUCCESS allows to specify shell commands to execute on success, i.e.
#   when one of the sanitizers was successfully enabled. This is a good place
#   to call AC_DEFINE for any precompiler constants you might need to make your
#   code play nice with sanitizers.
#
#   The variable ax_enabled_sanitizers contains a whitespace-separated list of
#   all enabled sanitizers, so that you can print them at the end of configure,
#   if you wish.
#
#   The additional --enable-sanitizers option allows users to enable/disable
#   all sanitizers, effectively overriding ENABLED-BY-DEFAULT.
#
# EXAMPLES
#
#   AX_SANITIZERS([address])
#     dnl offer users to enable address sanitizer via --enable-address-sanitizer
#
#   is_debug_build=…
#   if test "x$is_debug_build" = "xyes"; then
#     default_sanitizers="address memory"
#   else
#     default_sanitizers=
#   fi
#   AX_SANITIZERS([address memory], [$default_sanitizers])
#     dnl enable address sanitizer and memory sanitizer by default for debug
#     dnl builds, e.g. when building from git instead of a dist tarball.
#
#   AX_SANITIZERS(, , [
#     AC_DEFINE([SANITIZERS_ENABLED],
#               [],
#               [At least one sanitizer was enabled])])
#     dnl enable all sanitizers known to AX_SANITIZERS by default and set the
#     dnl SANITIZERS_ENABLED precompiler constant.
#
#   AX_SANITIZERS(, [ ])
#     dnl provide all sanitizers, but enable none by default.
#
# LICENSE
#
#   Copyright (c) 2016 Michael Stapelberg <michael@i3wm.org>
#
#   Copying and distribution of this file, with or without modification,
#   are permitted in any medium without royalty provided the copyright
#   notice and this notice are preserved.  This file is offered as-is,
#   without any warranty.

AC_DEFUN([AX_SANITIZERS],
[AX_REQUIRE_DEFINED([AX_CHECK_COMPILE_FLAG])
AX_REQUIRE_DEFINED([AX_CHECK_LINK_FLAG])
AX_REQUIRE_DEFINED([AX_APPEND_FLAG])
AC_ARG_ENABLE(sanitizers,
  AS_HELP_STRING(
    [--enable-sanitizers],
    [enable all known sanitizers]),
  [ax_sanitizers_default=$enableval],
  [ax_sanitizers_default=])
ax_enabled_sanitizers=
m4_foreach_w([mysan], m4_default($1, [address memory undefined]), [
  dnl If ax_sanitizers_default is unset, i.e. the user neither explicitly
  dnl enabled nor explicitly disabled all sanitizers, we get the default value
  dnl for this sanitizer based on whether it is listed in ENABLED-BY-DEFAULT.
  AS_IF([test "x$ax_sanitizers_default" = "x"], [dnl
          ax_sanitizer_default=
          for mycheck in m4_default([$2], [address memory undefined]); do
            AS_IF([test "x$mycheck" = "x[]mysan"], [ax_sanitizer_default=yes])
          done
          AS_IF([test "x$ax_sanitizer_default" = "x"], [ax_sanitizer_default=no])
        ],
        [ax_sanitizer_default=$ax_sanitizers_default])
  AC_ARG_ENABLE(mysan[]-sanitizer,
    AS_HELP_STRING(
      [--enable-[]mysan[]-sanitizer],
      [enable -fsanitize=mysan]),
    [ax_sanitizer_enabled=$enableval],
    [ax_sanitizer_enabled=$ax_sanitizer_default])

AS_IF([test "x$ax_sanitizer_enabled" = "xyes"], [
dnl Not using AX_APPEND_COMPILE_FLAGS and AX_APPEND_LINK_FLAGS because they
dnl lack the ability to specify ACTION-SUCCESS.
  AX_CHECK_COMPILE_FLAG([-fsanitize=[]mysan], [
    AX_CHECK_LINK_FLAG([-fsanitize=[]mysan], [
      AX_APPEND_FLAG([-fsanitize=[]mysan], [])
dnl If and only if libtool is being used, LDFLAGS needs to contain -Wc,-fsanitize=….
dnl See e.g. https://sources.debian.net/src/systemd/231-7/configure.ac/?hl=128#L135
dnl TODO: how can recognize that situation and add -Wc,?
      AX_APPEND_FLAG([-fsanitize=[]mysan], [LDFLAGS])
dnl TODO: add -fPIE -pie for memory
      # -fno-omit-frame-pointer results in nicer stack traces in error
      # messages, see http://clang.llvm.org/docs/AddressSanitizer.html#usage
      AX_CHECK_COMPILE_FLAG([-fno-omit-frame-pointer], [
        AX_APPEND_FLAG([-fno-omit-frame-pointer], [])])
dnl TODO: at least for clang, we should specify exactly -O1, not -O2 or -O0, so that performance is reasonable but stacktraces are not tampered with (due to inlining), see http://clang.llvm.org/docs/AddressSanitizer.html#usage
      m4_default([$3], :)
      ax_enabled_sanitizers="[]mysan $ax_enabled_sanitizers"
    ])
  ])
])
])dnl
])dnl AX_SANITIZERS
