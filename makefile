CC = gcc
CFLAGS = -std=c99 -g
CXX = clang++
CXXFLAGS = -std=c++11 -g

PREFIX = ./build
PART2_3_SRC = ./part2_3
EXE = socks4d
OBJS = socks4d.o server_arch.o socket.o io_wrapper.o

EXE_PATH = $(addprefix $(PREFIX)/, $(EXE))
OBJS_PATH = $(addprefix $(PREFIX)/, $(OBJS))

MAKE = make

# platform issue
UNAME = $(shell uname)
ifeq ($(UNAME), FreeBSD)
    MAKE = gmake
endif

all: $(EXE_PATH)

clean: 
	rm -f $(EXE_PATH) $(OBJS_PATH)

$(EXE_PATH): $(OBJS_PATH)
	$(CXX) -o $@ $(CXXFLAGS) $^

$(OBJS_PATH): $(PREFIX)/%.o: $(PART2_3_SRC)/%.cpp
	$(CXX) -o $@ $(CXXFLAGS) -c $<

.PHONY: all clean
