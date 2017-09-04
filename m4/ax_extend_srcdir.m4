# ===========================================================================
#     http://www.gnu.org/software/autoconf-archive/ax_extend_srcdir.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_EXTEND_SRCDIR
#
# DESCRIPTION
#
#   The AX_EXTEND_SRCDIR macro extends $srcdir by one path component.
#
#   As an example, when working in /home/michael/i3-4.12/build and calling
#   ../configure, your $srcdir is "..". After calling AX_EXTEND_SRCDIR,
#   $srcdir will be set to "../../i3-4.12".
#
#   The result of extending $srcdir is that filenames (e.g. in the output of
#   the "backtrace" gdb command) will include one more path component of the
#   absolute source path. The additional path component makes it easy for
#   users to recognize which files belong to the PACKAGE, and -- provided a
#   dist tarball was unpacked -- which version of PACKAGE was used.
#
#   As an example, in "backtrace", you will see:
#
#     #0  main (argc=1, argv=0x7fffffff1fc8) at ../../i3-4.12/src/main.c:187
#
#   instead of:
#
#     #0  main (argc=1, argv=0x7fffffff1fc8) at ../src/main.c:187
#
#   In case your code uses the __FILE__ preprocessor directive to refer to
#   the filename of the current source file (e.g. in debug messages), using
#   the extended path might be undesirable. For this purpose,
#   AX_EXTEND_SRCDIR defines the output variable AX_EXTEND_SRCDIR_CPPFLAGS,
#   which can be added to AM_CPPFLAGS in Makefile.am in order to define the
#   preprocessor directive STRIPPED__FILE__. As an example, when compiling
#   the file "../../i3-4.12/src/main.c", STRIPPED__FILE__ evaluates to
#   "main.c".
#
#   There are some caveats: When $srcdir is "." (i.e. when ./configure was
#   called instead of ../configure in a separate build directory),
#   AX_EXTEND_SRCDIR will still extend $srcdir, but the intended effect will
#   not be achieved because of the way automake specifies file paths:
#   automake defines COMPILE to use "`test -f '$source' || echo
#   '\$(srcdir)/'`$source" in order to prefer files in the current directory
#   over specifying $srcdir explicitly.
#
#   The AX_EXTEND_SRCDIR author is not aware of any way to influence this
#   automake behavior. Patches very welcome.
#
#   To work around this issue, you can use AX_ENABLE_BUILDDIR i.e. by adding
#   the following code to configure.ac:
#
#     AX_ENABLE_BUILDDIR
#     dnl ...
#     AX_EXTEND_SRCDIR
#
#   Then also add this bit to Makefile.am (if you wish to use
#   STRIPPED__FILE__ in your code):
#
#     AM_CPPFLAGS = @AX_EXTEND_SRCDIR_CPPFLAGS@
#
# LICENSE
#
#   Copyright (c) 2016 Michael Stapelberg <michael@i3wm.org>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved.  This file is offered as-is, without any
#   warranty.

#serial 3

AC_DEFUN([AX_EXTEND_SRCDIR],
[dnl
AS_CASE([$srcdir],
  [.|.*|/*],
  [
    # pwd -P is specified in IEEE 1003.1 from 2004
    as_dir=`cd "$srcdir" && pwd -P`
    as_base=`AS_BASENAME([$as_dir])`
    srcdir=${srcdir}/../${as_base}

    AC_SUBST([AX_EXTEND_SRCDIR_CPPFLAGS], ["-DSTRIPPED__FILE__=AS_ESCAPE([\"$$(basename $<)\"])"])
  ])
])dnl AX_EXTEND_SRCDIR
