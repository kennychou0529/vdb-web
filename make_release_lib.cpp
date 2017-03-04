#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

char *read_all(char *filename)
{
    FILE *f = fopen(filename, "rb");
    assert(f);
    fseek(f, 0, SEEK_END); int n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *s = (char*)malloc(n+1);
    fread(s, n, 1, f);
    fclose(f);
    s[n] = 0;
    return s;
}

char **read_lines(char *filename, int *out_n)
{
    char *p = read_all(filename);
    char **lines = (char**)calloc(1024*1024, sizeof(char*));
    int count = 0;
    while (*p)
    {
        int n = 0;
        while (p[n] != '\n')
            n++;

        lines[count] = (char*)calloc(n+1, 1);
        if (p[n-1] == '\r') memcpy(lines[count], p, n-1);
        else                memcpy(lines[count], p, n);
        count += 1;
        p += n + 1;
    }
    *out_n = count;
    return lines;
}

int is_include(char *ptr, char **name)
{
    static char buf[1024];
    char *cmp = "#include \"";
    int n = strlen(cmp);
    for (int i = 0; i < n; i++)
    {
        if (ptr[i] == 0 || ptr[i] != cmp[i])
            return 0;
    }
    ptr += n;
    char *dst = buf;
    while (*ptr != '\"')
    {
        *dst = *ptr;
        ptr++;
        dst++;
    }
    *dst = 0;
    *name = buf;
    return 1;
}

void concatenate_file(char *filename, FILE *f)
{
    int n = 0;
    char **lines = read_lines(filename, &n);
    for (int i = 0; i < n; i++)
    {
        if (is_include(lines[i], &filename))
        {
            fprintf(f, "\n// Begin auto-include %s\n", filename);
            concatenate_file(filename, f);
            fprintf(f, "\n// End auto-include %s\n", filename);
        }
        else
        {
            fprintf(f, "%s\n", lines[i]);
        }
    }
}

void embed_html(char *filename, FILE *f)
{
    int n = 0;
    char **lines = read_lines(filename, &n);
    fprintf(f, "\n// Begin embedded app.html\n");
    fprintf(f, "const char *vdb_html_page = \n");
    for (int i = 0; i < n; i++)
    {
        // replace " with '
        // todo: more robust replacement
        for (int j = 0; j < strlen(lines[i]); j++)
        {
            if (lines[i][j] == '\"')
                lines[i][j] = '\'';
        }
        fprintf(f, "\"%s\\n\"\n", lines[i]);
    }
    fprintf(f, ";\n// End embedded app.html\n");
}

char *format(char *fmt, ...)
{
    static char buffer[1024*1024];
    va_list args;
    va_start(args, fmt);
    vsprintf(buffer, fmt, args);
    va_end(args);
    return buffer;
}

int main(int argc, char **argv)
{
    char *cwd = (argc > 1) ? argv[1] : "";
    FILE *f = fopen(format("%svdb_release.h", cwd), "w");
    concatenate_file(format("%svdb.h", cwd), f);
    fprintf(f, "\n");
    fprintf(f, "#define VDB_RELEASE\n\n");
    concatenate_file(format("%svdb.c", cwd), f);
    embed_html(format("%sapp.html", cwd), f);
    fclose(f);
}
