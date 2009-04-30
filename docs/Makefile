
all: hacking-howto.html debugging.html

hacking-howto.html: hacking-howto
	asciidoc -a toc -n $<

debugging.html: debugging
	asciidoc -n $<


clean:
	rm -f */*.{aux,log,toc,bm,pdf,dvi}
	rm -f *.log *.html
