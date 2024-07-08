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
# Test that the binding event works properly
# Ticket: #1210
# Ticket: #5323
use i3test i3_autostart => 0;

my $keysym = 't';
my $command = 'nop';
my @mods = ('Shift', 'Ctrl');
my $binding_symbol = join("+", @mods) . "+$keysym";

my $config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

bindsym $binding_symbol $command

bindsym m mode some-mode
mode some-mode {
    bindsym a nop
    bindsym b mode default
}
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

    is($events[0]->{mode}, 'default', 'the `mode` field should be "default"');

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

    # Test that the mode field is correct
    # See #5323.
    @events = events_for(
	sub {
	    qx(xdotool key m);
	    qx(xdotool key a);
	    qx(xdotool key b);
	},
	'binding');
    is(scalar @events, 3, 'Received 3 events');

    is($events[0]->{mode}, 'default', 'Event for binding to enter new mode is atributed to default mode');
    is($events[1]->{mode}, 'some-mode', 'Event for binding while in mode is attributed to the non-default mode');
    is($events[2]->{mode}, 'some-mode', 'Event for binding exiting mode is attributed to the non-default mode');

    exit_gracefully($pid);

}
done_testing;
