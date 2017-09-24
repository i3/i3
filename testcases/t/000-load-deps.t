#!perl
# vim:ts=4:sw=4:expandtab

use Test::More;

BEGIN {
    my @deps = qw(
        X11::XCB::Connection
        X11::XCB::Window
        AnyEvent
        IPC::Run
        ExtUtils::PkgConfig
        Inline
    );
    for my $dep (@deps) {
        use_ok($dep) or BAIL_OUT(qq|The Perl module "$dep" could not be loaded. Please see https://build.i3wm.org/docs/testsuite.html#_installing_the_dependencies|);
    }
}

done_testing;
