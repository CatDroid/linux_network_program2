#include <linux/netfilter_ipv4.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/ip.h>					/*IP头部结构*/
#include <net/tcp.h> 				/*TCP头部结构*/
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include "nf_sockopte.h"
/* 版权声明*/
MODULE_LICENSE("Dual BSD/GPL");
/* NF初始化状态宏 */
#define NF_SUCCESS 0
#define NF_FAILURE 1
/* 初始化绑定状态 */
band_status b_status ;
/*快速绑定操作宏*/
/* 判断是否禁止TCP的端口*/
#define IS_BANDPORT_TCP(status)( status.band_port.port != 0 && status.band_port.protocol == IPPROTO_TCP)
/*判断是否禁止UDP端口 */
#define IS_BANDPORT_UDP(status)( status.band_port.port != 0 && status.band_port.protocol == IPPROTO_UDP)
/* 判断端是否禁止 PING */
#define IS_BANDPING(status)( status.band_ping )
/* 判断是否禁止IP协议 */
#define IS_BANDIP(status)( status.band_ip )
/* nf sock 选项扩展操作*/
static int 
nf_sockopt_set(struct sock *sock, 
	int cmd, 
	void __user *user, 
	unsigned int len)
{
	int ret = 0;
	struct band_status status;
	
	/* 权限检查 */
	if(!capable(CAP_NET_ADMIN))				/*没有足够权限*/
	{
		ret = -EPERM;
		goto ERROR;
	}
	/* 从用户空间复制数据*/
	ret = copy_from_user(&status, user,len);
	if(ret != 0)								/*复制数据失败*/
	{
		ret = -EINVAL;
		goto ERROR;
	}
	
	/* 命令类型 */
	switch(cmd)
	{
		case SOE_BANDIP:							/*禁止IP协议*/
			/* 设置禁止IP协议 */
			if(IS_BANDIP(status))					/*设置禁止IP协议*/
				b_status.band_ip = status.band_ip;
			else									/*取消禁止*/
				b_status.band_ip = 0;
	
			break;
		case SOE_BANDPORT:						/*禁止端口*/
			/* 设置端口禁止和相关的协议类型 */
			if(IS_BANDPORT_TCP(status))				/*禁止TCP*/
			{
				b_status.band_port.protocol = IPPROTO_TCP;
				b_status.band_port.port = status.band_port.port;
			}
			else if(IS_BANDPORT_UDP(status))			/*禁止UDP*/
			{
				b_status.band_port.protocol = IPPROTO_UDP;
				b_status.band_port.port = status.band_port.port;
			}
			else									/*其他*/
			{
				b_status.band_port.protocol = 0;
				b_status.band_port.port = 0;
			}
			
			break;
		case SOE_BANDPING:						/*禁止ping*/
			if(IS_BANDPING(status))				/*禁止PING*/
			{
				b_status.band_ping = 1;
			}
			else 									/*取消禁止*/
			{
				b_status.band_ping = 0;
			}
			
			break;
		default:									/*其他为错误命令*/
			ret = -EINVAL;
			break;
	}
	
ERROR:
	return ret;
}
/* nf sock 操作扩展命令操作*/
static int 
nf_sockopt_get(struct sock *sock, 
	int cmd, 
	void __user *user, 
	unsigned int len)
{
	int ret = 0;
	
	/* 权限检查*/
	if(!capable(CAP_NET_ADMIN))				/*没有权限*/
	{
		ret = -EPERM;
		goto ERROR;
	}	
	
	/* 将数据从内核空间复制到用户空间 */
	switch(cmd)
	{
		case SOE_BANDIP:
		case SOE_BANDPORT:
		case SOE_BANDPING:
			/*复制数据*/
			ret = copy_to_user(user, &b_status,len);
			if(ret != 0)								/*复制数据失败*/
			{
				ret = -EINVAL;
				goto ERROR;
			}
			break;
		default:
			ret = -EINVAL;
			break;
	}
	
ERROR:
	return ret;
}
/* 在LOCAL_OUT上挂接钩子 */
static unsigned int nf_hook_out(unsigned int hooknum,
	struct sk_buff **skb,
	const struct net_device *in,
	const struct net_device *out,
	int (*okfn)(struct sk_buff*))
{
	struct sk_buff *sk = *skb;
	struct iphdr *iph = ip_hdr(sk);
	
	if(IS_BANDIP(b_status))						/*判断是否禁止IP协议*/
	{
		if(b_status.band_ip == iph->saddr)			/*IP地址符合*/
		{
			return NF_DROP;							/*丢弃该网络报文*/
		}
	}
	
	return NF_ACCEPT;
}
/* 在LOCAL_IN挂接钩子 */
static unsigned int nf_hook_in(unsigned int hooknum,
	struct sk_buff **skb,
	const struct net_device *in,
	const struct net_device *out,
	int (*okfn)(struct sk_buff*))
{
	struct sk_buff *sk = *skb;
	struct iphdr *iph = ip_hdr(sk);
	unsigned int src_ip = iph->saddr;
	struct tcphdr *tcph = NULL;
	struct udphdr *udph = NULL;
	
	switch(iph->protocol)								/*IP协议类型*/
	{
		case IPPROTO_TCP:								/*TCP协议*/
			/*丢弃禁止端口的TCP数据*/
			if(IS_BANDPORT_TCP(b_status))
			{
				tcph = tcp_hdr(sk);							/*获得TCP头*/
				if(tcph->dest == b_status.band_port.port)	/*端口匹配*/
				{
					return NF_DROP;							/*丢弃该数据*/
				}
			}
			
			break;
		case IPPROTO_UDP:								/*UDP协议*/
			/*丢弃UDP数据*/
			if(IS_BANDPORT_UDP(b_status))					/*设置了丢弃UDP协议*/
			{
				udph = udp_hdr(sk);							/*UDP头部*/
				if(udph->dest == b_status.band_port.port)	/*UDP端口判定*/
				{
					return NF_DROP;								/*丢弃该数据*/
				}
			}
			
			break;
		case IPPROTO_ICMP:								/*ICMP协议*/
			/*丢弃ICMP报文*/
			if(!IS_BANDPING(b_status))						/*设置了禁止PING操作*/
			{
				printk(KERN_ALERT "DROP ICMP packet from %d.%d.%lld.%lld\n",
								 (src_ip&0xff000000)>>24,
								 (src_ip&0x00ff0000)>>16,
								 (src_ip&0xff0000ff00)>>8,
								 (src_ip&0xff000000ff)>>0);
				return NF_DROP;								/*丢弃该报文*/
			}
			
			break;
		default:
			break;
	}
	
	return NF_ACCEPT;
}
/* 初始化nfin钩子，在钩子LOCAL_IN上 */
static struct nf_hook_ops nfin = 
{
	.hook = nf_hook_in,
	.hooknum = NF_IP_LOCAL_IN,
	.pf = PF_INET,
	.priority = NF_IP_LOCAL_OUT
};
/*初始化nfout钩子，在钩子LOCAL_OUT上*/
static struct nf_hook_ops nfout=
{
	.hook = nf_hook_out,
	.hooknum = NF_IP_LOCAL_OUT,
	.pf = PF_INET,
	.priority = NF_IP_PRI_FIRST
};
/* 初始化nf套接字选项 */
static struct nf_sockopt_ops nfsockopt = {
 .pf	= PF_INET,
 .set_optmin = SOE_BANDIP,
 .set_optmax = SOE_BANDIP+2,
 .set	= nf_sockopt_set,
 .get_optmin = SOE_BANDIP,
 .get_optmax = SOE_BANDIP+2,
 .get	= nf_sockopt_get,
};
/* 初始化模块 */
static int __init init(void)
{
	nf_register_hook(&nfin);					/*注册LOCAL_IN的钩子*/
	nf_register_hook(&nfout);					/*注册LOCAL_OUT的钩子*/
	nf_register_sockopt(&nfsockopt);			/*注册扩展套接字选项*/
	
	printk(KERN_ALERT "netfilter example 2 init successfully\n");
												/*打印信息*/
	return NF_SUCCESS;
}
/* 清理模块 */
static void __exit exit(void)
{	
	nf_unregister_hook(&nfin);					/*注销LOCAL_IN的钩子*/
	nf_unregister_hook(&nfout);					/*注销LOCAL_OUT的钩子*/
	nf_unregister_sockopt(&nfsockopt);			/*注销扩展套接字选项*/
	printk(KERN_ALERT "netfilter example 2 clean successfully\n");
}
module_init(init);								/*初始化模块*/
module_exit(exit);								/*模块退出*/
/* 作者、描述、版本、别名*/
MODULE_AUTHOR("Jingbin Song");				/*作者声明*/
MODULE_DESCRIPTION("netfilter DEMO");		/*模块描述信息声明*/
MODULE_VERSION("0.0.1");					/*模块版本声明*/
MODULE_ALIAS("ex17.2");						/*模块别名声明*/
