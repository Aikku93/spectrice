.phony: clean

#----------------------------#
# Directories
#----------------------------#

OBJDIR := build
RELDIR := release

INCDIR := include
SRCDIR := . fourier libspectrice tools

#----------------------------#
# Cross-compilation, compile flags
#----------------------------#

# Alternatively, try "-march=native" for ARCHFLAGS
ARCHCROSS :=
ARCHFLAGS := -msse -msse2 -mavx -mavx2 -mfma

CCFLAGS := $(ARCHFLAGS) -fno-math-errno -O2 -Wall -Wextra $(foreach dir, $(INCDIR), -I$(dir))
LDFLAGS := -static

#----------------------------#
# Tools
#----------------------------#

CC := $(ARCHCROSS)gcc
LD := $(ARCHCROSS)gcc

#----------------------------#
# Files
#----------------------------#

SRC := $(foreach dir, $(SRCDIR), $(wildcard $(dir)/*.c))
OBJ := $(addprefix $(OBJDIR)/, $(SRC:.c=.o))
DEP := $(addsuffix .d, $(OBJ))
EXE := spectrice

#----------------------------#
# General rules
#----------------------------#

$(OBJDIR)/%.o : %.c
	@echo $(notdir $<)
	@mkdir -p $(dir $@)
	@$(CC) $(CCFLAGS) -c -MD -MP -MF $(OBJDIR)/$<.d -o $@ $<

#----------------------------#
# make all
#----------------------------#

all : $(EXE)

$(EXE) : $(OBJ) | $(RELDIR)
	$(LD) -s -o $(RELDIR)/$@ $^ $(LDFLAGS)

$(OBJ) : $(SRC) | $(OBJDIR)

$(OBJDIR) $(RELDIR) :; mkdir -p $@

#----------------------------#
# make clean
#----------------------------#

clean :; rm -rf $(OBJDIR) $(RELDIR)

#----------------------------#
# Dependencies
#----------------------------#

-include $(DEP)

#----------------------------#
