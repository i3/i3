package i3test;
# vim:ts=4:sw=4:expandtab
use strict; use warnings;

use File::Temp qw(tmpnam tempfile tempdir);
use Test::Builder;
use X11::XCB::Rect;
use X11::XCB::Window;
use X11::XCB qw(:all);
use AnyEvent::I3;
use List::Util qw(first);
use Time::HiRes qw(sleep);
use Cwd qw(abs_path);
use Scalar::Util qw(blessed);
use SocketActivation;

use v5.10;

# preload
use Test::More ();
use Data::Dumper ();

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
    exit_gracefully
    workspace_exists
    focused_ws
    get_socket_path
    launch_with_config
    wait_for_event
    wait_for_map
    wait_for_unmap
    $x
);

=head1 NAME

i3test - Testcase setup module

=encoding utf-8

=head1 SYNOPSIS

  use i3test;

  my $ws = fresh_workspace;
  is_num_children($ws, 0, 'no containers on this workspace yet');
  cmd 'open';
  is_num_children($ws, 1, 'one container after "open"');

  done_testing;

=head1 DESCRIPTION

This module is used in every i3 testcase and takes care of automatically
starting i3 before any test instructions run. It also saves you typing of lots
of boilerplate in every test file.


i3test automatically "use"s C<Test::More>, C<Data::Dumper>, C<AnyEvent::I3>,
C<Time::HiRes>’s C<sleep> and C<i3test::Test> so that all of them are available
to you in your testcase.

