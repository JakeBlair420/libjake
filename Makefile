TARGET  = libjake.a

SRCDIR  = src
OUTDIR  = lib
OBJDIR  = obj

SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SOURCES))

ifeq ($(PLATFORM),ios)
	CC = xcrun -sdk iphoneos gcc -arch arm64 -arch armv7s
else
	CC = gcc
endif

INC=-I./img4lib/libvfs/
CFLAGS  = -Wall -Wunused-command-line-argument
LIBTOOL = libtool
MKDIR   = mkdir
RM      = rm

all: $(OUTDIR)/$(TARGET)

$(OUTDIR)/$(TARGET): $(OBJECTS) | $(OUTDIR)
	$(LIBTOOL) -o $@ $^ 

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(OUTDIR):
	$(MKDIR) -p $(OUTDIR)

$(OBJDIR):
	$(MKDIR) -p $(OBJDIR)

clean:
	$(RM) -rf $(OUTDIR)
	$(RM) -rf $(OBJDIR)
