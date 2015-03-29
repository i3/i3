package StatusLine;
use strict; use warnings;

# enable autoflush on STDOUT.
# this is essential, because we print our statuslines without a newline
$| = 1;

use Exporter 'import';
our @EXPORT = qw/status_init status status_completed/;

my $ansi_clear_line = "\033[2K";
my $ansi_save_cursor = "\0337";
my $ansi_restore_cursor = "\0338";
my %ansi_line_upwards;

my $tests_total;

sub noninteractive {
    # CONTINUOUS_INTEGRATION gets set when running under Travis, see
    # http://docs.travis-ci.com/user/ci-environment/ and
    # https://github.com/travis-ci/travis-ci/issues/1337
    return (! -t STDOUT) || (
        defined($ENV{CONTINUOUS_INTEGRATION}) &&
        $ENV{CONTINUOUS_INTEGRATION} eq 'true');
}

# setup %ansi_line_upwards to map all working displays to the
# specific movement commands and initialize all status lines
sub status_init {
    my %args = @_;
    my $displays = $args{displays};
    $tests_total = $args{tests};

    return if noninteractive();

    for my $n (1 .. @$displays) {
        # since we are moving upwards, get $display in reverse order
        my $display = $displays->[-$n];

        $ansi_line_upwards{$display} = "\033[$n\101";

        # print an empty line for this status line
        print "\n";
    }

    status_completed(0);
}

# generates the status text, prints it in the appropiate line
# and returns it, so it can be used in conjuction with C<Log()>
sub status {
    my ($display, $msg) = @_;
    my $status = "[$display] $msg";

    return $status if noninteractive();

    print
        $ansi_save_cursor,
        $ansi_line_upwards{$display},
        $ansi_clear_line,
        $status,
        $ansi_restore_cursor;

    return $status;
}

sub status_completed {
    my $num = shift;

    return if noninteractive();

    print
        $ansi_save_cursor,
        $ansi_clear_line,
        "completed $num of $tests_total tests",
        $ansi_restore_cursor;
}


__PACKAGE__ __END__
