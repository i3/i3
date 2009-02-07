all:
	gcc -Wall -g -I/usr/include/xcb -o mainx mainx.c -lxcb-wm
