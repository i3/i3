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
# Test using multiple outputs for 'move workspace to output …'
# Ticket: #4337
use i3test i3_config => <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0,1024x768+0+768,1024x768+1024+768
EOT

# Test setup: 4 outputs 2 marked windows

open_window;
cmd 'mark aa, move to workspace 1, workspace 1';
open_window;
cmd 'mark ab, move to workspace 3';

sub is_ws {
    my $ws_num = shift;
    my $out_num = shift;
    my $msg = shift;

    local $Test::Builder::Level = $Test::Builder::Level + 1;
    is(get_output_for_workspace("$ws_num"), "fake-$out_num", "Workspace $ws_num -> $out_num: $msg");
}

###############################################################################
# Test moving workspace to same output
# See issue #4691
###############################################################################
is_ws(1, 0, 'sanity check');

my $reply = cmd '[con_mark=aa] move workspace to output fake-0';
is_ws(1, 0, 'workspace did not move');
ok($reply->[0]->{success}, 'reply success');

###############################################################################
# Test using "next" special keyword
###############################################################################

is_ws(1, 0, 'sanity check');
is_ws(3, 2, 'sanity check');

for (my $i = 1; $i < 9; $i++) {
    cmd '[con_mark=a] move workspace to output next';
    my $out1 = $i % 4;
    my $out3 = ($i + 2) % 4;

    is_ws(1, $out1, 'move workspace to next');
    is_ws(3, $out3, 'move workspace to next');
}

###############################################################################
# Same as above but explicitely type all the outputs
###############################################################################

is_ws(1, 0, 'sanity check');
is_ws(3, 2, 'sanity check');

for (my $i = 1; $i < 10; $i++) {
    cmd '[con_mark=a] move workspace to output fake-0 fake-1 fake-2 fake-3';
    my $out1 = $i % 4;
    my $out3 = ($i + 2) % 4;

    is_ws(1, $out1, 'cycle through explicit outputs');
    is_ws(3, $out3, 'cycle through explicit outputs');
}

###############################################################################
# Use a subset of the outputs plus some non-existing outputs
###############################################################################

cmd '[con_mark=aa] move workspace to output fake-1';
cmd '[con_mark=ab] move workspace to output fake-1';
is_ws(1, 1, 'start from fake-1 which is not included in output list');
is_ws(3, 1, 'start from fake-1 which is not included in output list');

my @order = (0, 3, 2);
for (my $i = 0; $i < 10; $i++) {
    cmd '[con_mark=a] move workspace to output doesnotexist fake-0 alsodoesnotexist fake-3 fake-2';

    my $out = $order[$i % 3];
    is_ws(1, $out, 'cycle through shuffled outputs');
    is_ws(3, $out, 'cycle through shuffled outputs');

}

done_testing;
