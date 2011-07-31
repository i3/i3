# vim:ts=4:sw=4:expandtab
package Con;

use Moose;
use MooseX::AttributeHelpers;
use v5.10;

has 'name' => (is => 'ro', isa => 'Str');
has 'width' => (is => 'rw', isa => 'Int', default => 100);
has '_nodes' => (is => 'ro', metaclass => 'Collection::Array', isa => 'ArrayRef[Con]',
    default => sub { [] },
    provides => {
        'push' => 'add_node',
        elements => 'nodes',
    }
);
has 'parent' => (is => 'rw', isa => 'Con', predicate => 'has_parent');

__PACKAGE__->meta->make_immutable;

1
