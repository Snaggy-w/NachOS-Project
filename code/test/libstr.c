#include "syscall.h"
#include "libstr.h"

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


int str_cmp(const char *a, const char *b) {
    int i = 0;

    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i])
            return (unsigned char)a[i] - (unsigned char)b[i];
        i++;
    }

    return (unsigned char)a[i] - (unsigned char)b[i];
}
