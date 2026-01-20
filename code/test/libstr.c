#include "syscall.h"
#include "libstr.h"

/* parse a command or string based on spaces and return the element with the specified index. much like bash cut -d ' ' command */
char *parse_str_cmd(int index, char *in_buff, char *out_buff, int MAX_LEN) {
    int pos = 0;
    int token = 0;

    // skip leading spaces if any
    while (pos < MAX_LEN && in_buff[pos] == ' ')
        pos++;

    while (pos < MAX_LEN && in_buff[pos] != '\0') {

        // if this is the token we want we copy it
        if (token == index) {
            int i = 0;
            while (pos < MAX_LEN &&
                   in_buff[pos] != '\0' &&
                   in_buff[pos] != ' ') {
                out_buff[i++] = in_buff[pos++];
            }
            out_buff[i] = '\0';
            return out_buff;
        }

        // skip this token
        while (pos < MAX_LEN &&
               in_buff[pos] != '\0' &&
               in_buff[pos] != ' ')
            pos++;

        // skip spaces between tokens
        while (pos < MAX_LEN && in_buff[pos] == ' ')
            pos++;

        token++;
    }

    // token not found
    return (char *)0;
}

// strcmp equivalent
int str_cmp(const char *a, const char *b) {
    int i = 0;

    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i])
            return (unsigned char)a[i] - (unsigned char)b[i];
        i++;
    }

    return (unsigned char)a[i] - (unsigned char)b[i];
}

// strcpy posix equivalent
void strcpy(char *dst, const char *src, int cap) {
    int i = 0;
    if (cap <= 0) return;
    while (src[i] && i < cap - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}
// atoi posix equivalent
int atoi(const char *s) {
    int i = 0, sign = 1, v = 0;
    while (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r') i++;
    if (s[i] == '-') { sign = -1; i++; }
    while (s[i] >= '0' && s[i] <= '9') {
        v = v * 10 + (s[i] - '0');
        i++;
    }
    return v * sign;
}

// strlen posix equivalent
int strlen(const char *s) {
    int n = 0;
    while (s && s[n]) n++;
    return n;
}

// strip the end of string from control characters
void rstrip_ws(char *s) {
    int n = 0;
    while (s[n] != 0) n++;
    while (n > 0) {
        char c = s[n - 1];
        if (c == '\n' || c == '\r' || c == ' ' || c == '\t')
            s[--n] = 0;
        else
            break;
    }
}


