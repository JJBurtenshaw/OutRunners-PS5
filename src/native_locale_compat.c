#include <stddef.h>

/*
 * Temporary native PS5 locale compatibility.
 * Assumes UTF-8 for the SDL2 diagnostic application.
 */

char *nl_langinfo(int item)
{
    static char utf8[] = "UTF-8";
    static char empty[] = "";

    /* FreeBSD CODESET = 0 */
    return item == 0 ? utf8 : empty;
}


