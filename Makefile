all: output/index.html

CURRENT_VERSION := v3.19.0
GITVERSION := $(shell git log -1 --pretty='%H')
GITSHORTVER := $(shell git log -1 --pretty='%h')
GITBRANCH := $(shell git rev-parse --abbrev-ref HEAD)
OUTDIR := output/$(GITVERSION)

OPT := -O1 -flto -s AGGRESSIVE_VARIABLE_ELIMINATION=1

DEBUG ?= 0
ifeq ($(DEBUG), 1)
	OPT := -O0 -g
endif

CFLAGS = $(OPT) -Ideps/lua-5.1.5/src/ -Ideps/zlib-1.2.11/ \
	-s ALLOW_MEMORY_GROWTH=1 -s WASM=1 -s ASSERTIONS=0 \
	-s EXPORTED_FUNCTIONS="['_inject_paste', '_generate_build', '_main', '_profiler_start', '_profiler_stop']" --no-heap-copy

LDFLAGS = -lidbfs.js

OBJECTS = \
	src/main.o \
	deps/zlib-1.2.11/adler32.o \
	deps/zlib-1.2.11/crc32.o \
	deps/zlib-1.2.11/deflate.o \
	deps/zlib-1.2.11/inffast.o \
	deps/zlib-1.2.11/inflate.o \
	deps/zlib-1.2.11/inftrees.o \
	deps/zlib-1.2.11/trees.o \
	deps/zlib-1.2.11/zutil.o \
	deps/lua-5.1.5/src/lapi.o \
	deps/lua-5.1.5/src/lauxlib.o \
	deps/lua-5.1.5/src/lbaselib.o \
	deps/lua-5.1.5/src/lcode.o \
	deps/lua-5.1.5/src/ldblib.o \
	deps/lua-5.1.5/src/ldebug.o \
	deps/lua-5.1.5/src/ldo.o \
	deps/lua-5.1.5/src/ldump.o \
	deps/lua-5.1.5/src/lfunc.o \
	deps/lua-5.1.5/src/lgc.o \
	deps/lua-5.1.5/src/linit.o \
	deps/lua-5.1.5/src/liolib.o \
	deps/lua-5.1.5/src/llex.o \
	deps/lua-5.1.5/src/lmathlib.o \
	deps/lua-5.1.5/src/lmem.o \
	deps/lua-5.1.5/src/loadlib.o \
	deps/lua-5.1.5/src/lobject.o \
	deps/lua-5.1.5/src/lopcodes.o \
	deps/lua-5.1.5/src/loslib.o \
	deps/lua-5.1.5/src/lparser.o \
	deps/lua-5.1.5/src/lstate.o \
	deps/lua-5.1.5/src/lstring.o \
	deps/lua-5.1.5/src/lstrlib.o \
	deps/lua-5.1.5/src/ltable.o \
	deps/lua-5.1.5/src/ltablib.o \
	deps/lua-5.1.5/src/ltm.o \
	deps/lua-5.1.5/src/lundump.o \
	deps/lua-5.1.5/src/lvm.o \
	deps/lua-5.1.5/src/lzio.o \
	deps/lua-5.1.5/src/print.o \
	deps/LuaBitOp-1.0.2/bit.o \
	src/c_hook.o

.PHONY: copy_assets

clean:
	-rm -rf $(OBJECTS) output/*

copy_assets:
	mkdir -p $(OUTDIR)
	cd pob && python2 ../tool/assets.py ../$(OUTDIR) ../$(OUTDIR)/assets.js && cd ..
	cp src/style.css $(OUTDIR)
	cp src/draw.js src/overlay.js $(OUTDIR)
	cp src/share.html src/share-link.html src/versions.html output
	cp -r static $(OUTDIR)

output/pob-$(GITVERSION).html: $(OBJECTS)
	cd pob && ./build.sh && cd ..
	mkdir -p output
	emcc --minify 0 --shell-file src/shell.html --js-library src/renderer.js $(CFLAGS) $(LDFLAGS) $^ -o $@ --preload-file pob/build/PathOfBuilding@/

output/index.html: output/pob-$(GITVERSION).html copy_assets
	sed -i "s/GITVERSION/$(GITVERSION)/g" $<
	sed -i "s/GITSHORTVER/$(GITSHORTVER)/g" $<
	sed -i "s/GITBRANCH/$(GITBRANCH)/g" $<
	sed -i "s/@CURRENT_VERSION@/$(CURRENT_VERSION)/g" $<
	sed -i "s/@CURRENT_VERSION@/$(CURRENT_VERSION)/g" output/share.html
	mv $< output/index.html

%.o: %.c
	emcc $(CFLAGS) -c $< -o $@
