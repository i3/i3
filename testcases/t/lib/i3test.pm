package i3test;
# vim:ts=4:sw=4:expandtab

use File::Temp qw(tmpnam tempfile tempdir);
use Test::Builder;
use X11::XCB::Rect;
use X11::XCB::Window;
use X11::XCB qw(:all);
use AnyEvent::I3;
use List::Util qw(first);
use List::MoreUtils qw(lastval);
use Time::HiRes qw(sleep);
use Try::Tiny;
use Cwd qw(abs_path);
use Proc::Background;

use v5.10;

use Exporter ();
our @EXPORT = qw(get_workspace_names get_unused_workspace fresh_workspace get_ws_content get_ws get_focused open_empty_con open_standard_window get_dock_clients cmd does_i3_live exit_gracefully workspace_exists focused_ws get_socket_path launch_with_config);

my $tester = Test::Builder->new();
my $_cached_socket_path = undef;
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

sub open_standard_window {
    my ($x, $color, $floating) = @_;

    $color ||= '#c0c0c0';

    # We cannot use a hashref here because create_child expands the arguments into an array
    my @args = (
        class => WINDOW_CLASS_INPUT_OUTPUT,
        rect => X11::XCB::Rect->new(x => 0, y => 0, width => 30, height => 30 ),
        background_color => $color,
    );

    if (defined($floating) && $floating) {
        @args = (@args, window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_UTILITY'));
    }

    my $window = $x->root->create_child(@args);

    $window->name('Window ' . counter_window());
    $window->map;

    sleep(0.25);

    return $window;
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
        @cons = grep { $_->{id} == $lf } (@{$con->{nodes}}, @{$con->{'floating_nodes'}});
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
            my $last = lastval { $_->{type} == 5 } @{$output->{nodes}};
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
    try {
        say "Exiting i3 cleanly...";
        i3($socketpath)->command('exit')->recv;
        $exited = 1;
    };

    if (!$exited) {
        kill(9, $pid) or die "could not kill i3";
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
    my ($config) = @_;

    if (!defined($tmp_socket_path)) {
        $tmp_socket_path = File::Temp::tempnam('/tmp', 'i3-test-socket-');
    }

    my ($fh, $tmpfile) = tempfile('i3-test-config-XXXXX', UNLINK => 1);
    say $fh $config;
    say $fh "ipc-socket $tmp_socket_path";
    close($fh);

    my $i3cmd = "exec " . abs_path("../i3") . " -V -d all --disable-signalhandler -c $tmpfile >/dev/null 2>/dev/null";
    my $process = Proc::Background->new($i3cmd);
    sleep 1;

    # force update of the cached socket path in lib/i3test
    get_socket_path(0);

    return $process;
}

1
