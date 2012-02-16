# To pass additional parameters for asciidoc
ASCIIDOC=asciidoc

all: hacking-howto.html debugging.html debugging-release-version.html userguide.html ipc.html multi-monitor.html wsbar.html refcard.pdf testsuite.html i3bar-protocol.html

hacking-howto.html: hacking-howto
	$(ASCIIDOC) -a toc -n $<

i3bar-protocol.html: i3bar-protocol
	$(ASCIIDOC) -a toc -n $<

debugging.html: debugging
	$(ASCIIDOC) -n $<

debugging-release-version.html: debugging-release-version
	$(ASCIIDOC) -n $<

userguide.html: userguide
	$(ASCIIDOC) -a toc -n $<

testsuite.html: testsuite
	$(ASCIIDOC) -a toc -n $<

ipc.html: ipc
	$(ASCIIDOC) -a toc -n $<

multi-monitor.html: multi-monitor
	$(ASCIIDOC) -a toc -n $<

wsbar.html: wsbar
	$(ASCIIDOC) -a toc -n $<

refcard.pdf: refcard.tex
	pdflatex refcard.tex && pdflatex refcard.tex

clean:
	find . -regex ".*\.\(aux\|out\|log\|toc\|bm\|pdf\|dvi\|log\|html\)" -exec rm '{}' \;
