package i3test;
# vim:ts=4:sw=4:expandtab
use strict; use warnings;

use File::Temp qw(tmpnam tempfile tempdir);
use Test::Builder;
use X11::XCB::Rect;
use X11::XCB::Window;
use X11::XCB qw(:all);
use AnyEvent::I3;
use EV;
use List::Util qw(first);
use Time::HiRes qw(sleep);
use Cwd qw(abs_path);
use SocketActivation;

use v5.10;

use Exporter ();
our @EXPORT = qw(
    get_workspace_names
    get_unused_workspace
    fresh_workspace
    get_ws_content
    get_ws
    get_focused
    open_empty_con
    open_window
    open_floating_window
    get_dock_clients
    cmd
    sync_with_i3
    does_i3_live
    exit_gracefully
    workspace_exists
    focused_ws
    get_socket_path
    launch_with_config
    wait_for_event
    wait_for_map
    wait_for_unmap
);

my $tester = Test::Builder->new();
my $_cached_socket_path = undef;
my $_sync_window = undef;
my $tmp_socket_path = undef;

BEGIN {
    my $window_count = 0;
    sub counter_window {
        return $window_count++;
    }
}

sub import {
    my $class = shift;
    my $pkg = caller;
    eval "package $pkg;
use Test::Most" . (@_ > 0 ? " qw(@_)" : "") . ";
use Data::Dumper;
use AnyEvent::I3;
use Time::HiRes qw(sleep);
use Test::Deep qw(eq_deeply cmp_deeply cmp_set cmp_bag cmp_methods useclass noclass set bag subbagof superbagof subsetof supersetof superhashof subhashof bool str arraylength Isa ignore methods regexprefonly regexpmatches num regexponly scalref reftype hashkeysonly blessed array re hash regexpref hash_each shallow array_each code arrayelementsonly arraylengthonly scalarrefonly listmethods any hashkeys isa);
use v5.10;
use strict;
use warnings;
";
    @_ = ($class);
    goto \&Exporter::import;
}

#
# Waits for the next event and calls the given callback for every event to
# determine if this is the event we are waiting for.
#
# Can be used to wait until a window is mapped, until a ClientMessage is
# received, etc.
#
# wait_for_event $x, 0.25, sub { $_[0]->{response_type} == MAP_NOTIFY };
#
sub wait_for_event {
    my ($x, $timeout, $cb) = @_;

    my $cv = AE::cv;

    my $prep = EV::prepare sub {
        $x->flush;
    };

    my $check = EV::check sub {
        while (defined(my $event = $x->poll_for_event)) {
            if ($cb->($event)) {
                $cv->send(1);
                last;
            }
        }
    };

    my $watcher = EV::io $x->get_file_descriptor, EV::READ, sub {
        # do nothing, we only need this watcher so that EV picks up the events
    };

    # Trigger timeout after $timeout seconds (can be fractional)
    my $t = AE::timer $timeout, 0, sub { warn "timeout ($timeout secs)"; $cv->send(0) };

    my $result = $cv->recv;
    undef $t;
    return $result;
}

# thin wrapper around wait_for_event which waits for MAP_NOTIFY
# make sure to include 'structure_notify' in the window’s event_mask attribute
sub wait_for_map {
    my ($x) = @_;
    wait_for_event $x, 2, sub { $_[0]->{response_type} == MAP_NOTIFY };
}

# Wrapper around wait_for_event which waits for UNMAP_NOTIFY. Also calls
# sync_with_i3 to make sure i3 also picked up and processed the UnmapNotify
# event.
sub wait_for_unmap {
    my ($x) = @_;
    wait_for_event $x, 2, sub { $_[0]->{response_type} == UNMAP_NOTIFY };
    sync_with_i3($x);
}

#
# Opens a new window (see X11::XCB::Window), maps it, waits until it got mapped
# and synchronizes with i3.
#
# set dont_map to a true value to avoid mapping
#
# default values:
#     class => WINDOW_CLASS_INPUT_OUTPUT
#     rect => [ 0, 0, 30, 30 ]
#     background_color => '#c0c0c0'
#     event_mask => [ 'structure_notify' ]
#     name => 'Window <n>'
#
sub open_window {
    my ($x, $args) = @_;
    my %args = ($args ? %$args : ());

    my $dont_map = delete $args{dont_map};

    $args{class} //= WINDOW_CLASS_INPUT_OUTPUT;
    $args{rect} //= [ 0, 0, 30, 30 ];
    $args{background_color} //= '#c0c0c0';
    $args{event_mask} //= [ 'structure_notify' ];
    $args{name} //= 'Window ' . counter_window();

    my $window = $x->root->create_child(%args);

    return $window if $dont_map;

    $window->map;
    wait_for_map($x);
    # We sync with i3 here to make sure $x->input_focus is updated.
    sync_with_i3($x);
    return $window;
}

