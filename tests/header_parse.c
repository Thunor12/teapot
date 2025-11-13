#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"
#include <stdio.h>
#include <string.h>

int main(void)
{

    const char *raw = "Host: localhost:8080\r\nContent-Type: text/plain\r\nUser-Agent: curl/8.6\r\n";

    tp_string_array headers = {0};
    tp_headers headers_parsed = {0};

    tp_chop_by_delim_into_array(&headers, raw, "\r\n");

    for (size_t i = 0; i < headers.count; i++)
    {
        char key[80] = {0};
        char value[80] = {0};
        tp_header_line header_line = {0};

        sscanf(headers.items[i], "%[^:]: %[^\r\n]", key, value);
        tp_sb_append_cstr(&header_line.name, key);
        tp_sb_append_cstr(&header_line.value, value);

        tp_da_append(&headers_parsed, header_line);

        printf("Header %lld: {Name: %s, Value: %s}\n", i, key, value);
    }

    tp_da_free(headers_parsed);
    tp_sa_free(headers);

    return 0;
}