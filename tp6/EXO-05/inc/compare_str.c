#include "compare.h"
int compare_str(char *str1, char *str2)
{
	while (*str1 && *str2 == *str2){
		str1++;
		str2++;
	}
	return *str1 - *str2;
}
