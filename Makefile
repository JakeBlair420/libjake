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

ifdef WITH_0DAY
CFLAGS += -DWITH_0DAY=1
endif

export CC
export COMMONCRYPTO = 1

.PHONY: all test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(LIBTOOL) $(LIBTOOL_FLAGS) -o $@ $^

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	$(MKDIR) -p $(OBJDIR)

# this sux, sorry in advance
test: $(TSTBIN)

$(TSTBIN): $(TARGET) $(TSTDIR)/*.c img4lib/libimg4.a img4lib/lzfse/build/bin/liblzfse.a
	$(CC) $(CFLAGS) -I$(SRCDIR) $(LDFLAGS) -o $@ $(TSTDIR)/*.c

img4lib/libimg4.a:
	$(MAKE) -C img4lib libimg4.a

img4lib/lzfse/build/bin/liblzfse.a:
	$(MAKE) -C img4lib/lzfse build/bin/liblzfse.a

clean:
	$(RM) -rf $(TARGET) $(OBJDIR) $(TSTBIN)
	$(MAKE) -C img4lib distclean
