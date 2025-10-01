#pragma once

#ifndef UDP_H_
#define UDP_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <linux/errqueue.h>
#include <linux/icmp.h>
#include <netdb.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <netdb.h>

//#include "../vban_common/vban_functions.h"

typedef struct
{
    int fd;
    struct sockaddr_in c_addr;
} udpc_t;


int getipaddresses(uint32_t* ips, uint32_t* ipnum);
in_addr get_ip_by_name(char* __restrict ifname);
udpc_t* udp_init(uint16_t rx_port, uint16_t tx_port, char* __restrict rx_ip, char* __restrict tx_ip, suseconds_t t, uint8_t priority, int broadcast);
int set_recverr(int fd);
void udp_free(udpc_t* c);


inline int udp_send(udpc_t* c, uint16_t txport, char* data, size_t n, in_addr_t txip = 0)
{
    in_addr_t ip = c->c_addr.sin_addr.s_addr;
    if(txport!= 0) c->c_addr.sin_port = htons(txport);
    if(txip!= 0) c->c_addr.sin_addr.s_addr = txip;
    int ret = sendto(c->fd, data, n, 0, (struct sockaddr*)&(c->c_addr), sizeof(struct sockaddr_in));
    c->c_addr.sin_addr.s_addr = ip;
    if(ret < 0) return 0;
    return ret;
}


inline int udp_recv(udpc_t* c, void* data, size_t n)
{
	unsigned int c_addr_size = sizeof(struct sockaddr_in);
    int ret = recvfrom(c->fd, data, n, 0, (struct sockaddr*)&(c->c_addr), &c_addr_size);
	if(ret < 0) return 0;
	return ret;
}


inline uint32_t udp_get_sender_ip(udpc_t* c)
{
    return htonl(c->c_addr.sin_addr.s_addr);
}


inline uint16_t udp_get_sender_port(udpc_t* c)
{
    return htons(c->c_addr.sin_port);
}


inline int check_icmp_status(int sockfd)
{
    struct msghdr msg;
    char control_buf[CMSG_SPACE(sizeof(struct sock_extended_err))];
    struct iovec iov;
    char data_buf[1];
    iov.iov_base = data_buf;
    iov.iov_len = sizeof(data_buf);

    memset(&msg, 0, sizeof(msg));
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control_buf;
    msg.msg_controllen = sizeof(control_buf);
    msg.msg_flags = 0;

    ssize_t ret = recvmsg(sockfd, &msg, MSG_ERRQUEUE | MSG_PEEK);
    if(ret < 0) return -1;

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if(cmsg && cmsg->cmsg_level == SOL_IP && cmsg->cmsg_type == IP_RECVERR)
    {
        struct sock_extended_err *err = (struct sock_extended_err *)CMSG_DATA(cmsg);
        if(err->ee_origin == SO_EE_ORIGIN_ICMP || err->ee_origin == SO_EE_ORIGIN_ICMP6)
            if(err->ee_type == ICMP_DEST_UNREACH)
                return err->ee_errno;
    }

    return 0;
}


#endif
