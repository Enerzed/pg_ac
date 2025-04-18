/*
 * Parser.c
 */


 #include "parser.h"


PG_FUNCTION_INFO_V1(parser_start);
Datum parser_start(PG_FUNCTION_ARGS)
{
    parser_state *pst = (parser_state*)palloc0(sizeof(parser_state));

    pst->buffer = (char *)PG_GETARG_POINTER(0);
    pst->len = PG_GETARG_INT32(1);
    pst->pos = 0;

    PG_RETURN_POINTER(pst);
}

PG_FUNCTION_INFO_V1(get_lex);
Datum get_lex(PG_FUNCTION_ARGS)
{
    parser_state *pst = (parser_state*)PG_GETARG_POINTER(0);
    char **t = (char **)PG_GETARG_POINTER(1);
    int *tlen = (int *)PG_GETARG_POINTER(2);
    int startpos = pst->pos;
    int type;

    *t = pst->buffer + pst->pos;

    if (pst->pos < pst->len &&
        (pst->buffer)[pst->pos] == ' ')
    {
    /* blank type */
    type = 12;
    /* go to the next non-space character */
    while (pst->pos < pst->len &&
            (pst->buffer)[pst->pos] == ' ')
        (pst->pos)++;
    }
    else
    {
    /* word type */
    type = 3;
    /* go to the next space character */
    while (pst->pos < pst->len &&
            (pst->buffer)[pst->pos] != ' ')
        (pst->pos)++;
    }

    *tlen = pst->pos - startpos;

    /* we are finished if (*tlen == 0) */
    if (*tlen == 0)
    type = 0;

    PG_RETURN_INT32(type);
}

PG_FUNCTION_INFO_V1(parser_end);
Datum parser_end(PG_FUNCTION_ARGS)
{
    parser_state *pst = (parser_state*)PG_GETARG_POINTER(0);

    pfree(pst);
    PG_RETURN_VOID();
}

PG_FUNCTION_INFO_V1(lex_type);
Datum lex_type(PG_FUNCTION_ARGS)
{
    /*
    * Remarks: - we have to return the blanks for headline reason - we use
    * the same lexids like Teodor in the default word parser; in this way we
    * can reuse the headline function of the default word parser.
    */
    LexDescription *descr = (LexDescription *)palloc(sizeof(LexDescription) * (2 + 1));

    /* there are only two types in this parser */
    descr[0].lexid = 3;
    descr[0].alias = pstrdup("word");
    descr[0].descr = pstrdup("Word");
    descr[1].lexid = 12;
    descr[1].alias = pstrdup("blank");
    descr[1].descr = pstrdup("Space symbols");
    descr[2].lexid = 0;

    PG_RETURN_POINTER(descr);
}