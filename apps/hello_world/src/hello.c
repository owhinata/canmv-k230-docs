#include <stdio.h>

int main(void)
{
#if defined(K230_BIGCORE)
    printf("Hello, K230 bigcore!\n");
#elif defined(K230_LITTLECORE)
    printf("Hello, K230 littlecore!\n");
#else
    printf("Hello, K230!\n");
#endif
    return 0;
}
