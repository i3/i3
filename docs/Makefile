
all: hacking-howto.html debugging.html userguide.html ipc.html

hacking-howto.html: hacking-howto
	asciidoc -a toc -n $<

debugging.html: debugging
	asciidoc -n $<

userguide.html: userguide
	asciidoc -a toc -n $<

ipc.html: ipc
	asciidoc -a toc -n $<

clean:
	rm -f */*.{aux,log,toc,bm,pdf,dvi}
	rm -f *.log *.html
