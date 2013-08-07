#!/usr/bin/env perl
# vim:ts=4:sw=4:expandtab
# renders the layout tree using asymptote

use strict;
use warnings;

use JSON::XS;
use Data::Dumper;
use AnyEvent::I3;
use v5.10;

use Gtk2 '-init';
use Gtk2::SimpleMenu;
use Glib qw/TRUE FALSE/;

my $window = Gtk2::Window->new('toplevel');
$window->signal_connect('delete_event' => sub { Gtk2->main_quit; });

my $tree_store = Gtk2::TreeStore->new(qw/Glib::String/, qw/Glib::String/, qw/Glib::String/, qw/Glib::String/, qw/Glib::String/, qw/Glib::String/, qw/Glib::String/, qw/Glib::String/);

my $i3 = i3();

my $tree_view = Gtk2::TreeView->new($tree_store);

my $layout_box = undef;

sub copy_node {
    my ($n, $parent, $piter, $pbox) = @_;

    my $o = ($n->{orientation} == 0 ? "u" : ($n->{orientation} == 1 ? "h" : "v"));
    my $w = (defined($n->{window}) ? $n->{window} : "N");

    # convert a rectangle struct to X11 notation (WxH+X+Y)
    my $r = $n->{rect};
    my $x = $r->{x};
    my $y = $r->{y};
    my $dim = $r->{width}."x".$r->{height}.($x<0?$x:"+$x").($y<0?$y:"+$y");

    # add node to the tree with all known properties
    my $iter = $tree_store->append($piter);
    $tree_store->set($iter, 0 => $n->{name}, 1 => $w, 2 => $o, 3 => sprintf("0x%08x", $n->{id}), 4 => $n->{urgent}, 5 => $n->{focused}, 6 => $n->{layout}, 7 => $dim);

    # also create a box for the node, each node has a vbox
    # for combining the title (and properties) with the
    # container itself, the container will be empty in case
    # of no children, a vbox or hbox
    my $box;
    if($n->{orientation} == 1) {
	 $box = Gtk2::HBox->new(1, 5);
    } else {
	 $box = Gtk2::VBox->new(1, 5);
    }

    # combine label and container
    my $node = Gtk2::Frame->new($n->{name}.",".$o.",".$w);
    $node->set_shadow_type('etched-out');
    $node->add($box);

    # the parent is added onto a scrolled window, so add it with a viewport
    if(defined($pbox)) {
    	$pbox->pack_start($node, 1, 1, 0);
    } else {
	$layout_box = $node;
    }

    # recurse into children
    copy_node($_, $n, $iter, $box) for @{$n->{nodes}};

    # if it is a window draw a nice color
    if(defined($n->{window})) {
	# use a drawing area to fill a colored rectangle
	my $area = Gtk2::DrawingArea->new();

	# the color is stored as hex in the name
	$area->{"user-data"} = $n->{name};

	$area->signal_connect(expose_event => sub {
	    my ($widget, $event) = @_;

	    # fetch a cairo context and it width/height to start drawing nodes
	    my $cr = Gtk2::Gdk::Cairo::Context->create($widget->window());

	    my $w = $widget->allocation->width;
	    my $h = $widget->allocation->height;

	    my $hc  = $widget->{"user-data"};
	    my $r = hex(substr($hc, 1, 2)) / 255.0;
	    my $g = hex(substr($hc, 3, 2)) / 255.0;
	    my $b = hex(substr($hc, 5, 2)) / 255.0;

	    $cr->set_source_rgb($r, $g, $b);
	    $cr->rectangle(0, 0, $w, $h);
	    $cr->fill();

        return FALSE;
	});

    $box->pack_end($area, 1, 1, 0);
    }
}

# Replaced by Gtk2 Boxes:
#sub draw_node {
#    my ($n, $cr, $x, $y, $w, $h) = @_;
#
#    $cr->set_source_rgb(1.0, 1.0, 1.0);
#    $cr->rectangle($x, $y, $w/2, $h/2);
#    $cr->fill();
#}

my $json_prev = "";

my $layout_sw = Gtk2::ScrolledWindow->new(undef, undef);
my $layout_container = Gtk2::HBox->new(0, 0);
$layout_sw->add_with_viewport($layout_container);

sub copy_tree {
    my $tree = $i3->get_tree->recv;

    # convert the tree back to json so we only rebuild/redraw when the tree is changed
    my $json = encode_json($tree);
    if ($json ne $json_prev) {
        $json_prev = $json;

        # rebuild the tree and the layout
        $tree_store->clear();
        if(defined($layout_box)) {
            $layout_container->remove($layout_box);
        }
        copy_node($tree);
        $layout_container->add($layout_box);
        $layout_container->show_all();

        # keep things expanded, otherwise the tree collapses every reload which is more annoying then this :-)
        $tree_view->expand_all();
    }

    return(TRUE);
}

sub new_column {
    my $tree_column = Gtk2::TreeViewColumn->new();
    $tree_column->set_title(shift);

    my $renderer = Gtk2::CellRendererText->new();
    $tree_column->pack_start($renderer, FALSE);
    $tree_column->add_attribute($renderer, text => shift);

    return($tree_column);
}

my $col = 0;
$tree_view->append_column(new_column("Name", $col++));
$tree_view->append_column(new_column("Window", $col++));
$tree_view->append_column(new_column("Orientation", $col++));
$tree_view->append_column(new_column("ID", $col++));
$tree_view->append_column(new_column("Urgent", $col++));
$tree_view->append_column(new_column("Focused", $col++));
$tree_view->append_column(new_column("Layout", $col++));
$tree_view->append_column(new_column("Rect", $col++));

$tree_view->set_grid_lines("both");

my $tree_sw = Gtk2::ScrolledWindow->new(undef, undef);
$tree_sw->add($tree_view);

# Replaced by Gtk2 Boxes:
#my $area = Gtk2::DrawingArea->new();
#$area->signal_connect(expose_event => sub {
#    my ($widget, $event) = @_;
#
#    # fetch a cairo context and it width/height to start drawing nodes
#    my $cr = Gtk2::Gdk::Cairo::Context->create($widget->window());
#
#    my $w = $widget->allocation->width;
#    my $h = $widget->allocation->height;
#
#    draw_node($gtree, $cr, 0, 0, $w, $h);
#
#    return FALSE;
#});

sub menu_export {
    print("TODO: EXPORT\n");
}

my $menu_tree = [
  	_File => {
		item_type => '<Branch>',
		children => [
			_Export => {
				callback => \&menu_export,
				accelerator => '<ctrl>E',
			},
			_Quit => {
				callback => sub { Gtk2->main_quit; },
				accelerator => '<ctrl>Q',
			},
		],
	},
];

my $menu = Gtk2::SimpleMenu->new(menu_tree => $menu_tree);

my $vbox = Gtk2::VBox->new(0, 0);
$vbox->pack_start($menu->{widget}, 0, 0, 0);
$vbox->pack_end($tree_sw, 1, 1, 0);
$vbox->pack_end($layout_sw, 1, 1, 0);

$window->add($vbox);
$window->show_all();
$window->set_size_request(500,500);

Glib::Timeout->add(1000, "copy_tree", undef, Glib::G_PRIORITY_DEFAULT);
copy_tree();

Gtk2->main();

