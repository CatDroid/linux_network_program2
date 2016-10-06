int main()
{
	int i = 0;									/*计数器*/
	int fd=0;									/*文件描述符*/
	for(i=0;fd>=0;i++)							/*循环打开文件直到出错*/
	{
		fd = open("test.txt",O_RDONLY);		/*只读打开文件*/
		if(fd > 0){							/*打开文件成功*/
			printf("fd:%d\n",fd);			/*打印文件描述符*/
		}
		else{									/*打开文件失败*/
			printf("error, can't open file\n");	/*打印错误*/
			exit(0);						/*退出*/
		}
	}
}
