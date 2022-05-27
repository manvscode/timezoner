# To build for Linux:
# 	make OS=linux
#
# To build for Windows x86:
# 	make OS=windows-x86
#
# To build for Windows x86_64:
# 	make OS=windows-x86_64

ifndef $(OS)
OS=linux
endif

ifndef $(DEBUG)
DEBUG=false
endif

CWD = $(shell pwd)

ifeq ($(DEBUG), true)
CFLAGS = -std=c99 -Wall -O0 -g -I /usr/local/include -I extern/include/xtd-1.0.0/ -I extern/include/collections-1.0.0/
else
CFLAGS = -std=c99 -Wall -D_DEFAULT_SOURCE -O2 -I /usr/local/include -I extern/include/xtd-1.0.0/ -I extern/include/collections-1.0.0/
endif

ifeq ($(OS),linux)
BIN_NAME = timezoner
CC = gcc
HOST=
LDFLAGS = extern/lib/libxtd.a extern/lib/libcollections.a -L /usr/local/lib -L extern/lib/ -L extern/libcollections/lib/
endif

ifeq ($(OS),windows-x86)
BIN_NAME = timezoner-x86.exe
CC=i686-w64-mingw32-gcc
HOST=i686-w64-mingw32
LDFLAGS = extern/lib/libxtd.a extern/lib/libcollections.a -L /usr/local/lib -L extern/lib/ -L extern/libcollections/lib/
endif

ifeq ($(OS),windows-x86_64)
BIN_NAME = timezoner-x64.exe
CC=x86_64-w64-mingw32-gcc
HOST=x86_64-w64-mingw32
CFLAGS += -D_POSIX -mwindows
#LDFLAGS = extern/lib/libxtd.dll.a extern/lib/libcollections.dll.a /usr/x86_64-w64-mingw32/lib/libmsvcrt.a -L/usr/local/lib -Lextern/lib/ -Lextern/libcollections/lib/ -L/usr/x86_64-w64-mingw32/lib/ -lmingw32 -lmsvcrt
LDFLAGS = extern/lib/libxtd.a extern/lib/libcollections.a -L/usr/local/lib -Lextern/lib/ -Lextern/libcollections/lib/ -L/usr/x86_64-w64-mingw32/lib/ -lmingw32 -lmsvcrt
endif


SOURCES = src/main.c


all: extern/libxtd extern/libcollections bin/$(BIN_NAME)

bin/$(BIN_NAME): $(SOURCES:.c=.o)
	@mkdir -p bin
	@echo "Linking: $^"
	@$(CC) $(CFLAGS) -o bin/$(BIN_NAME) $^ $(LDFLAGS)
	@echo "Created $@"

src/%.o: src/%.c
	@echo "Compiling: $<"
	@$(CC) $(CFLAGS) -c $< -o $@

#################################################
# Dependencies                                  #
#################################################
extern/libxtd:
	@mkdir -p extern/libxtd/
	@git clone https://github.com/manvscode/libxtd.git extern/libxtd/
	@cd extern/libxtd && autoreconf -fi && ./configure --libdir=$(CWD)/extern/lib/ --includedir=$(CWD)/extern/include/ --host=$(HOST) && make && make install

extern/libcollections:
	@mkdir -p extern/libcollections/
	@git clone https://github.com/manvscode/libcollections.git extern/libcollections/
	@cd extern/libcollections && autoreconf -fi && ./configure --libdir=$(CWD)/extern/lib/ --includedir=$(CWD)/extern/include/ --host=$(HOST) && make && make install

#################################################
# Cleaning                                      #
#################################################
clean_extern:
	@rm -rf extern

clean:
	@rm -rf src/*.o
	@rm -rf bin

#################################################
# Installing                                    #
#################################################
install:
ifeq ("$(INSTALL_PATH)","")
	$(error INSTALL_PATH is not set.)
endif
	@echo "Installing ${CWD}/bin/${BIN_NAME} to ${INSTALL_PATH}"
	@cp bin/$(BIN_NAME) $(INSTALL_PATH)/$(BIN_NAME)
