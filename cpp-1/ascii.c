#include <stdio.h>

int main() {
    for (int i = 0; i<0x100; i++)
    {
        if (i<' ' || i>=0x7F)
        {
            printf("\"<0x%02X>\", ", i);
        }
        else
        {
            printf("\"%c\", ", i);
        }
    }
    printf("\n");
}
