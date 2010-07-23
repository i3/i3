
all: hacking-howto.html debugging.html userguide.html ipc.html multi-monitor.html wsbar.html tree-migrating.html refcard.pdf

hacking-howto.html: hacking-howto
	asciidoc -a toc -n $<

debugging.html: debugging
	asciidoc -n $<

userguide.html: userguide
	asciidoc -a toc -n $<

tree-migrating.html: tree-migrating
	asciidoc -a toc -n $<


ipc.html: ipc
	asciidoc -a toc -n $<

multi-monitor.html: multi-monitor
	asciidoc -a toc -n $<

wsbar.html: wsbar
	asciidoc -a toc -n $<

refcard.pdf: refcard.tex
	pdflatex refcard.tex && pdflatex refcard.tex

clean:
	rm -f */*.{aux,log,toc,bm,pdf,dvi}
	rm -f *.log *.html
