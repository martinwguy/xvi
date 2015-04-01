.SUFFIXES: .1 .ms .lst .ps

all: source.lst xvi.lst source.ps xvi.ps

.ms.lst:
	tbl $< | nroff -ms -Tascii -c > $@

.ms.ps:
	tbl $< | troff -ms -Tps | grops > $@

.1.lst:
	nroff -man -Tascii -c $< > $@

.1.ps:
	troff -man -Tps -c $< | grops > $@

clean:
	rm -f *.lst *.ps