# Thin wrapper around open_window which sets window_type to
# _NET_WM_WINDOW_TYPE_UTILITY to make the window floating.
sub open_floating_window {
    my ($x, $args) = @_;
    my %args = ($args ? %$args : ());

    $args{window_type} = $x->atom(name => '_NET_WM_WINDOW_TYPE_UTILITY');

    return open_window($x, \%args);
}

sub open_empty_con {
    my ($i3) = @_;

    my $reply = $i3->command('open')->recv;
    return $reply->{id};
}

sub get_workspace_names {
    my $i3 = i3(get_socket_path());
    my $tree = $i3->get_tree->recv;
    my @outputs = @{$tree->{nodes}};
    my @cons;
    for my $output (@outputs) {
        # get the first CT_CON of each output
        my $content = first { $_->{type} == 2 } @{$output->{nodes}};
        @cons = (@cons, @{$content->{nodes}});
    }
    [ map { $_->{name} } @cons ]
}

sub get_unused_workspace {
    my @names = get_workspace_names();
    my $tmp;
    do { $tmp = tmpnam() } while ($tmp ~~ @names);
    $tmp
}

sub fresh_workspace {
    my $unused = get_unused_workspace;
    cmd("workspace $unused");
    $unused
}

sub get_ws {
    my ($name) = @_;
    my $i3 = i3(get_socket_path());
    my $tree = $i3->get_tree->recv;

    my @outputs = @{$tree->{nodes}};
    my @workspaces;
    for my $output (@outputs) {
        # get the first CT_CON of each output
        my $content = first { $_->{type} == 2 } @{$output->{nodes}};
        @workspaces = (@workspaces, @{$content->{nodes}});
    }

    # as there can only be one workspace with this name, we can safely
    # return the first entry
    return first { $_->{name} eq $name } @workspaces;
}

#
# returns the content (== tree, starting from the node of a workspace)
# of a workspace. If called in array context, also includes the focus
# stack of the workspace
#
sub get_ws_content {
    my ($name) = @_;
    my $con = get_ws($name);
    return wantarray ? ($con->{nodes}, $con->{focus}) : $con->{nodes};
}

sub get_focused {
    my ($ws) = @_;
    my $con = get_ws($ws);

    my @focused = @{$con->{focus}};
    my $lf;
    while (@focused > 0) {
        $lf = $focused[0];
        last unless defined($con->{focus});
        @focused = @{$con->{focus}};
        my @cons = grep { $_->{id} == $lf } (@{$con->{nodes}}, @{$con->{'floating_nodes'}});
        $con = $cons[0];
    }

    return $lf;
}

sub get_dock_clients {
    my $which = shift;

    my $tree = i3(get_socket_path())->get_tree->recv;
    my @outputs = @{$tree->{nodes}};
    # Children of all dockareas
    my @docked;
    for my $output (@outputs) {
        if (!defined($which)) {
            @docked = (@docked, map { @{$_->{nodes}} }
                                grep { $_->{type} == 5 }
                                @{$output->{nodes}});
        } elsif ($which eq 'top') {
            my $first = first { $_->{type} == 5 } @{$output->{nodes}};
            @docked = (@docked, @{$first->{nodes}});
        } elsif ($which eq 'bottom') {
            my @matching = grep { $_->{type} == 5 } @{$output->{nodes}};
            my $last = $matching[-1];
            @docked = (@docked, @{$last->{nodes}});
        }
    }
    return @docked;
}

sub cmd {
    i3(get_socket_path())->command(@_)->recv
}

sub workspace_exists {
    my ($name) = @_;
    ($name ~~ @{get_workspace_names()})
}

sub focused_ws {
    my $i3 = i3(get_socket_path());
    my $tree = $i3->get_tree->recv;
    my @outputs = @{$tree->{nodes}};
    my @cons;
    for my $output (@outputs) {
        # get the first CT_CON of each output
        my $content = first { $_->{type} == 2 } @{$output->{nodes}};
        my $first = first { $_->{fullscreen_mode} == 1 } @{$content->{nodes}};
        return $first->{name}
    }
}

