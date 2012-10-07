#!perl
# vim:ts=4:sw=4:expandtab
#
# Please read the following documents before working on tests:
# • http://build.i3wm.org/docs/testsuite.html
#   (or docs/testsuite)
#
# • http://build.i3wm.org/docs/lib-i3test.html
#   (alternatively: perldoc ./testcases/lib/i3test.pm)
#
# • http://build.i3wm.org/docs/ipc.html
#   (or docs/ipc)
#
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Tests the standalone parser binary to see if it calls the right code when
# confronted with various commands, if it prints proper error messages for
# wrong commands and if it terminates in every case.
#
use i3test i3_autostart => 0;
use IPC::Run qw(run);

sub parser_calls {
    my ($command) = @_;

    my $stdout;
    run [ '../test.config_parser', $command ],
        '>&-',
        '2>', \$stdout;
    # TODO: use a timeout, so that we can error out if it doesn’t terminate

    # Filter out all debugging output.
    my @lines = split("\n", $stdout);
    @lines = grep { not /^# / } @lines;

    ## The criteria management calls are irrelevant and not what we want to test
    ## in the first place.
    #@lines = grep { !(/cmd_criteria_init()/ || /cmd_criteria_match_windows/) } @lines;
    return join("\n", @lines) . "\n";
}

my $config = <<'EOT';
mode "meh" {
    bindsym Mod1 + Shift +   x resize grow
    bindcode Mod1+44 resize shrink
}
EOT

my $expected = <<'EOT';
cfg_enter_mode(meh)
cfg_mode_binding(bindsym, Mod1,Shift, x, resize grow)
cfg_mode_binding(bindcode, Mod1, 44, resize shrink)
EOT

is(parser_calls($config),
   $expected,
   'single number (move workspace 3) ok');

$config = <<'EOT';
exec geeqie
exec --no-startup-id /tmp/foo.sh
exec_always firefox
exec_always --no-startup-id /tmp/bar.sh
EOT

$expected = <<'EOT';
cfg_exec(exec, (null), geeqie)
cfg_exec(exec, --no-startup-id, /tmp/foo.sh)
cfg_exec(exec_always, (null), firefox)
cfg_exec(exec_always, --no-startup-id, /tmp/bar.sh)
EOT

is(parser_calls($config),
   $expected,
   'single number (move workspace 3) ok');


done_testing;
