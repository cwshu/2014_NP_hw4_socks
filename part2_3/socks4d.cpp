#include <iostream>
#include <cstdlib>
#include <cstdint>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>

#include "socket.h"
#include "server_arch.h"
#include "io_wrapper.h"

const unsigned char SOCKS4_ERROR = 255;
const unsigned char SOCKS4_CONNECT = 1;
const unsigned char SOCKS4_BIND = 2;
class Sock4Request{
public:
    SocketAddr client_addr;
    unsigned char version;
    unsigned char command_code;
    SocketAddr dest_addr; // destination address
    std::string userid;

    Sock4Request(){
        version = 4;
        command_code = SOCKS4_ERROR;
    }

    void print(){
        std::cout << "SOCKS4 request\n";
        std::cout << "VN: " << (int)version << ", CD: " << (int)command_code;
        std::cout << ", DST IP: " << dest_addr.ipv4_addr_str << ", DST PORT: " << dest_addr.port_hbytes;
        std::cout << ", USERID: " << userid << "\n";


        std::cout << "Src = ";
        std::cout << client_addr.ipv4_addr_str << "(" << client_addr.port_hbytes << "), ";
        std::cout << "Dst = ";
        std::cout << dest_addr.ipv4_addr_str << "(" << dest_addr.port_hbytes << ")\n";
    }
};

const int SOCKS4_RES_LEN = 8; // 1+1+2+4
const unsigned char SOCKS4_SUCCESS = 90;
const unsigned char SOCKS4_FAILED = 91;
const unsigned char SOCKS4_INETD_FAILED = 92;
const unsigned char SOCKS4_USERID_FAILED = 93;
class Sock4Response{
public:
    unsigned char version;
    unsigned char result_code;
    SocketAddr dest_addr; // destination address

    Sock4Response(){
        version = 0;
        result_code = SOCKS4_ERROR;
    }
    Sock4Response(unsigned char result_code){
        version = 0;
        this->result_code = result_code;
    }

    void to_buf(unsigned char* buf){
        print();

        buf[0] = version;
        buf[1] = result_code;

        uint16_t* port_nbytes_ptr = (uint16_t*)(buf+2);
        uint32_t* ipv4_nbytes_ptr = (uint32_t*)(buf+4);
        dest_addr.get_sockaddr(ipv4_nbytes_ptr, port_nbytes_ptr);
    }

    void print(){
        std::cout << "SOCKS4 response\n";
        std::cout << "VN: " << (int)version << ", CD: " << (int)result_code;
        std::cout << ", DST IP: " << dest_addr.ipv4_addr_str << ", DST PORT: " << dest_addr.port_hbytes;
        std::cout << "\n";
    }
};

void socks4_service(socketfd_t client_socket, SocketAddr client_addr);
void socks4_request_reader_and_parser(Sock4Request& request, socketfd_t connection_socket);
socketfd_t connect_to_app_server(SocketAddr server_addr);
void relay_data_between_2_sockets(socketfd_t client_socket, socketfd_t server_socket);

const char SOCKS4_IP[] = "0.0.0.0";
const int SOCK4_DEFAULT_PORT = 54444;
int main(int argc, char *argv[]){
    SocketAddr socks4_addr(SOCKS4_IP, (uint16_t)SOCK4_DEFAULT_PORT);
    if(argc == 2){
        socks4_addr.port_hbytes = strtol(argv[1], NULL, 0);
    }
    
    /* listening ras first */
    socketfd_t sock4_listen_socket;
    sock4_listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if( sock4_listen_socket < 0 )
        perror_and_exit("create socket error");

    // int on = 1;
    // setsockopt(sock4_listen_socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on));

    if( socket_bind(sock4_listen_socket, socks4_addr) < 0 )
        perror_and_exit("bind error");
    if( listen(sock4_listen_socket, 1) < 0)
        perror_and_exit("listen error");

    start_multiprocess_server(sock4_listen_socket, socks4_service);
    return 0;
}

void socks4_service(socketfd_t client_socket, SocketAddr client_addr){
    /*
     * socks4 server:
     * accept the connection of client now. (fd: client_socket)
     *
     * 1. recieve and parse the socks request
     *    1-1. CONNECT
     *    1-2. BIND
     * 
     * CONNECT:
     *    
     *    2. authentication and check application server can be connected.
     *    3. return socks response.
     *    4. relay data between application client and server.
     *
     * BIND:
     *    
     *    2. [optional] authentication client ip/port.
     *    3. prepare one port for application server to connect.
     *    4. return 1st socks response to client, tell the socket address.
     *    5. wait for application server connection.
     *    6. return 2nd socket response to client, 
     *    7. relay data between application client and server.
     */

    // 1. recieve and parse the socks request
    Sock4Request request;
    request.client_addr = client_addr;
    socks4_request_reader_and_parser(request, client_socket);
    request.print();
    if( request.command_code == SOCKS4_ERROR ){
        std::cout << "error request" << std::endl;
        return;
    }

    if( request.command_code == SOCKS4_CONNECT ){
        // CONNECT:
        std::cout << "recieve CONNECT request" << std::endl;

        // 2. authentication and check application server can be connected.
        // connect to application server
        socketfd_t server_socket = connect_to_app_server(request.dest_addr); 

        // 3. return socks response.
        if( server_socket < 0 ){
            std::cout << "connect to application server failed" << std::endl;
            Sock4Response failed_response(SOCKS4_FAILED);
            unsigned char response_buf[SOCKS4_RES_LEN];

            failed_response.to_buf(response_buf);
            write_all(client_socket, response_buf, SOCKS4_RES_LEN);
            return;
        }

        std::cout << "connect to application server success" << std::endl;
        Sock4Response success_response(SOCKS4_SUCCESS);
        unsigned char response_buf[SOCKS4_RES_LEN];
        success_response.to_buf(response_buf);
        write_all(client_socket, response_buf, SOCKS4_RES_LEN);
        
        // 4. relay data between application client and server.
        relay_data_between_2_sockets(client_socket, server_socket);
        /* connection finish, both end are closing connection */
        close(client_socket);
        close(server_socket);

        return;
    }
    else if( request.command_code == SOCKS4_BIND ){
        std::cout << "recieve BIND request" << std::endl;
    }
}

