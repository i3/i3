#!perl
# vim:ts=4:sw=4:expandtab
#
# Please read the following documents before working on tests:
# • http://build.i3wm.org/docs/testsuite.html
#   (or docs/testsuite)
#
# • http://build.i3wm.org/docs/lib-i3test.html
#   (alternatively: perldoc ./testcases/lib/i3test.pm)
#
# • http://build.i3wm.org/docs/ipc.html
#   (or docs/ipc)
#
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Tests if a 'move <key> <value>' command will set the environment variable <key> to <value>.
#
use i3test;

use POSIX qw(mkfifo);
use File::Temp qw(:POSIX tempfile);

#####################################################################
# Try to set FOOBAR to baz
#####################################################################

cmd('setenv FOOBAR baz');

my $fifo = tmpnam();
mkfifo($fifo, 0600) or BAIL_OUT("Could not create FIFO in $fifo");

cmd("exec echo \$FOOBAR > $fifo");

open(my $fh, '<', $fifo);
# Block on the FIFO, this will return exactly when the command is done.
my $text = <$fh>;
chomp($text);
close($fh);
unlink($fifo);

is($text, "baz", "simple setenv command");

#####################################################################
# Try to append to FOOBAR, to test expansion
#####################################################################

cmd('setenv FOOBAR foo$FOOBAR');

cmd("exec sh -c 'echo \$FOOBAR > $fifo'");

$fifo = tmpnam();
mkfifo($fifo, 0600) or BAIL_OUT("Could not create FIFO in $fifo");

cmd("exec echo \$FOOBAR > $fifo");

open($fh, '<', $fifo);
# Block on the FIFO, this will return exactly when the command is done.
$text = <$fh>;
chomp($text);
close($fh);
unlink($fifo);

is($text, "foobaz", "setenv expansion");

done_testing;
