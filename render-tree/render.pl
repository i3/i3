#!/usr/bin/env perl
# vim:ts=4:sw=4:expandtab
# © 2011 Michael Stapelberg, see LICENSE
#
# Needs SVG (libsvg-perl), IO::All (libio-all-perl), JSON::XS (libjson-xs-perl) and Moose (libmoose-perl)
#
# XXX: unfinished proof-of-concept. awaits a json dump in my.tree, renders to test.svg
# XXX: needs more beautifying (in the SVG but also in the code)
# XXX: has some rendering differences between firefox and chromium. maybe inkscape makes the file look the same in both browsers

use strict;
use warnings;
use SVG;
use Data::Dumper;
use JSON::XS;
use IO::All;
use List::Util qw(sum);
use lib qw(.);
use Con;
use v5.10;

my $input = io('my.tree')->slurp;
my $tree = decode_json($input);
my $root = parse_tree($tree);
render_tree($root);

sub parse_tree {
    my ($input, $parent) = @_;
    my $con = Con->new(name => $input->{name});
    $con->parent($parent) if defined($parent);
    for my $node (@{$input->{nodes}}) {
        $con->add_node(parse_tree($node, $con));
    }

    return $con;
}

sub render_tree {
    my ($con) = @_;
    say 'rendering con ' . $con->name;
    my @nodes = $con->nodes;
    for my $node (@nodes) {
        render_tree($node);
    }

    # nothing to calculate when there are no children
    return unless @nodes > 0;

    $con->width((@nodes > 1 ? (@nodes - 1) * 20 : 0) + sum map { $_->width } @nodes);

    say $con->name . ' has width ' . $con->width;
}

# TODO: figure out the height
my $svg = SVG->new(id => "tree", width => $root->width + 5, height => '1052');

my $l1 = $svg->group(id => 'layer1');

# gaussian blur (for drop shadows)
$svg->defs()->filter(id => 'dropshadow')->fe(-type => 'gaussianblur', stdDeviation => '2.19');

my $idcnt = 0;
my $y = 10;
render_svg($root, 0, 0);

sub render_svg {
    my ($con, $level, $x) = @_;

    my $indent = ' ' x $level;

    say $indent . 'svg-rendering con ' . $con->name . ' on level ' . $level;
    say $indent . 'width: ' . $con->width;

    # render the dropshadow rect
    $l1->rect(
        id => 'outer_rect_shadow' . $idcnt,
        style => 'opacity:1.0;fill:#000000;fill-opacity:1;stroke:#000000;stroke-width:4;stroke-opacity:1;stroke-miterlimit:4;filter:url(#dropshadow)',
        width => "96",
        height => '50',
        #x => $x + ($con->has_parent ? ($con->parent->width - 100) / 2 : 0),
        x => $x + ($con->width / 2) - (96 / 2) + 0,
        y => 4 + $level * 70 + 0,
    );
    $idcnt++;

    # render the main rect
    $l1->rect(
        id => 'outer_rect' . $idcnt,
        style => 'opacity:1.0;fill:#c30000;fill-opacity:1;stroke:#000000;stroke-width:4;stroke-opacity:1;stroke-miterlimit:4',
        width => "96",
        height => '50',
        x => $x + ($con->width / 2) - (96 / 2),
        y => 4 + $level * 70,
    );

    $idcnt++;

    # render the text
    $l1->text(
        style => 'font-size:14px;font-style:normal;font-variant:normal;font-weight:normal;font-stretch:normal;text-align:left;line-height:125%;letter-spacing:0px;word-spacing:0px;text-anchor:middle;fill:#000000;fill-opacity:1;stroke:none;font-family:Trebuchet MS;-inkscape-font-specification:Trebuchet MS',
        x => $x + ($con->width / 2) - (100/2) + 5,
        y => 4 + 15 + $level * 70,
        id => 'title_'.$idcnt,
    )->tspan(style => 'text-align:start;text-anchor:start')->cdata($con->name);
    $idcnt++;

    $y = $y + 50;
    my @nodes = $con->nodes;
    my $startx = $x + ($con->width / 2);

    for my $node (@nodes) {
        render_svg($node, $level + 1, $x);
        my $mid = $x + ($node->width / 2);
        $l1->path(
            d => 'M ' . $startx . ',' . (4 + $level * 70 + 50) . ' ' . $mid . ',' . (4 + ($level+1) * 70),
            id => 'path' . $idcnt,
            style => 'fill:none;stroke:#000000;stroke-width:2px;stroke-linecap:butt;stroke-linejoin:miter;stroke-opacity:1'
        );
        $x += $node->width + 20;
        $idcnt++;
    }

}

$svg->render > io('test.svg');
