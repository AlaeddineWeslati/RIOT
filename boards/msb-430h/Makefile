MODULE =$(BOARD)_base

DIRS = $(RIOTBOARD)/msb-430-common

all: $(BINDIR)$(MODULE).a
	@for i in $(DIRS) ; do "$(MAKE)" -C $$i || exit 1; done ;

include $(RIOTBASE)/Makefile.base
