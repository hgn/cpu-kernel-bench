GNUPLOT_FILES = $(wildcard *.gpi)
PNG_OBJ = $(patsubst %.gpi,%.png,  $(GNUPLOT_FILES))
PDF_OBJ = $(patsubst %.gpi,%.pdf,  $(GNUPLOT_FILES))

all: wss-cache-effects data $(PDF_OBJ)
png: $(PNG_OBJ)


wss-cache-effects: wss-cache-effects.c
	gcc -O0 -g wss-cache-effects.c -o wss-cache-effects

data: wss-cache-effects
	./generator.py

%.eps: %.gpi *.data
	@ echo "compillation of "$<
	@gnuplot $<

%.pdf: %.eps
	@echo "conversion in pdf format"
	@epstopdf --outfile=$*.pdf $<
	@rm $<
	@echo "end"

%.png: %.pdf
	@echo "conversion in png format"
	@convert -density 300 $< $*.png 
	@echo "end"

preview: all
	for i in $$(ls *.pdf); do xpdf -fullscreen $$i ; done

clean:
	@echo "cleaning ..."
	@rm -rf *.eps *.png *.pdf core
	@rm -rf wss-cache-effects
	@rm -rf *.data

distclean: clean
	@echo "distcleaning"
	@rm -rf *.data

