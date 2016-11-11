/*
 * route_ctl.c
 *
 *  Created on: 2016年11月7日
 *      Author: hanlon
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <net/route.h>
#include <sys/ioctl.h>

/*
 *  添加Null Routes 有效的屏蔽某个有问题的IP
	sudo route add 123.123.123.123 reject
	route del 123.123.123.123 reject
 */
int addNullRoute( long host )
{
   // create the control socket.
   int fd = socket( PF_INET, SOCK_DGRAM, IPPROTO_IP );
   struct rtentry route;
   memset( &route, 0, sizeof( route ) );
   // set the gateway to 0.
   struct sockaddr_in *addr = (struct sockaddr_in *)&route.rt_gateway;
   addr->sin_family = AF_INET;
   addr->sin_addr.s_addr = 0;
   // set the host we are rejecting.
   addr = (struct sockaddr_in*) &route.rt_dst;
   addr->sin_family = AF_INET;
   addr->sin_addr.s_addr = htonl(host);
   // Set the mask. In this case we are using 255.255.255.255, to block a single
   // IP. But you could use a less restrictive mask to block a range of IPs.
   // To block and entire C block you would use 255.255.255.0, or 0x00FFFFFFF
   addr = (struct sockaddr_in*) &route.rt_genmask;
   addr->sin_family = AF_INET;
   addr->sin_addr.s_addr = 0xFFFFFFFF;
   // These flags mean: this route is created "up", or active
   // The blocked entity is a "host" as opposed to a "gateway"
   // The packets should be rejected. On BSD there is a flag RTF_BLACKHOLE
   // that causes packets to be dropped silently. We would use that if Linux
   // had it. RTF_REJECT will cause the network interface to signal that the
   // packets are being actively rejected.
   route.rt_flags = RTF_UP | RTF_HOST | RTF_REJECT;
   route.rt_metric = 0;
   // this is where the magic happens..
   if ( ioctl( fd, SIOCADDRT, &route ) )
   {
      close( fd );
      return false;
   }
   // remember to close the socket lest you leak handles.
   close( fd );
   return true;
}

bool delNullRoute( long host )
{
   int fd = socket( PF_INET, SOCK_DGRAM, IPPROTO_IP );
   struct rtentry route;
   memset( &route, 0, sizeof( route ) );
   struct sockaddr_in *addr = (struct sockaddr_in *)&route.rt_gateway;
   addr->sin_family = AF_INET;
   addr->sin_addr.s_addr = 0;
   addr = (struct sockaddr_in*) &route.rt_dst;
   addr->sin_family = AF_INET;
   addr->sin_addr.s_addr = htonl(host);
   addr = (struct sockaddr_in*) &route.rt_genmask;
   addr->sin_family = AF_INET;
   addr->sin_addr.s_addr = 0xFFFFFFFF;
   route.rt_flags = RTF_UP | RTF_HOST | RTF_REJECT;
   route.rt_metric = 0;
   // this time we are deleting the route:
   if ( ioctl( fd, SIOCDELRT, &route ) )
   {
      close( fd );
      return false;
   }
   close( fd );
   return true;
}

