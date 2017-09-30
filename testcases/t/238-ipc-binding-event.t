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
# Test that the binding event works properly
# Ticket: #1210
use i3test i3_autostart => 0;

my $keysym = 't';
my $command = 'nop';
my @mods = ('Shift', 'Ctrl');
my $binding_symbol = join("+", @mods) . "+$keysym";

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

bindsym $binding_symbol $command
EOT

SKIP: {
    qx(which xdotool 2> /dev/null);

    skip 'xdotool is required to test the binding event. `[apt-get install|pacman -S] xdotool`', 1 if $?;

    my $pid = launch_with_config($config);

    my $cv = AnyEvent->condvar;

    my @events = events_for(
	sub {
	    # TODO: this is still flaky: we need to synchronize every X11
	    # connection with i3. Move to XTEST and synchronize that connection.
	    qx(xdotool key $binding_symbol);
	},
	'binding');

    is(scalar @events, 1, 'Received 1 event');

    is($events[0]->{change}, 'run',
        'the `change` field should indicate this binding has run');

    ok($events[0]->{binding},
        'the `binding` field should be a hash that contains information about the binding');

    is($events[0]->{binding}->{input_type}, 'keyboard',
        'the input_type field should be the input type of the binding (keyboard or mouse)');

    note 'the `mods` field should contain the symbols for the modifiers of the binding';
    foreach (@mods) {
        ok(grep(/$_/i, @{$events[0]->{binding}->{mods}}), "`mods` contains the modifier $_");
    }

    is($events[0]->{binding}->{command}, $command,
        'the `command` field should contain the command the binding ran');

    is($events[0]->{binding}->{input_code}, 0,
        'the input_code should be the specified code if the key was bound with bindcode, and otherwise zero');

    exit_gracefully($pid);

}
done_testing;
