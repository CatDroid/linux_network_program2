/*string.c*/
#define ENDSTRING '\0'					/*定义字符串*/
int StrLen(char *string)
{
  int len = 0;
  
  while(*string++ != ENDSTRING)			/*当*string的值为'\0'时，停止计算*/
    len++;
  return len;		 					/*返回此值*/
}
