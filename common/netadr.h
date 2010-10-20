//=======================================================================
//			Copyright XashXT Group 2010 ©
//		       netadr.h - net address local struct
//=======================================================================
#ifndef NETADR_H
#define NETADR_H

typedef enum
{
	NA_UNUSED,
	NA_LOOPBACK,
	NA_BROADCAST,
	NA_IP
} netadrtype_t;

typedef struct netadr_s
{
	netadrtype_t	type;
	unsigned char	ip[4];
	unsigned char	ipx[10];
	unsigned short	port;
} netadr_t;

#endif//NETADR_H