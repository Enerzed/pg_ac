MODULE_big = pg_ac
OBJS = ac.o

EXTENSION = pg_ac
DATA = pg_ac--0.1.sql pg_ac.control
PGFILEDESC = "pg_ac is an PostgreSQL EXTENSION for full text search using Aho-Corasick algorithm"

REGRESS = ac

PG_CONFIG ?= pg_config
PGXS = $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)