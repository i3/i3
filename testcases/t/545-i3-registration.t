#!perl
# vim:ts=4:sw=4:expandtab
#
# Tests whether our WM registration is done with the correct WM_S0 selection.

use i3test;

my $x = X11::XCB::Connection->new;
my $reply = $x->get_selection_owner($x->atom(name => 'WM_S0')->id);
ok($reply, "registration successful");
done_testing;
