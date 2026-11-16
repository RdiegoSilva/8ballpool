EE_BIN = pool8.elf
EE_OBJS = main.o
EE_LIBS = -lgskit -ldmakit -lpad -lm -lc -lkernel
EE_INCS += -I$(PS2SDK)/ports/include
EE_LDFLAGS += -L$(PS2SDK)/ports/lib

all: $(EE_BIN)

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
