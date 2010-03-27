package i3test;
# vim:ts=4:sw=4:expandtab

use File::Temp qw(tmpnam);
use X11::XCB::Rect;
use X11::XCB::Window;
use X11::XCB qw(:all);
use AnyEvent::I3;
use Exporter qw(import);
use base 'Exporter';

our @EXPORT = qw(get_workspace_names get_unused_workspace get_ws_content);

BEGIN {
    my $window_count = 0;
    sub counter_window {
        return $window_count++;
    }
}

sub open_standard_window {
    my ($x) = @_;

    my $window = $x->root->create_child(
        class => WINDOW_CLASS_INPUT_OUTPUT,
        rect => [ 0, 0, 30, 30 ],
        background_color => '#C0C0C0',
    );

    $window->name('Window ' . counter_window());
    $window->map;

    sleep(0.25);

    return $window;
}

sub get_workspace_names {
    my $i3 = i3("/tmp/nestedcons");
    # TODO: use correct command as soon as AnyEvent::i3 is updated
    my $tree = $i3->get_workspaces->recv;
    my @workspaces = map { @{$_->{nodes}} } @{$tree->{nodes}};
    [ map { $_->{name} } @workspaces ]
}

sub get_unused_workspace {
    my @names = get_workspace_names();
    my $tmp;
    do { $tmp = tmpnam() } while ($tmp ~~ @names);
    $tmp
}

#
# returns the content (== tree, starting from the node of a workspace)
# of a workspace
#
sub get_ws_content {
    my ($name) = @_;
    my $i3 = i3("/tmp/nestedcons");
    my $tree = $i3->get_workspaces->recv;
    my @ws = map { @{$_->{nodes}} } @{$tree->{nodes}};
    my @cons = map { $_->{nodes} } grep { $_->{name} eq $name } @ws;
    # as there can only be one workspace with this name, we can safely
    # return the first entry
    return $cons[0];
}

1
