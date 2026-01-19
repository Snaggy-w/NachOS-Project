#ifndef LIBSTR_H
#define LIBSTR_H

extern char *parse_str_cmd(int index, char *in_buff, char *out_buff, int MAX_LEN);
extern int str_cmp(const char *a, const char *b);
extern int atoi(const char *s);
extern void strcpy(char *dst, const char *src, int cap);
extern int strlen(const char *s);
#endif
