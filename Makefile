TARGET  = libjake.a

SRCDIR  = src
OUTDIR  = lib
OBJDIR  = obj
TSTDIR  = test

SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SOURCES))

TSTBIN  = $(TSTDIR)/test

CC      ?= xcrun -sdk iphoneos clang -arch arm64 -arch armv7
CFLAGS  ?= -Wall -O3 -Wunused-command-line-argument -Iimg4lib/libvfs
LDFLAGS ?= -L. -ljake -Limg4lib -limg4 -Limg4lib/lzfse/build/bin -llzfse -framework Security -framework CoreFoundation
LIBTOOL = libtool
LIBTOOL_FLAGS = -static
MKDIR   = mkdir
RM      = rm

export OVERRIDE_CC = $(CC)
export COMMONCRYPTO = 1

.PHONY: all test

all: $(TARGET)

$(TARGET): $(OBJECTS) img4lib/libimg4.a
	$(LIBTOOL) $(LIBTOOL_FLAGS) -o $@ $^

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	$(MKDIR) -p $(OBJDIR)

# this sux, sorry in advance
test: $(TSTBIN)

$(TSTBIN): $(TARGET) $(TSTDIR)/*.c
	$(CC) $(CFLAGS) -I$(SRCDIR) $(LDFLAGS) -o $@ $(TSTDIR)/*.c

img4lib/libimg4.a: img4lib/lzfse/build/bin/liblzfse.a
	$(MAKE) $(AM_MAKEFLAGS) -C img4lib libimg4.a

img4lib/lzfse/build/bin/liblzfse.a:
	$(MAKE) $(AM_MAKEFLAGS) -C img4lib/lzfse build/bin/liblzfse.a

clean:
	$(RM) -rf $(TARGET) $(OBJDIR) $(TSTBIN)
	$(MAKE) $(AM_MAKEFLAGS) -C img4lib distclean
	$(MAKE) $(AM_MAKEFLAGS) -C img4lib/lzfse clean
