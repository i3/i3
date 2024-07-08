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
# Verify i3 does not crash when reloading configuration with invalid match
# criteria.
# Ticket: #6141
# Bug still in: 4.23-47-gbe840af4
use i3test i3_config => <<EOT;
assign [class="class" window_type="some_type"] workspace 1
assign [class="class" window_type="some_type"] output 1
for_window [class="class" window_type="some_type"] workspace 1
no_focus [class="class" window_type="some_type"] workspace 1
EOT

does_i3_live;

cmd 'reload';
does_i3_live;

done_testing;
