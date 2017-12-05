#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define ECHO_SERVER_PORT        60000
#define LISTEN_BACKLOG          16
#define MAX_EVENT_COUNT         20
#define BUF_SIZE                2048

#include <errno.h>
#include <fcntl.h>

// 注意事项 
// https://my.oschina.net/u/989096/blog/1189269

// g++ echo_server.cpp -o echo_server --std=c++11 
// ./echo_server
// 使用 netcat 工具  nc 127.0.0.1 60000 

/*

while [ 1 ] 
do   
nc 127.0.0.1 60000 &   
sleep 1   
done 

*/

int main() {
    int ret, i;
    int server_fd, client_fd, epoll_fd;
    int ready_count;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t addr_len;
    struct epoll_event event;
    struct epoll_event* event_array;
    char* buf;

    event_array = (struct epoll_event*) malloc(sizeof(struct epoll_event)*MAX_EVENT_COUNT);
    buf = (char*)malloc(sizeof(char)*BUF_SIZE);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);	// 任意的网卡 	 // INADDR_ANY就是inet_addr("0.0.0.0") 
    server_addr.sin_port = htons(ECHO_SERVER_PORT);		// htonl 32位从主机字节顺序转网络字节序


	//  把"A.B.C.D"的IP地址转换为32位长整数 :   unsigned long inet_addr ( const char FAR *cp );
	// 																	这个htonl之后给 struct sockaddr_in.sin_addr.s_addr
	//  网络地址转换为用点分割的IP地址 :  		char FAR *  inet_ntoa( struct in_addr in ); 
	// 																	struct in_addr 就是 struct sockaddr_in.sin_addr
 
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd == -1) {
        perror("create socket failed.\n");
        return 1;
    }
    ret = bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if(ret == -1) {
        perror("bind failed.\n");
        return 1;
    }
    ret = listen(server_fd, LISTEN_BACKLOG);
    if(ret == -1) {
        perror("listen failed.\n");
        return 1;
    }
	/*
		ls -l /proc/10068/fd
		lrwx------ 1 hhl hhl 64 9月  20 14:56 0 -> /dev/pts/19
		lrwx------ 1 hhl hhl 64 9月  20 14:56 1 -> /dev/pts/19
		lrwx------ 1 hhl hhl 64 9月  20 14:55 2 -> /dev/pts/19
		lrwx------ 1 hhl hhl 64 9月  20 14:56 3 -> socket:[84214]
		lrwx------ 1 hhl hhl 64 9月  20 14:56 4 -> anon_inode:[eventpoll]   << epoll_create后会多了一个打开的文件 
		lrwx------ 1 hhl hhl 64 9月  20 14:56 5 -> socket:[84219]			<<
		lrwx------ 1 hhl hhl 64 9月  20 14:56 6 -> socket:[84261]			<< 两个客户端连接
	*/
    epoll_fd = epoll_create(1); 
    if(epoll_fd == -1) {
        perror("epoll_create failed.\n");
        return 1;
    }
    event.events 	= EPOLLIN;
    event.data.fd 	= server_fd;	//	用于从epoll_wait返回  不一定是fd 可以是自己定义的结构体  
	
    ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event);
    if(ret == -1) {
        perror("epoll_ctl failed.\n");
        return 1;
    }
	
	event.events 	= EPOLLIN;
    event.data.fd 	= STDIN_FILENO;
	ret = epoll_ctl(epoll_fd,  EPOLL_CTL_ADD, STDIN_FILENO, &event);
	if(ret == -1) {
        perror("epoll_ctl failed.\n");
        return 1;
    }
	
	/*
		工作模式:
		epoll对文件描述符的操作有两种模式：LT（level trigger）和ET（edge trigger）
		EPOLLET   ET = 边缘触发(Edge Triggered)模式 
		
		***ET  无--有
		***LT  只要有 
		
		ET模式在很大程度上减少了epoll事件被重复触发的次数，因此效率要比LT模式高。
		1.epoll工作在ET模式的时候, 必须使用非阻塞套接口：
			***以避免由于一个文件句柄的阻塞读/阻塞写操作, 把处理多个文件描述符的任务饿死。
		2.因为如果数据没有读取完的话, 会一直处于有数据的状态, 因此不会再触发,所以：
			***直到read()返回EAGAIN这个错误时，表明内核收到的数据已经全部读完(因为read的Buffer可能不够装)
		3.使用边沿触发模式时的饥饿问题（Starvation） 
			如果某个socket源源不断地收到非常多的数据，
			那么在试图读取完所有数据的过程中，
			有可能会造成其他的socket得不到处理，从而造成饥饿
			***为每个已经准备好的描述符维护一个队列，这样程序就可以知道哪些描述符已经准备好了但是还在轮询等待
		
		运作机制:
		
		当我们执行epoll_ctl时，除了把socket放到epoll文件系统里file对象对应的红黑树上之外，
		还会给内核中断处理程序注册一个回调函数，告诉内核，如果这个句柄的中断到了，
		就把它放到准备就绪list链表里。
		
		所以，当一个socket上有数据到了，内核在把网卡上的数据copy到内核中，后就来把socket插入到准备就绪链表里了。
		
		(**** 实际就是 LT模式 epoll_wait 在 每次调用的时候 都先poll一下 是否有设备可读写了  ****)
		
		当一个socket句柄上有事件时，内核会把该句柄插入上面所说的准备就绪list链表，
		这时我们调用epoll_wait，会把准备就绪的socket拷贝到用户态内存，然后清空准备就绪list链表，
		最后，epoll_wait干了件事，就是检查这些socket，如果不是ET模式（就是LT模式的句柄了），
		并且这些socket上确实有未处理的事件时，又把该句柄放回到刚刚清空的准备就绪链表了。
		所以，非ET的句柄，只要它上面还有事件，epoll_wait每次都会返回。
		而ET模式的句柄，除非有新中断到，即使socket上的事件没有处理完，也是不会次次从epoll_wait返回的．
		
		重复设置：
		Case 1:
			重复ADD同一个文件描述符 会遇到错误 error 17 File exists    EEXIST
			dup()复制一个文件描述符 添加到相同的epoll实例 然后 使用不同的events掩码注册 非常实用的区分事件的方法 
		
		Case 2:
			如果把epoll文件描述符添加到它自己监听的描述符集中 epoll_ctl()函数会失败（EINVAL) 
			但可以把epoll文件描述符添加到其他的epoll文件描述符集中
		*/

	#if 0 
	event.events 	= EPOLLOUT | EPOLLHUP  ;
    event.data.fd 	= server_fd  ; // server_fd+1 
	ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event);
    if(ret == -1) { 


		printf("epoll_ctl 2 fail with error %d %s\n" , errno, strerror(errno) );
        return 1;
    }
	#endif 
	
	bool stop = false ;
    while( !stop ) {
		// 如果在两次epoll_wait()调用之间,有多个事件发生,那么它们会集中一起通知
        ready_count = epoll_wait(epoll_fd, event_array, MAX_EVENT_COUNT, -1);
        if(ready_count == -1) { 			// ready_count 返回0表示已超时
            perror("epoll_wait failed.\n");
            stop = true ; break;
        }
 
        for(i = 0; i < ready_count; i++) { 	// ready_count 返回有效的文件描述符数目
            if(event_array[i].data.fd == server_fd) {
                client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
                if(client_fd == -1) {
                    perror("accept failed.\n");
                    stop = true ; break;
                }
				
				int flags = fcntl(client_fd, F_GETFL, 0);   
				fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);  

				printf("Client ip %s port %d fd %d \n", inet_ntoa(client_addr.sin_addr) , ntohs(client_addr.sin_port) , client_fd );
                event.events = EPOLLIN;
                event.data.fd = client_fd;
                ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event);
                if(ret == -1) {
                    perror("epoll_ctl failed.\n");
                    stop = true ; break;
                }
            } else if ( event_array[i].data.fd == STDIN_FILENO ){
				
				ret = read(STDIN_FILENO, buf, BUF_SIZE-1); // 包含回车符号  e.g 命令行 #hello 收到  hello\n     
				if( ret <= 0 ){
				    printf("stardard input is broken with errno = %d %s!\n", errno ,strerror(errno));
					continue ;
				}
				
				printf("stdin-%d-%s-%d\n",ret, buf , strcmp("exit\n",buf)  );
				
				buf[ret] = '\0' ;
				if( strcmp("exit\n",buf) == 0 ) { 		// "exit\n"  退出
					printf("Exit Command !\n");
					stop = true ; break;
				} 
				
			} else {
				
				printf(">fd=%d event=0x%x\n" , event_array[i].data.fd  ,event_array[i].events );
				
                ret = recv(event_array[i].data.fd, buf, BUF_SIZE, 0);
                if(ret <= 0) {
					printf("client disconnect fd %d \n" , event_array[i].data.fd);
                    close(event_array[i].data.fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, event_array[i].data.fd, &event); 
					// EPOLL_CTL_DEL，event的内容被忽略 (为了兼容不要传NULL)
					// 关闭一个文件描述符会导致它自动从所有的epoll集合中移除
                    continue;
                }
				// 应改成 epoll_ctl(epollfd,EPOLL_CTL_ADD,fd, event.events= EPOLLOUT ); 
				// 更好的做法应该是, 使用epoll_wait(), 等待socket可写了以后, 再发送数据, 否则可能造成阻塞
				// 这样做需要为每个socket都维护一个数据结构，其中包含socket对应的读写buf和读写到的位置指针等信息
				
                ret = send(event_array[i].data.fd, buf, (size_t)ret, 0);
                if(ret == -1) {
                    perror("send failed.\n");
                }
            }
        } // for each event
    } // while(1)

    close(epoll_fd);
    close(server_fd);
	// ??? client_fd ???? 
    free(event_array);
    free(buf);

	//sleep(30);
    return 0;
}