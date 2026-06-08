#!perl
# vim:ts=4:sw=4:expandtab
#
# Verifies the GET_BINDING_SYMS IPC command
# (Returns all keybindings grouped by mode)

use Test::Deep;
use i3test i3_config => <<'EOT';
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

# Default mode bindings
bindsym Mod4+h split h
bindsym Mod4+v split v
bindsym Mod4+Shift+p focus child

# Resize mode bindings
mode "resize" {
    bindsym 1 resize set width 66 ppt; mode default
    bindsym 2 resize set height 66 ppt
}
EOT

my $i3 = i3(get_socket_path());
$i3->connect->recv;

my $reply = $i3->message(13, "")->recv;

isa_ok($reply, 'HASH', 'Reply is a hash');

cmp_deeply(
  $reply,
  {
    default => [
      { hotkey => 'Shift+Mod4+p', command => 'focus child' },
      { hotkey => 'Mod4+h', command => 'split h' },
      { hotkey => 'Mod4+v', command => 'split v' },
    ],
    resize => [
      { hotkey => "1", "command" => "resize set width 66 ppt; mode default" },
      { hotkey => "2", "command" => "resize set height 66 ppt" }
    ]
  }
);

done_testing;
