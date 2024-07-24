#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "token.h"
#include "keywords.h"

KeywordType keyword_type(const char *identifier)
{
    for (size_t i = 0; i < KW_COUNT - 1; i++)
    {
        if (strcmp(keywords[i].text, identifier) == 0)
        {
            return keywords[i].type;
        }
    }

    return KW_UNK;
}

KeywordType keyword_token_ident_type(Token identifier)
{
    if (identifier.type != TOKEN_IDENTIFIER)
    {
        return KW_UNK;
    }

    char text[identifier.length + 1];
    sprintf(text, "%.*s", identifier.length, identifier.start);
    return keyword_type(text);
}
