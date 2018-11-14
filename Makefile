TARGET  = libjake.a

SRCDIR  = src
OUTDIR  = lib
OBJDIR  = obj

SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SOURCES))

CC      = gcc
CFLAGS  = -Wall -Wunused-command-line-argument

all: $(OUTDIR)/$(TARGET)

$(OUTDIR)/$(TARGET): $(OBJECTS) | $(OUTDIR)
	ar rcs $@ $^

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -r $(OUTDIR)
	rm -r $(OBJDIR)