See also C<i3test::Test> (L<http://build.i3wm.org/docs/lib-i3test-test.html>)
which provides additional test instructions (like C<ok> or C<is>).

=cut

my $tester = Test::Builder->new();
my $_cached_socket_path = undef;
my $_sync_window = undef;
my $tmp_socket_path = undef;

our $x;

BEGIN {
    my $window_count = 0;
    sub counter_window {
        return $window_count++;
    }
}

my $i3_pid;
my $i3_autostart;

END {

    # testcases which start i3 manually should always call exit_gracefully
    # on their own. Let’s see, whether they really did.
    if (! $i3_autostart) {
        return unless $i3_pid;

        $tester->ok(undef, 'testcase called exit_gracefully()');
    }

    # don't trigger SIGCHLD handler
    local $SIG{CHLD};

    # From perldoc -v '$?':
    # Inside an "END" subroutine $? contains the value
    # that is going to be given to "exit()".
    #
    # Since waitpid sets $?, we need to localize it,
    # otherwise TAP would be misinterpreted our return status
    local $?;

    # When measuring code coverage, try to exit i3 cleanly (otherwise, .gcda
    # files are not written)
    if ($ENV{COVERAGE} || $ENV{VALGRIND}) {
        exit_gracefully($i3_pid, "/tmp/nested-$ENV{DISPLAY}");

    } else {
        kill(9, $i3_pid)
            or $tester->BAIL_OUT("could not kill i3");

        waitpid $i3_pid, 0;
    }
}

sub import {
    my ($class, %args) = @_;
    my $pkg = caller;

    $i3_autostart = delete($args{i3_autostart}) // 1;

    my $cv = launch_with_config('-default', dont_block => 1)
        if $i3_autostart;

    my $test_more_args = '';
    $test_more_args = join(' ', 'qw(', %args, ')') if keys %args;
    local $@;
    eval << "__";
package $pkg;
use Test::More $test_more_args;
use Data::Dumper;
use AnyEvent::I3;
use Time::HiRes qw(sleep);
use i3test::Test;
__
    $tester->BAIL_OUT("$@") if $@;
    feature->import(":5.10");
    strict->import;
    warnings->import;

    $x ||= i3test::X11->new;
    # set the pointer to a predictable position in case a previous test has
    # disturbed it
    $x->root->warp_pointer(0, 0);
    $cv->recv if $i3_autostart;

    @_ = ($class);
    goto \&Exporter::import;
}

=head1 EXPORT

=head2 wait_for_event($timeout, $callback)

Waits for the next event and calls the given callback for every event to
determine if this is the event we are waiting for.

Can be used to wait until a window is mapped, until a ClientMessage is
received, etc.

  wait_for_event 0.25, sub { $_[0]->{response_type} == MAP_NOTIFY };

=cut
sub wait_for_event {
    my ($timeout, $cb) = @_;

    my $cv = AE::cv;

    $x->flush;

    # unfortunately, there is no constant for this
    my $ae_read = 0;

    my $guard = AE::io $x->get_file_descriptor, $ae_read, sub {
        while (defined(my $event = $x->poll_for_event)) {
            if ($cb->($event)) {
                $cv->send(1);
                last;
            }
        }
    };

    # Trigger timeout after $timeout seconds (can be fractional)
    my $t = AE::timer $timeout, 0, sub { warn "timeout ($timeout secs)"; $cv->send(0) };

    my $result = $cv->recv;
    undef $t;
    undef $guard;
    return $result;
}

=head2 wait_for_map($window)

Thin wrapper around wait_for_event which waits for MAP_NOTIFY.
Make sure to include 'structure_notify' in the window’s event_mask attribute.

This function is called by C<open_window>, so in most cases, you don’t need to
call it on your own. If you need special setup of the window before mapping,
you might have to map it on your own and use this function:

  my $window = open_window(dont_map => 1);
  # Do something special with the window first
  # …

  # Now map it and wait until it’s been mapped
  $window->map;
  wait_for_map($window);

=cut
sub wait_for_map {
    my ($win) = @_;
    my $id = (blessed($win) && $win->isa('X11::XCB::Window')) ? $win->id : $win;
    wait_for_event 4, sub {
        $_[0]->{response_type} == MAP_NOTIFY and $_[0]->{window} == $id
    };
}

=head2 wait_for_unmap($window)

Wrapper around C<wait_for_event> which waits for UNMAP_NOTIFY. Also calls
C<sync_with_i3> to make sure i3 also picked up and processed the UnmapNotify
event.

  my $ws = fresh_workspace;
  my $window = open_window;
  is_num_children($ws, 1, 'one window on workspace');
  $window->unmap;
  wait_for_unmap;
  is_num_children($ws, 0, 'no more windows on this workspace');

=cut
sub wait_for_unmap {
    my ($win) = @_;
    # my $id = (blessed($win) && $win->isa('X11::XCB::Window')) ? $win->id : $win;
    wait_for_event 4, sub {
        $_[0]->{response_type} == UNMAP_NOTIFY # and $_[0]->{window} == $id
    };
    sync_with_i3();
}

=head2 open_window([ $args ])

Opens a new window (see C<X11::XCB::Window>), maps it, waits until it got mapped
and synchronizes with i3.

The following arguments can be passed:

=over 4

=item class

The X11 window class (e.g. WINDOW_CLASS_INPUT_OUTPUT), not to be confused with
the WM_CLASS!

=item rect

An arrayref with 4 members specifying the initial geometry (position and size)
of the window, e.g. C<< [ 0, 100, 70, 50 ] >> for a window appearing at x=0, y=100
with width=70 and height=50.

Note that this is entirely irrelevant for tiling windows.

=item background_color

The background pixel color of the window, formatted as "#rrggbb", like HTML
color codes (e.g. #c0c0c0). This is useful to tell windows apart when actually
watching the testcases.

=item event_mask

An arrayref containing strings which describe the X11 event mask we use for that
window. The default is C<< [ 'structure_notify' ] >>.

=item name

The window’s C<_NET_WM_NAME> (UTF-8 window title). By default, this is "Window
n" with n being replaced by a counter to keep windows apart.

=item dont_map

Set to a true value to avoid mapping the window (making it visible).

=item before_map

A coderef which is called before the window is mapped (unless C<dont_map> is
true). The freshly created C<$window> is passed as C<$_> and as the first
argument.

=back

The default values are equivalent to this call:

  open_window(
    class => WINDOW_CLASS_INPUT_OUTPUT
    rect => [ 0, 0, 30, 30 ]
    background_color => '#c0c0c0'
    event_mask => [ 'structure_notify' ]
    name => 'Window <n>'
  );

Usually, though, calls are simpler:

  my $top_window = open_window;

To identify the resulting window object in i3 commands, use the id property:

  my $top_window = open_window;
  cmd '[id="' . $top_window->id . '"] kill';

=cut
sub open_window {
    my %args = @_ == 1 ? %{$_[0]} : @_;

    my $dont_map = delete $args{dont_map};
    my $before_map = delete $args{before_map};

    $args{class} //= WINDOW_CLASS_INPUT_OUTPUT;
    $args{rect} //= [ 0, 0, 30, 30 ];
    $args{background_color} //= '#c0c0c0';
    $args{event_mask} //= [ 'structure_notify' ];
    $args{name} //= 'Window ' . counter_window();

    my $window = $x->root->create_child(%args);
    $window->add_hint('input');

    if ($before_map) {
        # TODO: investigate why _create is not needed
        $window->_create;
        $before_map->($window) for $window;
    }

    return $window if $dont_map;

    $window->map;
    wait_for_map($window);
    return $window;
}

=head2 open_floating_window([ $args ])

Thin wrapper around open_window which sets window_type to
C<_NET_WM_WINDOW_TYPE_UTILITY> to make the window floating.

The arguments are the same as those of C<open_window>.

=cut
sub open_floating_window {
    my %args = @_ == 1 ? %{$_[0]} : @_;

    $args{window_type} = $x->atom(name => '_NET_WM_WINDOW_TYPE_UTILITY');

    return open_window(\%args);
}

sub open_empty_con {
    my ($i3) = @_;

    my $reply = $i3->command('open')->recv;
    return $reply->[0]->{id};
}

=head2 get_workspace_names()

Returns an arrayref containing the name of every workspace (regardless of its
output) which currently exists.

  my $workspace_names = get_workspace_names;
  is(scalar @$workspace_names, 3, 'three workspaces exist currently');

=cut
sub get_workspace_names {
    my $i3 = i3(get_socket_path());
    my $tree = $i3->get_tree->recv;
    my @outputs = @{$tree->{nodes}};
    my @cons;
    for my $output (@outputs) {
        next if $output->{name} eq '__i3';
        # get the first CT_CON of each output
        my $content = first { $_->{type} eq 'con' } @{$output->{nodes}};
        @cons = (@cons, @{$content->{nodes}});
    }
    [ map { $_->{name} } @cons ]
}

=head2 get_unused_workspace

Returns a workspace name which has not yet been used. See also
C<fresh_workspace> which directly switches to an unused workspace.

  my $ws = get_unused_workspace;
  cmd "workspace $ws";

=cut
sub get_unused_workspace {
    my @names = get_workspace_names();
    my $tmp;
    do { $tmp = tmpnam() } while ((scalar grep { $_ eq $tmp } @names) > 0);
    $tmp
}

=head2 fresh_workspace([ $args ])

Switches to an unused workspace and returns the name of that workspace.

Optionally switches to the specified output first.

    my $ws = fresh_workspace;

    # Get a fresh workspace on the second output.
    my $ws = fresh_workspace(output => 1);

=cut
sub fresh_workspace {
    my %args = @_;
    if (exists($args{output})) {
        my $i3 = i3(get_socket_path());
        my $tree = $i3->get_tree->recv;
        my $output = first { $_->{name} eq "fake-$args{output}" }
                        @{$tree->{nodes}};
        die "BUG: Could not find output $args{output}" unless defined($output);
        # Get the focused workspace on that output and switch to it.
        my $content = first { $_->{type} eq 'con' } @{$output->{nodes}};
        my $focused = $content->{focus}->[0];
        my $workspace = first { $_->{id} == $focused } @{$content->{nodes}};
        $workspace = $workspace->{name};
        cmd("workspace $workspace");
    }

    my $unused = get_unused_workspace;
    cmd("workspace $unused");
    $unused
}

=head2 get_ws($workspace)

Returns the container (from the i3 layout tree) which represents C<$workspace>.

  my $ws = fresh_workspace;
  my $ws_con = get_ws($ws);
  ok(!$ws_con->{urgent}, 'fresh workspace not marked urgent');

Here is an example which counts the number of urgent containers recursively,
starting from the workspace container:

  sub count_urgent {
      my ($con) = @_;

      my @children = (@{$con->{nodes}}, @{$con->{floating_nodes}});
      my $urgent = grep { $_->{urgent} } @children;
      $urgent += count_urgent($_) for @children;
      return $urgent;
  }
  my $urgent = count_urgent(get_ws($ws));
  is($urgent, 3, "three urgent windows on workspace $ws");


=cut
sub get_ws {
    my ($name) = @_;
    my $i3 = i3(get_socket_path());
    my $tree = $i3->get_tree->recv;

    my @outputs = @{$tree->{nodes}};
    my @workspaces;
    for my $output (@outputs) {
        # get the first CT_CON of each output
        my $content = first { $_->{type} eq 'con' } @{$output->{nodes}};
        @workspaces = (@workspaces, @{$content->{nodes}});
    }

    # as there can only be one workspace with this name, we can safely
    # return the first entry
    return first { $_->{name} eq $name } @workspaces;
}

=head2 get_ws_content($workspace)

Returns the content (== tree, starting from the node of a workspace)
of a workspace. If called in array context, also includes the focus
stack of the workspace.

  my $nodes = get_ws_content($ws);
  is(scalar @$nodes, 4, 'there are four containers at workspace-level');

Or, in array context:

  my $window = open_window;
  my ($nodes, $focus) = get_ws_content($ws);
  is($focus->[0], $window->id, 'newly opened window focused');

Note that this function does not do recursion for you! It only returns the
containers B<on workspace level>. If you want to work with all containers (even
nested ones) on a workspace, you have to use recursion:

  # NB: This function does not count floating windows
  sub count_urgent {
      my ($nodes) = @_;

      my $urgent = 0;
      for my $con (@$nodes) {
          $urgent++ if $con->{urgent};
          $urgent += count_urgent($con->{nodes});
      }

      return $urgent;
  }
  my $nodes = get_ws_content($ws);
  my $urgent = count_urgent($nodes);
  is($urgent, 3, "three urgent windows on workspace $ws");

If you also want to deal with floating windows, you have to use C<get_ws>
instead and access C<< ->{nodes} >> and C<< ->{floating_nodes} >> on your own.

=cut
sub get_ws_content {
    my ($name) = @_;
    my $con = get_ws($name);
    return wantarray ? ($con->{nodes}, $con->{focus}) : $con->{nodes};
}

=head2 get_focused($workspace)

Returns the container ID of the currently focused container on C<$workspace>.

Note that the container ID is B<not> the X11 window ID, so comparing the result
of C<get_focused> with a window's C<< ->{id} >> property does B<not> work.

  my $ws = fresh_workspace;
  my $first_window = open_window;
  my $first_id = get_focused();

  my $second_window = open_window;
  my $second_id = get_focused();

  cmd 'focus left';

  is(get_focused($ws), $first_id, 'second window focused');

=cut
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

=head2 get_dock_clients([ $dockarea ])

Returns an array of all dock containers in C<$dockarea> (one of "top" or
"bottom"). If C<$dockarea> is not specified, returns an array of all dock
containers in any dockarea.

  my @docked = get_dock_clients;
  is(scalar @docked, 0, 'no dock clients yet');

=cut
sub get_dock_clients {
    my $which = shift;

    my $tree = i3(get_socket_path())->get_tree->recv;
    my @outputs = @{$tree->{nodes}};
    # Children of all dockareas
    my @docked;
    for my $output (@outputs) {
        if (!defined($which)) {
            @docked = (@docked, map { @{$_->{nodes}} }
                                grep { $_->{type} eq 'dockarea' }
                                @{$output->{nodes}});
        } elsif ($which eq 'top') {
            my $first = first { $_->{type} eq 'dockarea' } @{$output->{nodes}};
            @docked = (@docked, @{$first->{nodes}}) if defined($first);
        } elsif ($which eq 'bottom') {
            my @matching = grep { $_->{type} eq 'dockarea' } @{$output->{nodes}};
            my $last = $matching[-1];
            @docked = (@docked, @{$last->{nodes}}) if defined($last);
        }
    }
    return @docked;
}

=head2 cmd($command)

Sends the specified command to i3 and returns the output.

  my $ws = unused_workspace;
  cmd "workspace $ws";
  cmd 'focus right';

=cut
sub cmd {
    i3(get_socket_path())->command(@_)->recv
}

=head2 workspace_exists($workspace)

Returns true if C<$workspace> is the name of an existing workspace.

  my $old_ws = focused_ws;
  # switch away from where we currently are
  fresh_workspace;

  ok(workspace_exists($old_ws), 'old workspace still exists');

=cut
sub workspace_exists {
    my ($name) = @_;
    (scalar grep { $_ eq $name } @{get_workspace_names()}) > 0;
}

=head2 focused_ws

Returns the name of the currently focused workspace.

  my $ws = focused_ws;
  is($ws, '1', 'i3 starts on workspace 1');

=cut
sub focused_ws {
    my $i3 = i3(get_socket_path());
    my $tree = $i3->get_tree->recv;
    my $focused = $tree->{focus}->[0];
    my $output = first { $_->{id} == $focused } @{$tree->{nodes}};
    my $content = first { $_->{type} eq 'con' } @{$output->{nodes}};
    my $first = first { $_->{fullscreen_mode} == 1 } @{$content->{nodes}};
    return $first->{name}
}

=head2 sync_with_i3([ $args ])

Sends an I3_SYNC ClientMessage with a random value to the root window.
i3 will reply with the same value, but, due to the order of events it
processes, only after all other events are done.

This can be used to ensure the results of a cmd 'focus left' are pushed to
X11 and that C<< $x->input_focus >> returns the correct value afterwards.

See also L<http://build.i3wm.org/docs/testsuite.html> for a longer explanation.

  my $window = open_window;
  $window->add_hint('urgency');
  # Ensure i3 picked up the change
  sync_with_i3;

The only time when you need to use the C<no_cache> argument is when you just
killed your own X11 connection:

  cmd 'kill client';
  # We need to re-establish the X11 connection which we just killed :).
  $x = i3test::X11->new;
  sync_with_i3(no_cache => 1);

=cut
sub sync_with_i3 {
    my %args = @_ == 1 ? %{$_[0]} : @_;

    # Since we need a (mapped) window for receiving a ClientMessage, we create
    # one on the first call of sync_with_i3. It will be re-used in all
    # subsequent calls.
    if (!exists($args{window_id}) &&
        (!defined($_sync_window) || exists($args{no_cache}))) {
        $_sync_window = open_window(
            rect => [ -15, -15, 10, 10 ],
            override_redirect => 1,
        );
    }

    my $window_id = delete $args{window_id};
    $window_id //= $_sync_window->id;

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

         $window_id,    # data[0]: our own window id
         $myrnd, # data[1]: a random value to identify the request
         0,
         0,
         0;

    # Send it to the root window -- since i3 uses the SubstructureRedirect
    # event mask, it will get the ClientMessage.
    $x->send_event(0, $root, EVENT_MASK_SUBSTRUCTURE_REDIRECT, $msg);

    return $myrnd if $args{dont_wait_for_event};

    # now wait until the reply is here
    return wait_for_event 4, sub {
        my ($event) = @_;
        # TODO: const
        return 0 unless $event->{response_type} == 161;

        my ($win, $rnd) = unpack "LL", $event->{data};
        return ($rnd == $myrnd);
    };
}

=head2 exit_gracefully($pid, [ $socketpath ])

Tries to exit i3 gracefully (with the 'exit' cmd) or kills the PID if that fails.

If C<$socketpath> is not specified, C<get_socket_path()> will be called.

You only need to use this function if you have launched i3 on your own with
C<launch_with_config>. Otherwise, it will be automatically called when the
testcase ends.

  use i3test i3_autostart => 0;
  my $pid = launch_with_config($config);
  # …
  exit_gracefully($pid);

=cut
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
        kill(9, $pid)
            or $tester->BAIL_OUT("could not kill i3");
    }

    if ($socketpath =~ m,^/tmp/i3-test-socket-,) {
        unlink($socketpath);
    }

    waitpid $pid, 0;
    undef $i3_pid;
}

