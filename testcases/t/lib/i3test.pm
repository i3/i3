package i3test;
# vim:ts=4:sw=4:expandtab

use File::Temp qw(tmpnam);
use Test::Builder;
use X11::XCB::Rect;
use X11::XCB::Window;
use X11::XCB qw(:all);
use AnyEvent::I3;
use List::Util qw(first);
use v5.10;

use Exporter ();
our @EXPORT = qw(get_workspace_names get_unused_workspace get_ws_content get_ws get_focused open_empty_con open_standard_window cmd does_i3_live);

my $tester = Test::Builder->new();

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
use Test::More" . (@_ > 0 ? " qw(@_)" : "") . ";
use Test::Exception;
use Data::Dumper;
use AnyEvent::I3;
use Test::Deep qw(eq_deeply cmp_deeply cmp_set cmp_bag cmp_methods useclass noclass set bag subbagof superbagof subsetof supersetof superhashof subhashof bool str arraylength Isa ignore methods regexprefonly regexpmatches num regexponly scalref reftype hashkeysonly blessed array re hash regexpref hash_each shallow array_each code arrayelementsonly arraylengthonly scalarrefonly listmethods any hashkeys isa);
use v5.10;
use strict;
use warnings;
";
    @_ = ($class);
    goto \&Exporter::import;
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

sub open_empty_con {
    my ($i3) = @_;

    my $reply = $i3->command('open')->recv;
    return $reply->{id};
}

sub get_workspace_names {
    my $i3 = i3("/tmp/nestedcons");
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

sub get_ws {
    my ($name) = @_;
    my $i3 = i3("/tmp/nestedcons");
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

sub cmd {
    i3("/tmp/nestedcons")->command(@_)->recv
}

sub does_i3_live {
    my $tree = i3('/tmp/nestedcons')->get_tree->recv;
    my @nodes = @{$tree->{nodes}};
    my $ok = (@nodes > 0);
    $tester->ok($ok, 'i3 still lives');
    return $ok;
}

1
