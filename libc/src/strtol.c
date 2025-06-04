#include <ctype.h>
#include <stdlib.h>

long strtol(const char *nptr, char **endptr, int base)
{
    const char *s = nptr;
    long sign = 1;
    long val = 0;

    while (*s == ' ' || *s == '\t')
        s++;

    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') { s++; }

    if (base == 0) {
        if (s[0] == '0') {
            if (s[1] == 'x' || s[1] == 'X') { base = 16; s += 2; }
            else base = 8;
        } else {
            base = 10;
        }
    }

    if (base == 16 && s[0]=='0' && (s[1]=='x' || s[1]=='X'))
        s += 2;

    while (*s) {
        int digit;
        if (isdigit(*s)) digit = *s - '0';
        else if (*s >= 'a' && *s <= 'f') digit = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F') digit = *s - 'A' + 10;
        else break;
        if (digit >= base) break;
        val = val * base + digit;
        s++;
    }

    if (endptr)
        *endptr = (char*)s;
    return sign * val;
}

