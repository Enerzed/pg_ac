/*
* Parser.h
*/


#pragma once


#include "postgres.h"
#include "fmgr.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>


typedef struct 
{
    char* buffer;
    int len;
    int pos;
} parser_state; 


typedef struct
{
    int lexid;
    char *alias;
    char *descr;
} lex_description;


Datum parser_start(PG_FUNCTION_ARGS);
Datum get_lex(PG_FUNCTION_ARGS);
Datum parser_end(PG_FUNCTION_ARGS);
Datum lex_type(PG_FUNCTION_ARGS);