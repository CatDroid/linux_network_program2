


static struct lwip_socket sockets[NUM_SOCKETS];




/**
 * Map a externally used socket index to the internal socket representation.
 *
 * @param s externally used socket index
 * @return struct lwip_socket for the socket or NULL if not found
 */
static struct lwip_socket *get_socket(int s)
{
	struct lwip_socket *sock;
	if ((s < 0) || (s >= NUM_SOCKETS)) 
	{
		return NULL;
	}

	sock = &sockets[s];
	if (!sock->conn) 
	{
		return NULL;
	}

	return sock;
}

/**
 * Allocate a new socket for a given sock.
 *
 * @param newconn the sock for which to allocate a socket
 * @return the index of the new socket; -1 on error
 */
static int alloc_socket(struct sock *newconn)
{
	int i;

	/* allocate a new socket identifier */
	for (i = 0; i < NUM_SOCKETS; ++i) 
	{
		if (!sockets[i].conn) 
		{
			sockets[i].conn       = newconn;
			sockets[i].lastdata   = NULL;
			sockets[i].lastoffset = 0;
			sockets[i].rcvevent   = 0;
			sockets[i].sendevent  = 1; /* TCP send buf is empty */
			sockets[i].flags      = 0;
			sockets[i].err        = 0;
			return i;
		}
	}

	return -1;
}

/**
 * Close a sock 'connection' and free its resources.
 * UDP and RAW connection are completely closed, TCP pcbs might still be in a waitstate
 * after this returns.
 *
 * @param conn the sock to delete
 * @return 0 if the connection was deleted
 */
int
netconn_delete(struct sock *conn)
{
  /* No ASSERT here because possible to get a (conn == NULL) if we got an accept error */
  if (conn == NULL) {
    return -1;
  }

  conn->pcb.tcp = NULL;
  free(conn);

  return 0;
}


/* int socket(int domain, int type, int protocol);*/
int socket(int domain, int type, int protocol)
{
	struct sock *conn;
	int i;

	/* create a sock */
	switch (type) 
	{  
		case SOCK_DGRAM:
			conn = netconn_new( NETCONN_UDP);
			break;
		case SOCK_STREAM:
			conn = netconn_new(NETCONN_TCP);
			break;
		default:
			return -1;
	}

	if (!conn) 
	{
		return -1;
	}

	i = alloc_socket(conn);

	if (i == -1) 
	{
		netconn_delete(conn);
		return -1;
	}

	conn->socket = i;
	return i;
}


int close(int s)
{
	struct lwip_socket *sock;

	sock = get_socket(s);
	if (!sock) 
	{
		return -1;
	}

	netconn_delete(sock->conn);

	sys_sem_wait(socksem);
	if (sock->lastdata) 
	{
		netbuf_delete(sock->lastdata);
	}
	sock->lastdata   = NULL;
	sock->lastoffset = 0;
	sock->conn       = NULL;
	sys_sem_signal(socksem);

	return 0;
}

int bind(int s, struct sockaddr *name, socklen_t namelen)
{
	struct lwip_socket *sock;
	struct in_addr local_addr;
	__u16 port_local;
	int err;

	sock = get_socket(s);
	if (!sock)
		return -1;

	local_addr.addr = ((struct sockaddr_in *)name)->sin_addr.s_addr;
	port_local = ((struct sockaddr_in *)name)->sin_port;

	err = netconn_bind(sock->conn, &local_addr, ntohs(port_local));
	if (err != 0) 
	{
		return -1;
	}

	return 0;
}


int connect(int s, const struct sockaddr *name, socklen_t namelen)
{
	struct lwip_socket *sock;
	err_t err;

	sock = get_socket(s);
	if (!sock)
		return -1;

	if (((struct sockaddr_in *)name)->sin_family == AF_UNSPEC) 
	{
		err = netconn_disconnect(sock->conn);
	} 
	else 
	{
		struct ip_addr remote_addr;
		u16_t remote_port;

		remote_addr.addr = ((struct sockaddr_in *)name)->sin_addr.s_addr;
		remote_port = ((struct sockaddr_in *)name)->sin_port;

		err = netconn_connect(sock->conn, &remote_addr, ntohs(remote_port));
	}

	if (err != ERR_OK) 
	{
		return -1;
	}

	return 0;
}


