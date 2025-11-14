#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    char raw[] = "Host: localhost:8080\r\nContent-Type: text/plain\r\nUser-Agent: curl/8.6\r\n";

    tp_headers headers_parsed = {0};

    tp_extract_header_keyval(&headers_parsed, raw, sizeof(raw));

    for (size_t i = 0; i < headers_parsed.count; i++)
    {
        printf("Header " TP_SIZE_T_FMT ": {Name: %s, Value: %s}\n", i, //
               headers_parsed.items[i].name.items,                     //
               headers_parsed.items[i].value.items                     //
        );
    }

    tp_headers_free(&headers_parsed);

    return 0;
}
