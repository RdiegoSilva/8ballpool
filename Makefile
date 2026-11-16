EE_BIN = pool8.elf
EE_OBJS = main.o music.o
EE_LIBS = -lgskit -ldmakit -lpad -laudsrv -lfileXio -lpatches -lm -lc -lkernel
EE_INCS += -I$(PS2SDK)/ports/include
EE_LDFLAGS += -L$(PS2SDK)/ports/lib
EE_INCS += -I$(PS2DEV)/gsKit/include
EE_LDFLAGS += -L$(PS2DEV)/gsKit/lib

# modulos do IOP embutidos no .elf (USB, fileXio e audio do player de musicas)
IRX_FILES = iomanX.irx fileXio.irx usbd.irx audsrv.irx
ifneq ($(wildcard $(PS2SDK)/iop/irx/bdm.irx),)
IRX_FILES += bdm.irx bdmfs_fatfs.irx usbmass_bd.irx      # PS2SDK novo (BDM)
EE_CFLAGS += -DUSE_BDM
else
IRX_FILES += usbhdfsd.irx                                # PS2SDK antigo
endif
EE_OBJS += $(IRX_FILES:.irx=_irx.o)

all: $(EE_BIN)

clean:
	rm -f $(EE_BIN) $(EE_OBJS) $(IRX_FILES:.irx=_irx.c)

%_irx.c:
	$(PS2SDK)/bin/bin2c $(PS2SDK)/iop/irx/$*.irx $@ $*_irx

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
