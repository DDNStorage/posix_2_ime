libposix2ime.so:
	gcc -O3 -fpic -Wno-nonnull-compare -shared -L/opt/ddn/ime/lib -lim_client -I/opt/ddn/ime/include -o libposix2ime.so posix2ime.c -ldl

all: libposix2ime.so

clean:
	rm libposix2ime.so
