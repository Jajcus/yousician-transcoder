
.PHONY: all bdist clean

all: yousician_transcode.so

bdist: all
	version="$$(git describe --dirty)" && \
	mkdir yousician-transcoder-$${version}-bin && \
	cp -a LICENSE README.md YousicianT yousician_transcode.so yousician-transcoder-$${version}-bin && \
	tar czvf yousician-transcoder-$${version}-bin.tar.gz yousician-transcoder-$${version}-bin ; \
	rm -rf yousician-transcoder-$${version}-bin

clean:
	-rm -f *.so *.o

yousician_transcode.o: yousician_transcode.c
	gcc -fPIC -DPIC -Wall -c -g -ggdb -o $@ $<

yousician_transcode.so: yousician_transcode.o
	ld -shared -o $@ $< -ldl -lc
