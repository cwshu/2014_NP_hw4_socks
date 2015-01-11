#include <cstring>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "socket.h"

using namespace std;

/* SocketAddr */
SocketAddr::SocketAddr(){
    port_hbytes = 0;
}

SocketAddr::SocketAddr(const char *ipv4_addr_str, uint16_t port_hbytes){ 
    this->ipv4_addr_str = std::string(ipv4_addr_str);
    this->port_hbytes = port_hbytes;
}

void SocketAddr::set_sockaddr(uint32_t ipv4_nbytes, uint16_t port_nbytes){
    struct sockaddr_in sock_addr;
    memset(&sock_addr, 0, sizeof(struct sockaddr_in));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = ipv4_nbytes;
    sock_addr.sin_port = port_nbytes;

    from_sockaddr_in(sock_addr);
}

void SocketAddr::get_sockaddr(uint32_t* ipv4_nbytes, uint16_t* port_nbytes){
    struct sockaddr_in sock_addr;
    to_sockaddr_in(sock_addr);
    
    *ipv4_nbytes = sock_addr.sin_addr.s_addr;
    *port_nbytes = sock_addr.sin_port;
}

void SocketAddr::to_sockaddr_in(struct sockaddr_in& ret_addr){
    memset(&ret_addr, 0, sizeof(struct sockaddr_in));
    ret_addr.sin_family = AF_INET;
    inet_aton(ipv4_addr_str.c_str(), &(ret_addr.sin_addr));
    ret_addr.sin_port = htons((uint16_t)port_hbytes);
}

void SocketAddr::from_sockaddr_in(struct sockaddr_in& input_addr){
    ipv4_addr_str = std::string(inet_ntoa(input_addr.sin_addr));
    port_hbytes = ntohs(input_addr.sin_port);
}

int socket_bind(socketfd_t socketfd, SocketAddr& bind_addr){
    struct sockaddr_in addr;
    bind_addr.to_sockaddr_in(addr);

    return bind(socketfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));
}

int socket_connect(socketfd_t socketfd, SocketAddr& connect_addr){
    struct sockaddr_in addr;
    connect_addr.to_sockaddr_in(addr);

    return connect(socketfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));
}

socketfd_t socket_accept(socketfd_t socketfd, SocketAddr& accept_addr){
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(struct sockaddr_in);
    int connection_fd = accept(socketfd, (struct sockaddr*)&client_addr, &client_addr_len);

    if( connection_fd < 0 )
        /* accept error */
        return connection_fd;

    accept_addr.from_sockaddr_in(client_addr);
    return connection_fd;
}