=head2 get_socket_path([ $cache ])

Gets the socket path from the C<I3_SOCKET_PATH> atom stored on the X11 root
window. After the first call, this function will return a cached version of the
socket path unless you specify a false value for C<$cache>.

  my $i3 = i3(get_socket_path());
  $i3->command('nop test example')->recv;

To avoid caching:

  my $i3 = i3(get_socket_path(0));

=cut
sub get_socket_path {
    my ($cache) = @_;
    $cache //= 1;

    if ($cache && defined($_cached_socket_path)) {
        return $_cached_socket_path;
    }

    my $atom = $x->atom(name => 'I3_SOCKET_PATH');
    my $cookie = $x->get_property(0, $x->get_root_window(), $atom->id, GET_PROPERTY_TYPE_ANY, 0, 256);
    my $reply = $x->get_property_reply($cookie->{sequence});
    my $socketpath = $reply->{value};
    if ($socketpath eq "/tmp/nested-$ENV{DISPLAY}") {
        $socketpath .= '-activation';
    }
    $_cached_socket_path = $socketpath;
    return $socketpath;
}

=head2 launch_with_config($config, [ $args ])

Launches a new i3 process with C<$config> as configuration file. Useful for
tests which test specific config file directives.

  use i3test i3_autostart => 0;

  my $config = <<EOT;
  # i3 config file (v4)
  for_window [class="borderless"] border none
  for_window [title="special borderless title"] border none
  EOT

  my $pid = launch_with_config($config);

  # …

  exit_gracefully($pid);

