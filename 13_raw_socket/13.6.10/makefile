all:client server sync_flood	    #all规则，它依赖于client和server规则

sync_flood:tcp_syn_flood.o
	g++ -o sync_flood tcp_syn_flood.o -lpthread
	rm -f tcp_syn_flood.o
client: tcp_client.o	 
	gcc -o client tcp_client.o
	rm -r tcp_client.o
server: tcp_server.o				#server规则，生成服务器端可执行程序
	gcc -o server tcp_server.o	
	rm -r tcp_server.o
clean:								#清理规则，删除client、server和中间文件
	rm -f client server sync_flood *.o
