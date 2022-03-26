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
# Test using multiple output for 'focus output …'
# Ticket: #4619
use i3test i3_config => <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0,1024x768+0+768,1024x768+1024+768
EOT

###############################################################################
# Test using "next" special keyword
###############################################################################

is(focused_output, "fake-0", 'sanity check');

for (my $i = 1; $i < 9; $i++) {
    cmd 'focus output next';

    my $out = $i % 4;
    is(focused_output, "fake-$out", 'focus output next cycle');
}

###############################################################################
# Same as above but explicitely type all the outputs
###############################################################################

is(focused_output, "fake-0", 'sanity check');

for (my $i = 1; $i < 10; $i++) {
    cmd 'focus output fake-0 fake-1 fake-2 fake-3';

    my $out = $i % 4;
    is(focused_output, "fake-$out", 'focus output next cycle');
}

###############################################################################
# Use a subset of the outputs plus some non-existing outputs
###############################################################################

cmd 'focus output fake-1';
is(focused_output, "fake-1", 'start from fake-1 which is not included in output list');

my @order = (0, 3, 2);
for (my $i = 0; $i < 10; $i++) {
    cmd 'focus output doesnotexist fake-0 alsodoesnotexist fake-3 fake-2';

    my $out = $order[$i % 3];
    is(focused_output, "fake-$out", 'focus output next cycle');
}

done_testing;
