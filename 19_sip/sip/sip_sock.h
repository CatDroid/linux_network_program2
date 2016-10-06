#ifndef __SIP_SOCK_H__
#define __SIP_SOCK_H__



struct sock;

enum netconn_type {
  NETCONN_INVALID    = 0,
  /* NETCONN_TCP Group */
  NETCONN_TCP        = 0x10,
  /* NETCONN_UDP Group */
  NETCONN_UDP        = 0x20,
  NETCONN_UDPLITE    = 0x21,
  NETCONN_UDPNOCHKSUM= 0x22,
  /* NETCONN_RAW Group */
  NETCONN_RAW        = 0x40
};

enum netconn_state {
  NETCONN_NONE,
  NETCONN_WRITE,
  NETCONN_LISTEN,
  NETCONN_CONNECT,
  NETCONN_CLOSE
};
enum netconn_evt {
  NETCONN_EVT_RCVPLUS,
  NETCONN_EVT_RCVMINUS,
  NETCONN_EVT_SENDPLUS,
  NETCONN_EVT_SENDMINUS
};

struct tcp_pcb {};
typedef void (* netconn_callback)(struct sock *, enum netconn_evt, __u16 len);

#define NUM_SOCKETS 4
/** Contains all internal pointers and states used for a socket */
struct lwip_socket {
  /** sockets currently are built on netconns, each socket has one sock */
  struct sock *conn;
  /** data that was left from the previous read */
  struct skbuff *lastdata;
  /** offset in the data that was left from the previous read */
  __u16 lastoffset;
  /** number of times data was received, set by event_callback(),
      tested by the receive and select functions */
  __u16 rcvevent;
  /** number of times data was received, set by event_callback(),
      tested by select */
  __u16 sendevent;
  /** socket flags (currently, only used for O_NONBLOCK) */
  __u16 flags;
  /** last error that occurred on this socket */
  int err;
};
/** A sock descriptor */
struct sock {
  /** type of the sock (TCP, UDP or RAW) */
  enum netconn_type type;
  /** current state of the sock */
  enum netconn_state state;
  /** the lwIP internal protocol control block */
  union {
    struct ip_pcb  *ip;
    struct tcp_pcb *tcp;
    struct udp_pcb *udp;
    //struct raw_pcb *raw;
  } pcb;
  /** the last error this sock had */
  int err;
  /** sem that is used to synchroneously execute functions in the core context */
  sys_sem_t op_completed;
  /** mbox where received packets are stored until they are fetched
      by the sock application thread (can grow quite big) */
  struct skbuff *skb_recv;
  /** mbox where new connections are stored until processed
      by the application thread */
  sys_mbox_t acceptmbox;
  /** only used for socket layer */
  int socket;
#if LWIP_SO_RCVTIMEO
  /** timeout to wait for new data to be received
      (or connections to arrive for listening netconns) */
  int recv_timeout;
#endif /* LWIP_SO_RCVTIMEO */
#if LWIP_SO_RCVBUF
  /** maximum amount of bytes queued in recvmbox */
  int recv_bufsize;
#endif /* LWIP_SO_RCVBUF */
  __u16 recv_avail;
  /** TCP: when data passed to netconn_write doesn't fit into the send buffer,
      this temporarily stores the message. */
  /** TCP: when data passed to netconn_write doesn't fit into the send buffer,
      this temporarily stores how much is already sent. */
  int write_offset;
#if LWIP_TCPIP_CORE_LOCKING
  /** TCP: when data passed to netconn_write doesn't fit into the send buffer,
      this temporarily stores whether to wake up the original application task
      if data couldn't be sent in the first try. */
  u8_t write_delayed;
#endif /* LWIP_TCPIP_CORE_LOCKING */
  /** A callback function that is informed about events for this sock */
  netconn_callback callback;
};

#endif/*__SIP_SOCK_H__*/
