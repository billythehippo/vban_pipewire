#include "udpsocket.h"

/*static int sd;	//дескриптор сокета
struct sockaddr_in si_other;
struct sockaddr_in serv_addr;
int si_other_size=0;*/

//mode = 's', 'c' (s-server, c-client)

// int UDP_init(int* socdesc, sockaddr_in* si, const char* __restrict IP, uint16_t port, char mode, int broadcast, int priority)
// {
//     int bcast = 0;
//     int reuse = 1;
//     int prio  = 6; // 1- low priority, 7 - high priority
//     sockaddr_in serv_addr;
//     static int sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

//     if(sd<0)
//     {
//         perror("Invalid socket descriptor!\n");
//         return -1;
//     }
//     fprintf(stderr, "Socket creating, %d\n", sd);

//     if (*socdesc==sd)
//     {
//         if(setsockopt(sd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse))<0)
//         {
//             perror("Can't reuse address or port!\n");
//             return -1;
//         }
//     }
//     else *socdesc = sd;

//     if (broadcast==1) bcast = 1;
//     if(setsockopt(sd, SOL_SOCKET, SO_BROADCAST, &bcast, sizeof(bcast))<0)
//     {
//         perror("Can't allow broadcast mode!\n");
//         return -1;
//     }
//     else fprintf(stderr, "Broadcast mode successfully set!\n");

//     if ((priority>0)&&(priority<8)) prio = priority;
//     if(setsockopt(sd, SOL_SOCKET, SO_PRIORITY, &prio, sizeof(prio)))
//     {
//         perror("Can't set socket priority\n");
//         return -1;
//     }
//     else fprintf(stderr, "UDP priority is set to %d!\n", prio);

//     memset(&serv_addr, 0, sizeof(serv_addr));
//     memset(si, 0, sizeof(*si));

//     //server parameters
//     serv_addr.sin_family=AF_INET;	//IPv4
//     //serv_addr.sin_addr.s_addr=htonl(INADDR_ANY);	//local area networks only (by destination)
//     serv_addr.sin_addr.s_addr=inet_addr(IP);
//     serv_addr.sin_port=htons(port);

//     if(mode=='s') // bind, like server
//     {
//         if(bind(sd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr))<0)
//         {
//             perror("Error in func bind()");
//             return -1;
//         }
//     }
//     else *si=serv_addr; // no bind, like client

//     return *socdesc;
// }

int UDP_init(int* socket_descriptor, sockaddr_in* si, sockaddr_in* si_other, const char* __restrict IP, uint16_t port, char mode, int broadcast, int priority)
{
    int bcast = 0;
    int prio = 6;
    //int reuse = 1;
	
    //создаем UDP сокет
    static int sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sd<0)
    {
        perror("Error in func socket(): ");
        return -1;
    }
    fprintf(stderr, "Creating UDP socket , %d\n", sd);
	
    /*if (*socket_descriptor==sd)
    {
        if(setsockopt(sd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse))<0)
        {
            perror("Can't reuse address or port!\n");
            return -1;
        }
    }
    else*/ *socket_descriptor = sd;
	
    bcast = broadcast;
    if (bcast)
    {
        if(setsockopt(sd, SOL_SOCKET, SO_BROADCAST, &bcast, sizeof(bcast))<0)
        {
            perror("Error in func setsockopt() (allow broadcast mode): ");
            return -1;
        }
        fprintf(stderr, "Broadcast mode successfully set!\n");
    }
	
    if ((priority>0)&&(priority<8)) prio = priority;
    if(setsockopt(sd, SOL_SOCKET, SO_PRIORITY, &prio, sizeof(prio)))
    {
        perror("Can't set socket priority\n");
        return -1;
    }
    else fprintf(stderr, "UDP priority is set to %d!\n", prio);
	
    memset(si, 0, sizeof(*si));
    memset(si_other, 0, sizeof(*si_other));
	
    si->sin_family = AF_INET;	//IPv4
    si->sin_addr.s_addr = inet_addr(IP);//htonl(INADDR_ANY);
    si->sin_port = htons(port);
    
    if(mode=='s')	//server
    {
        //bind the socket with the server address
        if(bind(sd, (const struct sockaddr*)si, sizeof(*si))<0)
        {
            perror("Error in func bind(): ");
            return -1;
        }
        // si_other->sin_family = AF_INET;
        // si_other->sin_addr.s_addr = htonl(INADDR_ANY);
        // si_other->sin_port = htons(port);
    }
    else			//client
    {
        *si_other=*si;
    }
    
    return sd;
}

void UDP_deinit(int socket_descriptor)
{
	if(socket_descriptor) shutdown(socket_descriptor, SHUT_RDWR);
}

int UDP_send_datagram(u_int32_t ip, uint16_t port, uint8_t* buf, uint32_t buf_len, int priority)
{
    int prio = 3;
    sockaddr_in saddr;
    static int sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sd<0)
    {
        perror("Invalid socket descriptor!\n");
        return -1;
    }
    if ((priority>0)&&(priority<7)) prio = priority;
    if(setsockopt(sd, SOL_SOCKET, SO_PRIORITY, &prio, sizeof(prio)))
    {
        perror("Can't set socket priority\n");
        return -1;
    }
    saddr.sin_family=AF_INET;
    saddr.sin_addr.s_addr=ip;
    saddr.sin_port=port;
    socklen_t sisize=sizeof(saddr);
    int ret = sendto(sd, buf, buf_len, 0, (const struct sockaddr*)&saddr, sisize);
    return ret;
}

int UDP_send(int socket_descriptor, sockaddr_in* si_other, uint8_t* buf, uint32_t buf_len) // basic send to si_other
{
	int send_bytes=0;
	int sisize = sizeof(*si_other);	
    if(socket_descriptor) send_bytes = sendto(socket_descriptor, buf, buf_len, MSG_CONFIRM, (const struct sockaddr*)si_other, sisize);
	return send_bytes;
}

int UDP_recv(int socket_descriptor, sockaddr_in* si_other, uint8_t* buf, uint32_t buf_len)
{
	int rec_bytes=0;
    int sisize =  sizeof(struct sockaddr_in); //sizeof(*si_other);
    if(socket_descriptor) rec_bytes = recvfrom(socket_descriptor, buf, buf_len, MSG_WAITALL, (struct sockaddr*)si_other, (socklen_t*)&sisize);
    //volatile uint16_t port = si_other->sin_port;
	return rec_bytes;
}

void disp_recv_addr_info(sockaddr_in si_other)
{
	printf("Client_addr_info:\n");
	printf("IP: %s\n", inet_ntoa(si_other.sin_addr));
    printf("Addr struct size: %lu\n", sizeof(si_other));
}