const int MAX_REQUEST_SIZE = 1<<14;
void socks4_request_reader_and_parser(Sock4Request& request, socketfd_t connection_socket){
    int total_size = 0;
    char request_buf[MAX_REQUEST_SIZE] = {0};
    int userid_index = 0;
    while( 1 ){
        int size = read(connection_socket, request_buf + total_size, MAX_REQUEST_SIZE - total_size);
        if( size < 0 ){
            perror_and_exit("read error");
        }
        else if( size == 0 ){
            return;
        }

        // data doesn't enough
        total_size += size;
        std::cout << "total_size: " << total_size << std::endl;
        std::cout << "request_buf: " << request_buf << std::endl;
        if( total_size < 8 ){
            continue;
        }
        if( userid_index == 0 ){
            userid_index = 8;
        }

        for( ; userid_index < total_size; userid_index++ ){
            if( request_buf[userid_index] == 0 ){
                /* end of userid */
                request.userid = std::string(request_buf+8, userid_index-8);
                break;
            }
        }
        if( userid_index == total_size ){
            /* doesn't achieve the end of request(enter the if statement above) */
            continue;
        }

        request.version = (unsigned char)request_buf[0];
        request.command_code = (unsigned char)request_buf[1];
        uint16_t* port_nbytes_ptr = (uint16_t*)(request_buf+2);
        uint32_t* ipv4_nbytes_ptr = (uint32_t*)(request_buf+4);
        request.dest_addr.set_sockaddr(*ipv4_nbytes_ptr, *port_nbytes_ptr);
        return;
    }
}

socketfd_t connect_to_app_server(SocketAddr server_addr){
    socketfd_t app_server_socket = 0;
    app_server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if( app_server_socket < 0 ){
        perror("create socket error");
        return -1;
    }
    if( socket_connect(app_server_socket, server_addr) < 0 ){
        perror("connect error");
        return -1;
    }
    return app_server_socket;
}

void set_nonblocking_flag(socketfd_t fd){
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

bool is_closed(socketfd_t sock) {
    /*
    fd_set rfd;
    FD_ZERO(&rfd);
    FD_SET(sock, &rfd);
    timeval tv = { 0 };
    select(sock+1, &rfd, 0, 0, &tv);
    if (!FD_ISSET(sock, &rfd))
      return false;

    int n = 0;
    ioctl(sock, FIONREAD, &n);
    return n == 0;
    */
    char x;
    ssize_t r = ::recv(sock, &x, 1, MSG_DONTWAIT|MSG_PEEK);

    std::cout << "r: " << r << std::endl;

    return r == 0;
}

void relay_data_between_2_sockets(socketfd_t client_socket, socketfd_t server_socket){

    if( is_closed(client_socket) ) return;
    if( is_closed(server_socket) ) return;

    set_nonblocking_flag(client_socket);
    set_nonblocking_flag(server_socket);

    /* init */
    fd_set read_fds;
    FD_ZERO(&read_fds);
    int max_fds = 2;

    /* set listening fds */
    FD_SET(client_socket, &read_fds);
    FD_SET(server_socket, &read_fds);

    while( 1 ){
        fd_set select_read_fds = read_fds;
        int select_ret = select(max_fds, &select_read_fds, NULL, NULL, NULL);
        std::cout << "select return\n";

        if( select_ret < 0 ){
             perror_and_exit("select error");
        }
        else if( FD_ISSET(client_socket, &select_read_fds) ){
            /* recieve client and send to server */
            std::string client_data = str::read(server_socket, 1024, true);
            if( client_data.empty() ){
                /* if any socket is closed, then data can be relayed, end of all connection */
                return;
            }

            write_all(server_socket, client_data.c_str(), client_data.size());

            std::cout << "client (size, data): " << client_data.size();
            std::cout << ", " << client_data << std::endl;
        }
        else if( FD_ISSET(server_socket, &select_read_fds) ){
            /* recieve server and send to client */
            std::string server_data = str::read(client_socket, 1024, true);
            if( server_data.empty() ){
                /* if any socket is closed, then data can be relayed, end of all connection */
                return;
            }

            write_all(client_socket, server_data.c_str(), server_data.size());

            std::cout << "server (size, data): " << server_data.size();
            std::cout << ", " << server_data << std::endl;
        }
    }
}
