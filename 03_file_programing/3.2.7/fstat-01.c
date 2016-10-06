/*文件fstat-01.c,使用stat获得文件的状态*/
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
int main(void)
{
	struct stat st;
	
	if( -1 == stat("test.txt", &st)){ 	/*获得文件的状态，将状态值放入st中*/
		printf("获得文件状态失败\n");
		return -1;
	}
	
	printf("包含此文件的设备ID：%d\n",(int)st.st_dev);		/*文件的ID号*/
	printf("此文件的节点：%d\n",(int)st.st_ino);				/*文件的节点*/
	printf("此文件的保护模式：%d\n",(int)st.st_mode);		/*文件的模式*/
	printf("此文件的硬链接数：%d\n",(int)st.st_nlink);		/*文件的硬链接数*/
	printf("此文件的所有者ID：%d\n",(int)st.st_uid);			/*文件的所有者ID*/
	printf("此文件的所有者的组ID：%d\n",(int)st.st_gid);		/*文件的组ID*/
	printf("设备ID（如果此文件为特殊设备）：%d\n",(int)st.st_rdev); /*文件的设备ID*/
	printf("此文件的大小：%d\n",(int)st.st_size);				/*文件的大小*/
	printf("此文件的所在文件系统块大小：%d\n",(int)st.st_blksize);	/*文件的系统块大小*/
	printf("此文件的占用块数量：%d\n",(int)st.st_blocks);			/*文件的块大小*/
	printf("此文件的最后访问时间：%d\n",(int)st.st_atime);	/*文件的最后访问时间*/
	printf("此文件的最后修改时间：%d\n",(int)st.st_mtime);	/*文件的最后修改时间*/
	printf("此文件的最后状态改变时间：%d\n",(int)st.st_ctime);/*文件的最后状态改变时间*/
	
	return 0;
}
