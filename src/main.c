#include "context.h"

#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    int verbosity = VERBOSITY_LOW;
    for (int i = 1; i < argc; i++)
    {
        // set verbosity based on -v argument where verbosity is set from 0-3 based
        //   on the number of 'v's in the flag
        // NOTE: using just `-v` silences any non-error output
        if (argv[i][0] == '-' && argv[i][1] == 'v')
        {
            verbosity = VERBOSITY_NONE;
            for (int j = 2; argv[i][j] == 'v'; j++)
            {
                if (verbosity >= VERBOSITY_HIGH)
                {
                    break;
                }

                verbosity++;
            }
        }
    }

    switch (verbosity)
    {
    case VERBOSITY_NONE:
        break;
    case VERBOSITY_LOW:
        printf("verbosity set to low");
        break;
    case VERBOSITY_MEDIUM:
        printf("verbosity set to medium");
        break;
    case VERBOSITY_HIGH:
        printf("verbosity set to high");
        break;
    }

    Context *context = context_new();

    context_free(context);
    context = NULL;

    return 0;
}
