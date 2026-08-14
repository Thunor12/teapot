#define TEAPOT_USE_EPOLL
#define TEAPOT_DEMO_WAIT_NAME "epoll"
#define STB_TEAPOT_IMPLEMENTATION
#include "docs_app.h"

int main(void)
{
    return teapot_docs_main();
}
