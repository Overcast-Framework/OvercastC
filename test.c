#include <stdio.h>

typedef struct
{
	int x;
	int y;
} Container;

void test_struct(Container* c)
{
	printf("%d", c->y);
}