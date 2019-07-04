TARGET  = libjake.a

SRCDIR  = src
OUTDIR  = lib
OBJDIR  = obj
TSTDIR  = test

SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SOURCES)) 

TSTBIN  = $(TSTDIR)/test 

ifeq ($(PLATFORM),ios)
	CC = xcrun -sdk iphoneos gcc -arch arm64 -arch armv7s
else
	CC = gcc
endif

INC     = -I./img4lib/libvfs/
LIB     = ./img4lib/lzss.o ./img4lib/libvfs/*.o ./img4lib/libDER/*.o ./img4lib/lzfse/build/bin/liblzfse.a
CFLAGS  = -Wall -Wunused-command-line-argument
LIBTOOL = libtool
MKDIR   = mkdir
RM      = rm

.PHONY: all test

all: $(OUTDIR)/$(TARGET)

$(OUTDIR)/$(TARGET): $(OBJECTS) | $(OUTDIR)
	$(LIBTOOL) -o $@ $^ 

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(OUTDIR):
	$(MKDIR) -p $(OUTDIR)

$(OBJDIR):
	$(MKDIR) -p $(OBJDIR)

test:
	xcrun gcc -o $(TSTBIN) $(TSTDIR)/*.c -I$(SRCDIR) $(INC) $(OUTDIR)/$(TARGET) $(LIB) -lssl -lcrypto 

clean:
	$(RM) -rf $(OUTDIR)
	$(RM) -rf $(OBJDIR)

	$(RM) -f $(TSTBIN)