=cut
sub launch_with_config {
    my ($config, %args) = @_;

    $tmp_socket_path = "/tmp/nested-$ENV{DISPLAY}";

    $args{dont_create_temp_dir} //= 0;

    my ($fh, $tmpfile) = tempfile("i3-cfg-for-$ENV{TESTNAME}-XXXXX", UNLINK => 1);

    if ($config ne '-default') {
        say $fh $config;
    } else {
        open(my $conf_fh, '<', './i3-test.config')
            or $tester->BAIL_OUT("could not open default config: $!");
        local $/;
        say $fh scalar <$conf_fh>;
    }

    say $fh "ipc-socket $tmp_socket_path"
        unless $args{dont_add_socket_path};

    close($fh);

    my $cv = AnyEvent->condvar;
    $i3_pid = activate_i3(
        unix_socket_path => "$tmp_socket_path-activation",
        display => $ENV{DISPLAY},
        configfile => $tmpfile,
        outdir => $ENV{OUTDIR},
        testname => $ENV{TESTNAME},
        valgrind => $ENV{VALGRIND},
        strace => $ENV{STRACE},
        xtrace => $ENV{XTRACE},
        restart => $ENV{RESTART},
        cv => $cv,
        dont_create_temp_dir => $args{dont_create_temp_dir},
    );

    # force update of the cached socket path in lib/i3test
    # as soon as i3 has started
    $cv->cb(sub { get_socket_path(0) });

    return $cv if $args{dont_block};

    # blockingly wait until i3 is ready
    $cv->recv;

    return $i3_pid;
}

=head1 AUTHOR

Michael Stapelberg <michael@i3wm.org>

=cut

package i3test::X11;
use parent 'X11::XCB::Connection';

sub input_focus {
    my $self = shift;
    i3test::sync_with_i3();

    return $self->SUPER::input_focus(@_);
}

1
