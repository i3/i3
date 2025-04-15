#!perl
# vim:ts=4:sw=4:expandtab
#
# Please read the following documents before working on tests:
# • https://build.i3wm.org/docs/testsuite.html
#   (or docs/testsuite)
#
# • https://build.i3wm.org/docs/lib-i3test.html
#   (alternatively: perldoc ./testcases/lib/i3test.pm)
#
# • https://build.i3wm.org/docs/ipc.html
#   (or docs/ipc)
#
# • https://i3wm.org/downloads/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Test that i3 doesn't crash if the config contains nested variables.
# Ticket: #5002

use i3test i3_autostart => 0;

#######################################################################
# Test calloc crash
#######################################################################

my $config = <<'EOT';
set $long_variable_name_with_short_value 1
set $$long_variable_name_with_short_value 2
set $$$long_variable_name_with_short_value 3
EOT

my $pid = launch_with_config($config);

# ==2108678==ERROR: AddressSanitizer: requested allocation size 0xffffffffffffffe1 (0x7e8 after adjustments for alignment, red zones etc.) exceeds maximum supported size of 0x10000000000 (thread T0)
#     #0 0x7feaa9cbf411 in __interceptor_calloc /usr/src/debug/gcc/libsanitizer/asan/asan_malloc_linux.cpp:77
#     #1 0x560867c0c13d in scalloc ../libi3/safewrappers.c:29
#     #2 0x560867b7bd63 in parse_file ../src/config_parser.c:1030
#     #3 0x560867b6b1b8 in load_configuration ../src/config.c:261
#     #4 0x560867baf9cf in main ../src/main.c:677
#     #5 0x7feaa95b52cf  (/usr/lib/libc.so.6+0x232cf)

does_i3_live;

exit_gracefully($pid);


#######################################################################
# Test buffer overflow
#######################################################################

$config = <<'EOT';
set $x 1
set $$x 2
EOT

$pid = launch_with_config($config);

# ==2110007==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x6070000014f2 at pc 0x558590f680ba bp 0x7ffced72b760 sp 0x7ffced72b750
# WRITE of size 1 at 0x6070000014f2 thread T0
#     #0 0x558590f680b9 in parse_file ../src/config_parser.c:1051
#     #1 0x558590f571b8 in load_configuration ../src/config.c:261
#     #2 0x558590f9b9cf in main ../src/main.c:677
#     #3 0x7f81c61c82cf  (/usr/lib/libc.so.6+0x232cf)
#     #4 0x7f81c61c8389 in __libc_start_main (/usr/lib/libc.so.6+0x23389)
#     #5 0x558590f0bd54 in _start ../sysdeps/x86_64/start.S:115

# 0x6070000014f2 is located 0 bytes to the right of 66-byte region [0x6070000014b0,0x6070000014f2)
# allocated by thread T0 here:
#     #0 0x7f81c68bf411 in __interceptor_calloc /usr/src/debug/gcc/libsanitizer/asan/asan_malloc_linux.cpp:77
#     #1 0x558590ff813d in scalloc ../libi3/safewrappers.c:29
#     #2 0x558590f67d63 in parse_file ../src/config_parser.c:1030
#     #3 0x558590f571b8 in load_configuration ../src/config.c:261
#     #4 0x558590f9b9cf in main ../src/main.c:677
#     #5 0x7f81c61c82cf  (/usr/lib/libc.so.6+0x232cf)

does_i3_live;

exit_gracefully($pid);
done_testing;
