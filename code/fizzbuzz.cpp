#include <stdio.h>

int main(int arc, char** argv)
{
    for(int i = 1; i <= 100; i++)
    {
        if(i % 3 == 0)
        {
            printf("Fizz");
        }
        if(i % 5 == 0)
        {
            printf("Buzz");
        }
        if((i % 3) && (i % 5))
        {
            printf("%i", i);
        }
        printf("\r\n");
    }   
}