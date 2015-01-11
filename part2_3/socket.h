#ifndef __SOCKET_H__
#define __SOCKET_H__

#include <cstdint>
#include <string>

typedef int socketfd_t;

const int IP_MAX_LEN = 32;

struct SocketAddr{
    /* only support ipv4 now */
    std::string ipv4_addr_str;
    uint16_t port_hbytes;

    SocketAddr();
    SocketAddr(const char *ipv4_addr_str, uint16_t port_hbytes);
    void set_sockaddr(uint32_t ipv4_nbytes, uint16_t port_nbytes);
    void get_sockaddr(uint32_t* ipv4_nbytes, uint16_t* port_nbytes);

    void to_sockaddr_in(struct sockaddr_in& ret_addr);
    void from_sockaddr_in(struct sockaddr_in& input_addr);
};

int socket_bind(socketfd_t socketfd, SocketAddr& bind_addr);
int socket_connect(socketfd_t socketfd, SocketAddr& connect_addr);
socketfd_t socket_accept(socketfd_t socketfd, SocketAddr& accept_addr);

#endif
