all:
	gcc -Wall -gdwarf-2 -g3 -I/usr/include/xcb -c mainx.c
	gcc -Wall -gdwarf-2 -g3 -I/usr/include/xcb -c table.c
	gcc -Wall -gdwarf-2 -g3 -I/usr/include/xcb -c test_table.c
	gcc -Wall -gdwarf-2 -g3 -I/usr/include/xcb -o mainx mainx.o table.o -lxcb-wm
	gcc -Wall -gdwarf-2 -g3 -I/usr/include/xcb -o tt test_table.o table.o
