.SUFFIXES: .1 .ms .txt .pdf

all: source.txt xvi.txt source.pdf xvi.pdf

.ms.txt:
	tbl $< | nroff -ms -Tascii -c | col -b > $@

.ms.pdf:
	tbl $< | troff -ms -Tps | grops | ps2pdf - - > $@

.1.txt:
	nroff -man -Tascii -c $< | col -b > $@

.1.pdf:
	troff -man -Tps -c $< | grops | ps2pdf - - > $@

clean:
	rm -f *.txt *.pdf
