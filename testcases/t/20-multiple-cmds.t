#!perl
# vim:ts=4:sw=4:expandtab
#
# Tests multiple commands (using ';') and multiple operations (using ',')
#
use i3test;

my $tmp = fresh_workspace;

sub multiple_cmds {
    my ($cmd) = @_;

    cmd 'open';
    cmd 'open';
    ok(@{get_ws_content($tmp)} == 2, 'two containers opened');

    cmd $cmd;
    ok(@{get_ws_content($tmp)} == 0, "both containers killed (cmd = $cmd)");
}
multiple_cmds('kill;kill');
multiple_cmds('kill; kill');
multiple_cmds('kill ; kill');
multiple_cmds('kill ;kill');
multiple_cmds('kill  ;kill');
multiple_cmds('kill  ;  kill');
multiple_cmds("kill;\tkill");
multiple_cmds("kill\t;kill");
multiple_cmds("kill\t;\tkill");
multiple_cmds("kill\t ;\tkill");
multiple_cmds("kill\t ;\t kill");
multiple_cmds("kill \t ; \t kill");

# TODO: need a non-invasive command before implementing a test which uses ','

done_testing;
