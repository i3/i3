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
# Verify that i3 does not crash when a floating command is run and for_window
# rule exists.
# Ticket: #6561
# Bug still in: 4.25-6-g0e2e8290
use i3test i3_config => <<EOT;
for_window [class=xxx] nop
EOT

cmd 'floating toggle';
does_i3_live;

done_testing;
