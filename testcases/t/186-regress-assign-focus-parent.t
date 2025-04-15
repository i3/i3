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
# Regression test: New windows were not opened in the correct place if they
# matched an assignment.
# Wrong behaviour manifested itself up to (including) commit
# f78caf8c5815ae7a66de9e4b734546fd740cc19d
#
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

assign [title="testcase"] targetws
EOT

my $i3 = i3(get_socket_path(0));

cmd 'workspace targetws';

open_window(name => "testcase");
is_num_children('targetws', 1, 'precisely one window');

open_window(name => "testcase");
is_num_children('targetws', 2, 'precisely two windows');

cmd 'split v';

open_window(name => "testcase");
is_num_children('targetws', 2, 'still two windows');

# focus parent. the new window should now be opened right next to the last one.
cmd 'focus parent';

open_window(name => "testcase");
is_num_children('targetws', 3, 'new window opened next to last one');

done_testing;
