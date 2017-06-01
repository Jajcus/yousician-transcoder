

all: yousician_transcode.so

bdist: all
	version="$$(git describe --dirty)" && \
	mkdir yousician_transcode-$${version}-bin && \
	cp -a LICENSE README.md YousicianT yousician_transcode.so yousician_transcode-$${version}-bin && \
	tar czvf yousician_transcode-$${version}-bin.tar.gz yousician_transcode-$${version}-bin ; \
	rm -rf yousician_transcode-$${version}-bin

yousician_transcode.o: yousician_transcode.c
	gcc -fPIC -DPIC -Wall -c -g -ggdb -o $@ $<

yousician_transcode.so: yousician_transcode.o
	ld -shared -o $@ $< -ldl -lc
