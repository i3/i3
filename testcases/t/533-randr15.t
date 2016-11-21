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
# TODO: Description of this file.
# Ticket: #999
# Bug still in: 4.13-12-g2ff3d9d
use File::Temp qw(tempfile);
use i3test i3_autostart => 0;

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

EOT

my ($outfh, $outname) = tempfile('i3-randr15reply-XXXXXX', UNLINK => 1);

# Prepare a RRGetMonitors reply, see A.2.4 in
# https://cgit.freedesktop.org/xorg/proto/randrproto/tree/randrproto.txt
my $reply = pack('cxSLLLLx[LLL]',
     1, # reply
     0, # sequence (will be filled in by inject_randr15)
     # 56 = length($reply) + length($monitor1)
     # 32 = minimum X11 reply length
     (56-32) / 4, # length in words
     0, # timestamp TODO
     1, # nmonitors
     0); # noutputs

# Manually intern _NET_CURRENT_DESKTOP as $x->atom will not create atoms if
# they are not yet interned.
my $atom_cookie = $x->intern_atom(0, length("DP3"), "DP3");
my $DP3 = $x->intern_atom_reply($atom_cookie->{sequence})->{atom};

# MONITORINFO is defined in A.1.1 in
# https://cgit.freedesktop.org/xorg/proto/randrproto/tree/randrproto.txt
my $monitor1 = pack('LccSssSSLL',
        $DP3, # name (ATOM)
        1, # primary
        1, # automatic
        0, # ncrtcs
        0, # x
        0, # y
        3840, # width in pixels
        2160, # height in pixels
        520, # width in millimeters
        290); # height in millimeters

print $outfh $reply;
print $outfh $monitor1;

close($outfh);

my $pid = launch_with_config($config, inject_randr15 => $outname);

cmd 'nop';

exit_gracefully($pid);

done_testing;
