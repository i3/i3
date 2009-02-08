all:
	gcc -Wall -gdwarf-2 -g3 -I/usr/include/xcb -o mainx mainx.c -lxcb-wm