#
# Sends an I3_SYNC ClientMessage with a random value to the root window.
# i3 will reply with the same value, but, due to the order of events it
# processes, only after all other events are done.
#
# This can be used to ensure the results of a cmd 'focus left' are pushed to
# X11 and that $x->input_focus returns the correct value afterwards.
#
# See also docs/testsuite for a long explanation
#
sub sync_with_i3 {
    my ($x) = @_;

    # Since we need a (mapped) window for receiving a ClientMessage, we create
    # one on the first call of sync_with_i3. It will be re-used in all
    # subsequent calls.
    if (!defined($_sync_window)) {
        $_sync_window = $x->root->create_child(
            class => WINDOW_CLASS_INPUT_OUTPUT,
            rect => X11::XCB::Rect->new(x => -15, y => -15, width => 10, height => 10 ),
            override_redirect => 1,
            background_color => '#ff0000',
            event_mask => [ 'structure_notify' ],
        );

        $_sync_window->map;

        wait_for_event $x, 2, sub { $_[0]->{response_type} == MAP_NOTIFY };
    }

    my $root = $x->get_root_window();
    # Generate a random number to identify this particular ClientMessage.
    my $myrnd = int(rand(255)) + 1;

    # Generate a ClientMessage, see xcb_client_message_t
    my $msg = pack "CCSLLLLLLL",
         CLIENT_MESSAGE, # response_type
         32,     # format
         0,      # sequence
         $root,  # destination window
         $x->atom(name => 'I3_SYNC')->id,

         $_sync_window->id,    # data[0]: our own window id
         $myrnd, # data[1]: a random value to identify the request
         0,
         0,
         0;

    # Send it to the root window -- since i3 uses the SubstructureRedirect
    # event mask, it will get the ClientMessage.
    $x->send_event(0, $root, EVENT_MASK_SUBSTRUCTURE_REDIRECT, $msg);

    # now wait until the reply is here
    return wait_for_event $x, 2, sub {
        my ($event) = @_;
        # TODO: const
        return 0 unless $event->{response_type} == 161;

        my ($win, $rnd) = unpack "LL", $event->{data};
        return ($rnd == $myrnd);
    };
}

sub does_i3_live {
    my $tree = i3(get_socket_path())->get_tree->recv;
    my @nodes = @{$tree->{nodes}};
    my $ok = (@nodes > 0);
    $tester->ok($ok, 'i3 still lives');
    return $ok;
}

# Tries to exit i3 gracefully (with the 'exit' cmd) or kills the PID if that fails
sub exit_gracefully {
    my ($pid, $socketpath) = @_;
    $socketpath ||= get_socket_path();

    my $exited = 0;
    eval {
        say "Exiting i3 cleanly...";
        i3($socketpath)->command('exit')->recv;
        $exited = 1;
    };

    if (!$exited) {
        kill(9, $pid) or die "could not kill i3";
    }

    if ($socketpath =~ m,^/tmp/i3-test-socket-,) {
        unlink($socketpath);
    }
}

# Gets the socket path from the I3_SOCKET_PATH atom stored on the X11 root window
sub get_socket_path {
    my ($cache) = @_;
    $cache ||= 1;

    if ($cache && defined($_cached_socket_path)) {
        return $_cached_socket_path;
    }

    my $x = X11::XCB::Connection->new;
    my $atom = $x->atom(name => 'I3_SOCKET_PATH');
    my $cookie = $x->get_property(0, $x->get_root_window(), $atom->id, GET_PROPERTY_TYPE_ANY, 0, 256);
    my $reply = $x->get_property_reply($cookie->{sequence});
    my $socketpath = $reply->{value};
    $_cached_socket_path = $socketpath;
    return $socketpath;
}

#
# launches a new i3 process with the given string as configuration file.
# useful for tests which test specific config file directives.
#
# be sure to use !NO_I3_INSTANCE! somewhere in the file to signal
# complete-run.pl that it should not create an instance of i3
#
sub launch_with_config {
    my ($config, $dont_add_socket_path) = @_;

    $dont_add_socket_path //= 0;

    if (!defined($tmp_socket_path)) {
        $tmp_socket_path = File::Temp::tempnam('/tmp', 'i3-test-socket-');
    }

    my ($fh, $tmpfile) = tempfile('/tmp/i3-test-config-XXXXX', UNLINK => 1);
    say $fh $config;
    say $fh "ipc-socket $tmp_socket_path" unless $dont_add_socket_path;
    close($fh);

    my $cv = AnyEvent->condvar;
    my $pid = activate_i3(
        unix_socket_path => "$tmp_socket_path-activation",
        display => $ENV{DISPLAY},
        configfile => $tmpfile,
        outdir => $ENV{OUTDIR},
        testname => $ENV{TESTNAME},
        valgrind => $ENV{VALGRIND},
        strace => $ENV{STRACE},
        cv => $cv,
    );

    # blockingly wait until i3 is ready
    $cv->recv;

    # force update of the cached socket path in lib/i3test
    get_socket_path(0);

    return $pid;
}

package i3test::X11;
use parent 'X11::XCB::Connection';

sub input_focus {
    my $self = shift;
    i3test::sync_with_i3($self);

    return $self->SUPER::input_focus(@_);
}

1
