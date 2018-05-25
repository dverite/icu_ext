EXTENSION  = icu_ext
EXTVERSION = 1.0

PG_CONFIG = pg_config

DATA = $(wildcard sql/*.sql)

MODULE_big = icu_ext
OBJS      = icu_ext.o icu_break.o icu_num.o icu_spoof.o

all:


PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
