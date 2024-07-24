#ifndef KEYWORDS_H
#define KEYWORDS_H

typedef enum KeywordType
{
    KW_UNK, // unknown keyword

    KW_USE, // use
    KW_DEF, // def
    KW_VAL, // val
    KW_VAR, // var
    KW_FUN, // fun
    KW_STR, // str
    KW_UNI, // uni
    KW_IF,  // if
    KW_OR,  // or
    KW_FOR, // for
    KW_BRK, // brk
    KW_CNT, // cnt
    KW_RET, // ret

    KW_COUNT
} KeywordType;

typedef struct keyword
{
    KeywordType type;
    const char *text;
} keyword;

static const keyword keywords[] = {
    {KW_USE, "use"},
    {KW_DEF, "def"},
    {KW_VAL, "val"},
    {KW_VAR, "var"},
    {KW_FUN, "fun"},
    {KW_STR, "str"},
    {KW_UNI, "uni"},
    {KW_IF, "if"},
    {KW_OR, "or"},
    {KW_FOR, "for"},
    {KW_BRK, "brk"},
    {KW_CNT, "cnt"},
    {KW_RET, "ret"},
};

KeywordType keyword_type(const char *text);
KeywordType keyword_token_ident_type(Token identifier);

#endif // KEYWORDS_H
