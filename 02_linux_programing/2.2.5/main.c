/*main.c*/
#include <stdio.h>
extern int StrLen(char* str);						/*声明Strlen函数*/
int main(void)
{
  char src[]="Hello Dymatic";						/*字符串*/
  printf("string length is:%d\n",StrLen(src));		/*计算src的长度，将结果													   打印出来*/
  return 0;
}
