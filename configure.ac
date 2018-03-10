#                                               -*- Autoconf -*-
# Run autoreconf -fi to generate a configure script from this file.

AC_PREREQ([2.69])
AC_INIT([i3], [4.15], [https://github.com/i3/i3/issues])
# For AX_EXTEND_SRCDIR
AX_ENABLE_BUILDDIR
AM_INIT_AUTOMAKE([foreign subdir-objects -Wall no-dist-gzip dist-bzip2])
# Default to silent rules, use V=1 to get verbose compilation output.
AM_SILENT_RULES([yes])
# Make it possible to disable maintainer mode to disable re-generation of build
# system files.
AM_MAINTAINER_MODE([enable])
AC_CONFIG_SRCDIR([libi3/ipc_recv_message.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

dnl Verify macros defined in m4/ such as AX_SANITIZERS are not present in the
dnl output, i.e. are replaced as expected. This line results in a better error
dnl message when using aclocal < 1.13 (which does not understand
dnl AC_CONFIG_MACRO_DIR) without passing the -I m4 parameter.
m4_pattern_forbid([AX_SANITIZERS])

# Verify we are using GNU make because we use '%'-style pattern rules in
# Makefile.am, which are a GNU make extension. Pull requests to replace
# '%'-style pattern rules with a more portable alternative are welcome.
AX_CHECK_GNU_MAKE
AS_VAR_IF([_cv_gnu_make_command], [""], [AC_MSG_ERROR([the i3 Makefile.am requires GNU make])])

AX_EXTEND_SRCDIR

AS_IF([test -d ${srcdir}/.git],
      [
        VERSION="$(git -C ${srcdir} describe --tags --abbrev=0)"
        I3_VERSION="$(git -C ${srcdir} describe --tags --always) ($(git -C ${srcdir} log --pretty=format:%cd --date=short -n1), branch \\\"$(git -C ${srcdir} describe --tags --always --all | sed s:heads/::)\\\")"
        # Mirrors what libi3/is_debug_build.c does:
        is_release=$(test $(echo "${I3_VERSION}" | cut -d '(' -f 1 | wc -m) -lt 10 && echo yes || echo no)
      ],
      [
        VERSION="$(cut -d '-' -f 1 ${srcdir}/I3_VERSION | cut -d ' ' -f 1)"
        I3_VERSION="$(sed -e 's/@<:@\"?\\@:>@/\\&/g' ${srcdir}/I3_VERSION)"
        is_release="$(grep -q non-git ${srcdir}/I3_VERSION && echo no || echo yes)"
      ])
AC_SUBST([I3_VERSION], [$I3_VERSION])
MAJOR_VERSION="$(echo ${VERSION} | cut -d '.' -f 1)"
MINOR_VERSION="$(echo ${VERSION} | cut -d '.' -f 2)"
PATCH_VERSION="$(echo ${VERSION} | cut -d '.' -f 3)"
AS_IF([test "x${PATCH_VERSION}" = x], [PATCH_VERSION=0])
AC_DEFINE_UNQUOTED([I3_VERSION], ["${I3_VERSION}"], [i3 version])
AC_DEFINE_UNQUOTED([MAJOR_VERSION], [${MAJOR_VERSION}], [i3 major version])
AC_DEFINE_UNQUOTED([MINOR_VERSION], [${MINOR_VERSION}], [i3 minor version])
AC_DEFINE_UNQUOTED([PATCH_VERSION], [${PATCH_VERSION}], [i3 patch version])

AX_CODE_COVERAGE

dnl is_release must be lowercase because AX_CHECK_ENABLE_DEBUG calls m4_tolower
dnl on its fourth argument.
AX_CHECK_ENABLE_DEBUG([yes], , [UNUSED_NDEBUG], [$is_release])

AC_PROG_CC_C99

# For strnlen() and vasprintf().
AC_USE_SYSTEM_EXTENSIONS

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
dnl The error message should include the specific type which could not be
dnl found, but I do not see a way to achieve that.
AC_CHECK_TYPES([mode_t, off_t, pid_t, size_t, ssize_t], , [AC_MSG_FAILURE([cannot find required type])])

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_STRNLEN
AC_CHECK_FUNCS([atexit dup2 ftruncate getcwd gettimeofday localtime_r memchr memset mkdir rmdir setlocale socket strcasecmp strchr strdup strerror strncasecmp strndup strrchr strspn strstr strtol strtoul], , [AC_MSG_FAILURE([cannot find the $ac_func function, which i3 requires])])

# Checks for libraries.

AC_SEARCH_LIBS([floor], [m], , [AC_MSG_FAILURE([cannot find the required floor() function despite trying to link with -lm])])

# libev does not ship with a pkg-config file :(.
AC_SEARCH_LIBS([ev_run], [ev], , [AC_MSG_FAILURE([cannot find the required ev_run() function despite trying to link with -lev])])

AC_SEARCH_LIBS([shm_open], [rt])

AC_SEARCH_LIBS([iconv_open], [iconv], , [AC_MSG_FAILURE([cannot find the required iconv_open() function despite trying to link with -liconv])])

AX_PTHREAD

dnl Each prefix corresponds to a source tarball which users might have
dnl downloaded in a newer version and would like to overwrite.
PKG_CHECK_MODULES([LIBSN], [libstartup-notification-1.0])
PKG_CHECK_MODULES([XCB], [xcb xcb-xkb xcb-xinerama xcb-randr])
PKG_CHECK_MODULES([XCB_UTIL], [xcb-event xcb-util])
PKG_CHECK_MODULES([XCB_UTIL_CURSOR], [xcb-cursor])
PKG_CHECK_MODULES([XCB_UTIL_KEYSYMS], [xcb-keysyms])
PKG_CHECK_MODULES([XCB_UTIL_WM], [xcb-icccm])
PKG_CHECK_MODULES([XCB_UTIL_XRM], [xcb-xrm])
PKG_CHECK_MODULES([XKBCOMMON], [xkbcommon xkbcommon-x11])
PKG_CHECK_MODULES([YAJL], [yajl])
PKG_CHECK_MODULES([LIBPCRE], [libpcre >= 8.10])
PKG_CHECK_MODULES([PANGOCAIRO], [cairo >= 1.14.4 pangocairo])

# Checks for programs.
AC_PROG_AWK
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_RANLIB
AC_PROG_LN_S

AC_PATH_PROG([PATH_ASCIIDOC], [asciidoc])
AC_PATH_PROG([PATH_XMLTO], [xmlto])
AC_PATH_PROG([PATH_POD2MAN], [pod2man])

AM_CONDITIONAL([BUILD_MANS], [test x$PATH_ASCIIDOC != x && test x$PATH_XMLTO != x && test x$PATH_POD2MAN != x])
AM_CONDITIONAL([BUILD_DOCS], [test x$PATH_ASCIIDOC != x])

AM_PROG_AR

AX_FLAGS_WARN_ALL
AX_CHECK_COMPILE_FLAG([-Wunused-value], [AX_APPEND_FLAG([-Wunused-value], [AM_CFLAGS])])
AC_SUBST(AM_CFLAGS)

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h float.h inttypes.h limits.h locale.h netinet/in.h paths.h stddef.h stdint.h stdlib.h string.h sys/param.h sys/socket.h sys/time.h unistd.h], , [AC_MSG_FAILURE([cannot find the $ac_header header, which i3 requires])])

AC_CONFIG_FILES([Makefile testcases/lib/i3test.pm man/asciidoc.conf])
AC_CONFIG_FILES([testcases/complete-run.pl], [chmod +x testcases/complete-run.pl])

# Enable address sanitizer for non-release builds. The performance hit is a
# 50% increase of wallclock time for the testsuite on my machine.
if test x$is_release = xyes; then
	default_sanitizers=
else
	default_sanitizers=address
fi
AX_SANITIZERS(, [$default_sanitizers], [AC_DEFINE([I3_ASAN_ENABLED], [], [Enable ASAN])])

AC_OUTPUT

if test -z "${BUILD_DOCS_TRUE}"; then
	print_BUILD_DOCS=yes
else
	print_BUILD_DOCS=no
fi


if test -z "${BUILD_MANS_TRUE}"; then
	print_BUILD_MANS=yes
else
	print_BUILD_MANS=no
fi

in_git_worktree=`git rev-parse --is-inside-work-tree 2>/dev/null`
if [[ "$in_git_worktree" = "true" ]]; then
	git_dir=`git rev-parse --git-dir 2>/dev/null`
	srcdir=`dirname "$git_dir"`
	exclude_dir=`pwd | sed "s,^$srcdir,,g"`
	if ! grep -q "^$exclude_dir" "$git_dir/info/exclude"; then
		echo "$exclude_dir" >> "$git_dir/info/exclude"
	fi
fi

echo \
"--------------------------------------------------------------------------------
build configured:

AS_HELP_STRING([i3 version:], [`echo ${I3_VERSION} | sed 's,\\\\,,g'`])
AS_HELP_STRING([is release version:], [${is_release}])

AS_HELP_STRING([build manpages:], [${print_BUILD_MANS}])
AS_HELP_STRING([build docs:], [${print_BUILD_DOCS}])
AS_HELP_STRING([enable debug flags:], [${ax_enable_debug}])
AS_HELP_STRING([code coverage:], [${CODE_COVERAGE_ENABLED}])
AS_HELP_STRING([enabled sanitizers:], [${ax_enabled_sanitizers}])

To compile, run:

  cd `pwd` && make -j8
--------------------------------------------------------------------------------"
