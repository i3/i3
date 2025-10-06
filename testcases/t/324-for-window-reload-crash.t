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
# This test ensures that i3 does not crash when a for_window rule triggers a
# 'reload' command.
# Bug still in: 4.24-12-gab6a75a6

use i3test i3_config => <<'EOT';
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

for_window [class="special"] reload
EOT

my $window = open_window(
    wm_class => 'special',
);

does_i3_live;

done_testing;
