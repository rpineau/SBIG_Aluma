# Makefile for libSBIG_Aluma

CC = gcc
CFLAGS = -fpic -Wall -Wextra -O2 -g -DSB_LINUX_BUILD -I. -I./../../
CPPFLAGS = -fpic -Wall -Wextra -O2 -g -DSB_LINUX_BUILD -I. -I./../../
LDFLAGS = -shared -lstdc++ -L/usr/local/lib -lftd2xx -lftd3xx
RM = rm -f
STRIP = strip
TARGET_LIB = libSBIG_Aluma.so

SRCS = main.cpp SBIG_Aluma.cpp x2camera.cpp
OBJS = $(SRCS:.cpp=.o)

.PHONY: all
all: ${TARGET_LIB}

$(TARGET_LIB): $(OBJS)
	$(CC) ${LDFLAGS} -o $@ $^ static_libs/`arch`/libdlapi.a static_libs/`arch`/libcfitsio.a static_libs/`arch`/libtinyxml2.a
	$(STRIP) $@ >/dev/null 2>&1  || true

$(SRCS:.cpp=.d):%.d:%.cpp
	$(CC) $(CFLAGS) $(CPPFLAGS) -MM $< >$@

.PHONY: clean
clean:
	${RM} ${TARGET_LIB} ${OBJS} *.d
