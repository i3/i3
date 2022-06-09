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
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Test that i3 doesn't crash if the config contains nested variables.
# Ticket: #5002

use i3test i3_autostart => 0;

#######################################################################
# Test calloc crash
#######################################################################
my $config = <<EOT;
# i3 config file (v4)
set \$xxxxxxxxxx 1
set \$\$xxxxxxxxxx 2
set \$\$\$xxxxxxxxxx 3
set \$\$\$\$xxxxxxxxxx 4
set \$\$\$\$\$xxxxxxxxxx 5
set \$\$\$\$\$\$xxxxxxxxxx 6
set \$\$\$\$\$\$\$xxxxxxxxxx 7
set \$\$\$\$\$\$\$\$xxxxxxxxxx 8
set \$\$\$\$\$\$\$\$\$xxxxxxxxxx 9
set \$\$\$\$\$\$\$\$\$\$xxxxxxxxxx 10
set \$\$\$\$\$\$\$\$\$\$\$xxxxxxxxxx 11
set \$\$\$\$\$\$\$\$\$\$\$\$xxxxxxxxxx 12
set \$\$\$\$\$\$\$\$\$\$\$\$\$xxxxxxxxxx 13
set \$\$\$\$\$\$\$\$\$\$\$\$\$\$xxxxxxxxxx 14
EOT

my $pid = launch_with_config($config);
does_i3_live;

exit_gracefully($pid);


#######################################################################
# Test buffer overflow
#######################################################################
$config = <<EOT;
# i3 config file (v4)
set \$xxxxxxxxxx 1
set \$\$xxxxxxxxxx 2
set \$\$\$xxxxxxxxxx 3
EOT

$pid = launch_with_config($config);
does_i3_live;

exit_gracefully($pid);
done_testing;
