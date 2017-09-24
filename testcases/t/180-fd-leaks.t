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
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Verifies that i3 does not leak any file descriptors in 'exec'.
#
use i3test;
use POSIX qw(mkfifo);
use File::Temp qw(:POSIX tempfile);

SKIP: {
skip "Procfs not available on $^O", 1 if $^O eq 'openbsd';

my $i3 = i3(get_socket_path());

my $tmp = tmpnam();
mkfifo($tmp, 0600) or die "Could not create FIFO in $tmp";
my ($outfh, $outname) = tempfile('/tmp/i3-ls-output.XXXXXX', UNLINK => 1);

cmd qq|exec ls -l /proc/self/fd >$outname && echo done >$tmp|;

open(my $fh, '<', $tmp);
# Block on the FIFO, this will return exactly when the command is done.
<$fh>;
close($fh);
unlink($tmp);

# Get the ls /proc/self/fd output
my $output;
{
    local $/;
    $output = <$outfh>;
}
close($outfh);

# Split lines, keep only those which are symlinks.
my @lines = grep { /->/ } split("\n", $output);

my %fds = map { /([0-9]+) -> (.+)$/; ($1, $2) } @lines;

# Filter out 0, 1, 2 (stdin, stdout, stderr).
delete $fds{0};
delete $fds{1};
delete $fds{2};

# Filter out the fd which is caused by ls calling readdir().
for my $fd (keys %fds) {
    delete $fds{$fd} if $fds{$fd} =~ m,^/proc/\d+/fd$,;
}

is(scalar keys %fds, 0, 'No file descriptors leaked');

}

done_testing;
