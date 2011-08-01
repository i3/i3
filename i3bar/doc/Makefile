all: i3bar.1

i3bar.1: i3bar.man
	echo "A2X i3bar"
	a2x --no-xmllint -f manpage i3bar.man
clean:
	rm -f i3bar.xml i3bar.1 i3bar.html

distclean: clean
