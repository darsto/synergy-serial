OBJECTS = main.o common.o synergy_proto.o serial.o
_CFLAGS := -O2 -g -MMD -MP -fno-strict-aliasing -Wall -Wno-format-truncation $(CFLAGS)

$(@shell mkdir -p build &>/dev/null)

.PHONY: clean all build/gcc_ver.h

all: build/synergy-serial

clean:
	rm -f $(OBJECTS:%.o=build/%.o) $(OBJECTS:%.o=build/%.d) build/gcc_ver.h

build:

build/gcc_ver.h: build
	@mkdir -p build &> /dev/null
	$(shell echo "#define BUILD_GCC_VER (\"$$(gcc --version | head -n1)\")\n#define BUILD_CFLAGS (\"$(_CFLAGS)\")" > build/gcc_ver_tmp.h)
	$(shell if ! cmp build/gcc_ver_tmp.h build/gcc_ver.h 2>/dev/null 1>&2; then make clean; fi)
	@cp build/gcc_ver_tmp.h build/gcc_ver.h

build/synergy-serial: build/gcc_ver.h $(OBJECTS:%.o=build/%.o)
	gcc $(_CFLAGS) -o $@ $^

build/%.o: %.c
	gcc $(_CFLAGS) -c -o $@ $<

-include $(OBJECTS:%.o=build/%.d)
