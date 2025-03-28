EXTENSION = AhoCorasick
MODULES = AhoCorasick
DATA = AhoCorasick--0.1.sql AhoCorasick.control
REGRESS = AhoCorasick

LDFLAGS =- lrt

PG_CONFIG ?= pg_config
PGXS = $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)