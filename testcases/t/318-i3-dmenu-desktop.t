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
# Verifies that i3-dmenu-desktop correctly parses Exec= lines in .desktop files
# and sends the command to i3 for execution.
# Ticket: #5152, #5156
# Bug still in: 4.21-17-g389d555d
use i3test;
use i3test::Util qw(slurp);
use File::Temp qw(tempfile tempdir);
use POSIX qw(mkfifo);
use JSON::XS qw(decode_json);

my $desktopdir = tempdir(CLEANUP => 1);

$ENV{XDG_DATA_DIRS} = "$desktopdir";

mkdir("$desktopdir/applications");

# Create an i3-msg executable that dumps command line flags to a FIFO
my $tmpdir = tempdir(CLEANUP => 1);

$ENV{PATH} = "$tmpdir:" . $ENV{PATH};

mkfifo("$tmpdir/fifo", 0600) or BAIL_OUT "Could not create FIFO: $!";

open(my $i3msg_dump, '>', "$tmpdir/i3-msg");
say $i3msg_dump <<EOT;
#!/usr/bin/env perl
use strict;
use warnings;
use JSON::XS qw(encode_json);
open(my \$f, '>', "$tmpdir/fifo");
say \$f encode_json(\\\@ARGV);
close(\$f);
EOT
close($i3msg_dump);
chmod 0755, "$tmpdir/i3-msg";

my $testcnt = 0;
sub verify_exec {
    my ($execline, $want_arg) = @_;

    $testcnt++;

    open(my $desktop, '>', "$desktopdir/applications/desktop$testcnt.desktop");
    say $desktop <<EOT;
[Desktop Entry]
Name=i3-testsuite-$testcnt
Type=Application
Exec=$execline
EOT
    close($desktop);

    # complete-run.pl arranges for $PATH to be set up such that the
    # i3-dmenu-desktop version we execute is the one from the build directory.
    my $exit = system("i3-dmenu-desktop --dmenu 'echo i3-testsuite-$testcnt' &");
    if ($exit != 0) {
	die "failed to run i3-dmenu-desktop";
    }

    chomp($want_arg);  # trim trailing newline
    my $got_args = decode_json(slurp("$tmpdir/fifo"));
    is_deeply($got_args, [ $want_arg ], 'i3-dmenu-desktop executed command as expected');
}

# recommended number of backslashes by the spec, not ambiguous
my $exec_1 = <<'EOS';
echo "hello \\$PWD \\"and\\" more"
EOS
my $want_1 = <<'EOS';
exec  "echo \"hello \\$PWD \\\"and\\\" more\""
EOS
verify_exec($exec_1, $want_1);

# permitted, but ambiguous
my $exec_2 = <<'EOS';
echo "hello \$PWD \"and\" more"
EOS
my $want_2 = <<'EOS';
exec  "echo \"hello \\$PWD \\\"and\\\" more\""
EOS
verify_exec($exec_2, $want_2);

# electrum
my $exec_3 = <<'EOS';
sh -c "PATH=\"\\$HOME/.local/bin:\\$PATH\"; electrum %u"
EOS
my $want_3 = <<'EOS';
exec  "sh -c \"PATH=\\\"\\$HOME/.local/bin:\\$PATH\\\"; electrum \""
EOS
verify_exec($exec_3, $want_3);

# complicated emacsclient command
my $exec_4 = <<'EOS';
sh -c "if [ -n \\"\\$*\\" ]; then exec emacsclient --alternate-editor= --display=\\"\\$DISPLAY\\" \\"\\$@\\"; else exec emacsclient --alternate-editor= --create-frame; fi" placeholder %F
EOS
my $want_4 = <<'EOS';
exec  "sh -c \"if [ -n \\\"\\$*\\\" ]; then exec emacsclient --alternate-editor= --display=\\\"\\$DISPLAY\\\" \\\"\\$@\\\"; else exec emacsclient --alternate-editor= --create-frame; fi\" placeholder "
EOS
verify_exec($exec_4, $want_4);

# permitted, but unusual to quote the first arg
my $exec_5 = <<'EOS';
"electrum" arg
EOS
my $want_5 = <<'EOS';
exec  "\"electrum\" arg"
EOS
verify_exec($exec_5, $want_5);

done_testing;
