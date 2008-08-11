#include "admath.h"

int sqrt(int a)
{
	int loc =128;
	int x = loc * loc;
	int level = 6;
	while((level >= 0) && (x != a))
	{
		if(a > x) loc += (1<<level);
		else loc -= (1<<level);
		level --;
		x = loc * loc;
	}
	int test = (loc * loc)-a;
	if (test >= loc) loc --;
	else if (test < (loc * -1)) loc ++;
	return loc;

}
