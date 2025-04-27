SHELL = /usr/bin/env bash

all:
	pdflatex doc.tex
	bibtex doc
	pdflatex doc.tex
	pdflatex doc.tex
	

clean:
	rm -f doc.{aux,dvi,log,pdf,ps,out,bbl,blg,fdb_latexmk,fls,synctex.gz,toc}
