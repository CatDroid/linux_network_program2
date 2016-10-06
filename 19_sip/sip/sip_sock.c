

static struct sock *sock_list[];

struct sock * sip_get_sock(int fd)
{
	return sock_list[fd];
}





/**
 * Create a new sock (of a specific type) that has a callback function.
 * The corresponding pcb is NOT created!
 *
 * @param t the type of 'connection' to create (@see enum netconn_type)
 * @param proto the IP protocol for RAW IP pcbs
 * @param callback a function to call on status changes (RX available, TX'ed)
 * @return a newly allocated struct sock or
 *         NULL on memory error
 */
struct sock*
netconn_alloc(enum netconn_type t, netconn_callback callback)
{
	struct sock *conn;
	int size;

	conn = malloc(sizeof(struct sock));
	if (conn == NULL) 
	{
		return NULL;
	}

	conn->err = 0;
	conn->type = t;
	conn->pcb.tcp = NULL;

	if ((conn->op_completed = sys_sem_new(0)) == SYS_SEM_NULL) 
	{
		memp_free(MEMP_NETCONN, conn);
		return NULL;
	}

	if ((conn->recvmbox = sys_mbox_new(size)) == SYS_MBOX_NULL) 
	{
		sys_sem_free(conn->op_completed);
		memp_free(MEMP_NETCONN, conn);
		return NULL;
	}

	conn->acceptmbox   = SYS_MBOX_NULL;
	conn->state        = NETCONN_NONE;
	/* initialize socket to -1 since 0 is a valid socket */
	conn->socket       = -1;
	conn->callback     = callback;
	conn->recv_avail   = 0;
	conn->recv_timeout = 0;
	conn->recv_bufsize = INT_MAX;

	return conn;
}

/**
 * Create a new sock (of a specific type) that has a callback function.
 * The corresponding pcb is also created.
 *
 * @param t the type of 'connection' to create (@see enum netconn_type)
 * @param proto the IP protocol for RAW IP pcbs
 * @param callback a function to call on status changes (RX available, TX'ed)
 * @return a newly allocated struct sock or
 *         NULL on memory error
 */
struct sock*
netconn_new(enum netconn_type t, u8_t proto)
{
	struct sock *conn;

	conn = netconn_alloc(t, callback);

	return conn;
}


/**
 * Bind a sock to a specific local IP address and port.
 * Binding one sock twice might not always be checked correctly!
 *
 * @param conn the sock to bind
 * @param addr the local IP address to bind the sock to (use IP_ADDR_ANY
 *             to bind to all addresses)
 * @param port the local port to bind the sock to (not used for RAW)
 * @return ERR_OK if bound, any other err_t on failure
 */
err_t
netconn_bind(struct sock *conn, struct in_addr *addr, u16_t port)
{
	if (conn->pcb.tcp != NULL) 
	{
		switch (NETCONNTYPE_GROUP(conn->type)) 
		{
			case NETCONN_RAW:
				conn->err = raw_bind(conn->pcb.raw,addr);
				break;
			case NETCONN_UDP:
				conn->err = SIP_UDPBind(conn->pcb.udp, addr, port);
				break;
			case NETCONN_TCP:
				msg->conn->err = tcp_bind(conn->pcb.tcp, addr, port);
				break;
			default:
				break;
		}
	} 
	else 
	{
		/* msg->conn->pcb is NULL */
      		msg->conn->err = ERR_VAL;
	}
}


/**
 * Connect a sock to a specific remote IP address and port.
 *
 * @param conn the sock to connect
 * @param addr the remote IP address to connect to
 * @param port the remote port to connect to (no used for RAW)
 * @return ERR_OK if connected, return value of tcp_/udp_/raw_connect otherwise
 */
err_t netconn_connect(struct sock *conn, struct in_addr *addr, u16_t port)
{
  	if (msg->conn->pcb.tcp == NULL) 
	{
		sys_sem_signal(conn->op_completed);
		return;
	}

  	switch (NETCONNTYPE_GROUP(conn->type)) 
	{
		case NETCONN_RAW:
			conn->err = raw_connect(conn->pcb.raw,addr);
			sys_sem_signal(conn->op_completed);
			break;
		case NETCONN_UDP:
			conn->err = SIP_UDPConnect(conn->pcb.udp, addr, port);
			sys_sem_signal(conn->op_completed);
			break;
		case NETCONN_TCP:
			msg->conn->state = NETCONN_CONNECT;
			setup_tcp(msg->conn);
			msg->conn->err = tcp_connect(conn->pcb.tcp, addr, port, do_connected);
			/* sys_sem_signal() is called from do_connected (or err_tcp()),
			* when the connection is established! */
			break;
		default:
			break;
	}
}




/**
 * Disconnect a sock from its current peer (only valid for UDP netconns).
 *
 * @param conn the sock to disconnect
 * @return TODO: return value is not set here...
 */
err_t netconn_disconnect(struct sock *conn)
{
	 if (NETCONNTYPE_GROUP(msg->conn->type) == NETCONN_UDP) 
	 {
	 	SIP_UDPDisconnect(conn->pcb.udp);
	}
}



/**
 * Receive data (in form of a skbuff containing a packet buffer) from a sock
 *
 * @param conn the sock from which to receive data
 * @return a new skbuff containing received data or NULL on memory error or timeout
 */
struct skbuff *netconn_recv(struct sock *conn)
{
	struct skbuff *skb_recv = conn->skb_recv;

	if(skb_recv == NULL)
		return NULL;

	conn->skb_recv = skb_recv->next;
	skb_recv->next = NULL;
	
	return skb_recv;
}


/**
 * Send data (in form of a skbuff) to a specific remote IP address and port.
 * Only to be used for UDP and RAW netconns (not TCP).
 *
 * @param conn the sock over which to send data
 * @param buf a skbuff containing the data to send
 * @param addr the remote IP address to which to send the data
 * @param port the remote port to which to send the data
 * @return ERR_OK if data was sent, any other err_t on error
 */
err_t
netconn_sendto(struct sock *conn, struct skbuff *skb, struct in_addr *addr, u16_t port)
{
if (conn->pcb.tcp != NULL) 
	{
		switch (NETCONNTYPE_GROUP(msg->conn->type)) 
		{
			case NETCONN_RAW:
				conn->err = raw_sendto(conn->pcb.raw, skb, addr);
				break;
				conn->err = SIP_UDPSendTo(conn->pcb.udp, skb, addr, port);
				break;
			default:
				break;
		}
	}

	return ERR_VAL;
}


/**
 * Send data over a UDP or RAW sock (that is already connected).
 *
 * @param conn the UDP or RAW sock over which to send data
 * @param buf a skbuff containing the data to send
 * @return ERR_OK if data was sent, any other err_t on error
 */
err_t
netconn_send(struct sock *conn, struct skbuff *skb)
{
	if (conn->pcb.tcp != NULL) 
	{
		switch (NETCONNTYPE_GROUP(msg->conn->type)) 
		{
			case NETCONN_RAW:
				conn->err = raw_send(conn->pcb.raw, skb);
				break;
			case NETCONN_UDP:
				conn->err = SIP_UDPSend(conn->pcb.udp, skb);
				break;
			default:
				break;
		}
	}
}

