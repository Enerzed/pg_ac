MODULE_big = aho_corasick
OBJS = aho_corasick.o

EXTENSION = aho_corasick
DATA = aho_corasick--0.1.sql aho_corasick.control
PGFILEDESC = "aho_corasick is an PostgreSQL EXTENSION for full text search using Aho-Corasick algorithm"

REGRESS = aho_corasick

PG_CONFIG ?= pg_config
PGXS = $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)