int recvfrom(int s, void *mem, int len, unsigned int flags,
        struct sockaddr *from, socklen_t *fromlen)
{
	struct lwip_socket *sock;
	struct skbuff      *buf;
	u16_t               buflen, copylen, off = 0;
	struct ip_addr     *addr;
	u16_t               port;
	u8_t                done = 0;

	sock = get_socket(s);
	if (!sock)
		return -1;

	do {
		/* Check if there is data left from the last recv operation. */
		if (sock->lastdata) 
		{
			buf = sock->lastdata;
		} 
		else 
		{
			/* If this is non-blocking call, then check first */
			if (((flags & MSG_DONTWAIT) || (sock->flags & O_NONBLOCK)) && !sock->rcvevent) 
			{
				return -1;
			}

			/* No data was left from the previous operation, so we try to get
			some from the network. */
			sock->lastdata = buf = netconn_recv(sock->conn);

			if (!buf) 
			{
				/* We should really do some error checking here. */
				return 0;
			}
		}

		buflen = netbuf_len(buf);

		buflen -= sock->lastoffset;
		if (len > buflen) 
		{
			copylen = buflen;
		} 
		else 
		{
			copylen = len;
		}

		/* copy the contents of the received buffer into
		the supplied memory pointer mem */
		netbuf_copy_partial(buf, (u8_t*)mem + off, copylen, sock->lastoffset);
		off += copylen;

		if (netconn_type(sock->conn) == NETCONN_TCP) 
		{
			len -= copylen;
			if ( (len <= 0) || (buf->p->flags & PBUF_FLAG_PUSH) || !sock->rcvevent) 
			{
				done = 1;
			}
		} 
		else 
		{
			done = 1;
		}

		/* If we don't peek the incoming message... */
		if ((flags & MSG_PEEK)==0) 
		{
			/* If this is a TCP socket, check if there is data left in the
			buffer. If so, it should be saved in the sock structure for next
			time around. */
			if ((sock->conn->type == NETCONN_TCP) && (buflen - copylen > 0)) 
			{
				sock->lastdata = buf;
				sock->lastoffset += copylen;
			} 
			else 
			{
				sock->lastdata = NULL;
				sock->lastoffset = 0;
				netbuf_delete(buf);
			}
		} 
		else 
		{
			done = 1;
		}
	} while (!done);

	/* Check to see from where the data was.*/
	if (from && fromlen) 
	{
		struct sockaddr_in sin;

		if (netconn_type(sock->conn) == NETCONN_TCP) 
		{
			addr = (struct ip_addr*)&(sin.sin_addr.s_addr);
			netconn_getaddr(sock->conn, addr, &port, 0);
		} 
		else 
		{
			addr = netbuf_fromaddr(buf);
			port = netbuf_fromport(buf);
		}

		memset(&sin, 0, sizeof(sin));
		sin.sin_len = sizeof(sin);
		sin.sin_family = AF_INET;
		sin.sin_port = htons(port);
		sin.sin_addr.s_addr = addr->addr;

		if (*fromlen > sizeof(sin))
			*fromlen = sizeof(sin);

		SMEMCPY(from, &sin, *fromlen);
	} 

	return off;
}

int recv(int s, void *mem, int len, unsigned int flags)
{
  	return recvfrom(s, mem, len, flags, NULL, NULL);
}


int send(int s, const void *data, int size, unsigned int flags)
{
	struct lwip_socket *sock;
	err_t err;

	sock = get_socket(s);
	if (!sock)
		return -1;
	if (sock->conn->type!=NETCONN_TCP) 
	{
		return lwip_sendto(s, data, size, flags, NULL, 0);
	}

	err = netconn_write(sock->conn, data, size, NETCONN_COPY | ((flags & MSG_MORE)?NETCONN_MORE:0));

	return (err==ERR_OK?size:-1);
}


int sendto(int s, const void *data, int size, unsigned int flags,
       struct sockaddr *to, socklen_t tolen)
{
  struct lwip_socket *sock;
  struct ip_addr remote_addr;
  int err;

  sock = get_socket(s);
  if (!sock)
    return -1;

  if (sock->conn->type==NETCONN_TCP) 
  {
    return lwip_send(s, data, size, flags);
  }


  return (err==ERR_OK?size:-1);
}




