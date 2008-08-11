#include <iostream.h>
#include <stdio.h>

#include "EQU.h"


char order[4];
char operators[10] = "*/+-";

unsigned char text[256];

struct equation{
	char type;
	void *item;
} equation;

void main()
{
	order[0] = '*';
	order[1] = '/';
	order[2] = '+';
	order[3] = '-';

	printf("Input an equation:");
	getEquation();
	sortEquation();

	cout<< text;

}

void getEquation()
{
	gets(text);
	return;
}

void sortEquation()
{
	char *index;
	index = &text;
	while(*index != "\0")
	{
		index++;
		char place = strstr(operators,*index);
		if (place != null)
		{

		}
	}
}

void push()
{

}

void * pop()
{

}