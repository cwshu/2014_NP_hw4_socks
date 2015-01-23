#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstdint>
#include <vector>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>

#include "socket.h"
#include "server_arch.h"
#include "io_wrapper.h"
#include "sockslib.h"
#include "string_more.h"

struct SingleSocks4FirewallRule: public IPv4AddressSet{
    char kind;
    bool is_permit;

    SingleSocks4FirewallRule(){
        kind = '\0'; // NONE;
        is_permit = false;
    }
};

struct Socks4FirewallRules{
    std::vector<SingleSocks4FirewallRule> ip_permit_rules;

    bool is_ip_pass_rule(uint32_t ip_nbyte, char kind);
    void add_rule_at_last(SingleSocks4FirewallRule& rule);
    void print_rules();
};

void socks4_service(socketfd_t client_socket, SocketAddr& client_addr);
void socks4_request_reader_and_parser(Sock4Request& request, socketfd_t connection_socket);
socketfd_t connect_to_app_server(SocketAddr server_addr);
void relay_data_between_2_sockets(socketfd_t client_socket, socketfd_t server_socket);

const char SPILT_CHARS[] = " ";
void open_and_parse_firewall_rule(Socks4FirewallRules& firewall_rules, std::string filename);
void set_nonblocking_flag(socketfd_t fd);

const char SOCKS4_IP[] = "0.0.0.0";
const int SOCK4_DEFAULT_PORT = 54444;
const char SOCKS4_CONF_FILE[] = "socks.conf";

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

/* struct Socks4FirewallRules */
bool Socks4FirewallRules::is_ip_pass_rule(uint32_t ip_nbyte, char kind){
    for( auto ip_permit_rule : ip_permit_rules ){
        if( ip_permit_rule.kind != kind ){
            continue;
        }

        if( ip_permit_rule.is_belong_to_set(ip_nbyte) ){
            return ip_permit_rule.is_permit;
        }
    }
    return false;
}

void Socks4FirewallRules::add_rule_at_last(SingleSocks4FirewallRule& rule){
    ip_permit_rules.push_back(rule);
}

void Socks4FirewallRules::print_rules(){
    for (auto ip_permit_rule : ip_permit_rules) {
        if( ip_permit_rule.is_permit )
            std::cout << "permit ";
        else
            std::cout << "deny ";

        std::cout << ip_permit_rule.kind;
        std::cout << " " << ip_permit_rule.to_str() << std::endl;
    }
}

/* functions */
void socks4_service(socketfd_t client_socket, SocketAddr& client_addr){
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

    // processing: read setting files
    Socks4FirewallRules firewall;
    open_and_parse_firewall_rule(firewall, SOCKS4_CONF_FILE);
    firewall.print_rules();

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
        // std::cout << "recieve CONNECT request" << std::endl;
        uint32_t ip_nbyte = ip_string_to_nbyte(request.dest_addr.ipv4_addr_str);
        if( firewall.is_ip_pass_rule(ip_nbyte, 'c') ){
            std::cout << "SOCKS_CONNECT GRANTED ...." << std::endl;
        }
        else{
            std::cout << "SOCKS_CONNECT DENY ...." << std::endl;
            return;
        }

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

        Sock4Response success_response(SOCKS4_SUCCESS);
        unsigned char response_buf[SOCKS4_RES_LEN];
        success_response.to_buf(response_buf);
        write_all(client_socket, response_buf, SOCKS4_RES_LEN);

        // 4. relay data between application client and server.
        relay_data_between_2_sockets(client_socket, server_socket);
        /* connection finish, both end are closing connection */
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
        // std::cout << "total_size: " << total_size << std::endl;
        // std::cout << "request_buf: " << request_buf << std::endl;
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

void relay_data_between_2_sockets(socketfd_t client_socket, socketfd_t server_socket){

    set_nonblocking_flag(client_socket);
    set_nonblocking_flag(server_socket);

    /* init */
    fd_set read_fds;
    FD_ZERO(&read_fds);
    int max_fds = 0;

    /* set listening fds */
    FD_SET(client_socket, &read_fds);
    max_fds = (client_socket > max_fds) ? client_socket : max_fds;
    FD_SET(server_socket, &read_fds);
    max_fds = (server_socket > max_fds) ? server_socket : max_fds;

    max_fds++;

    while( 1 ){
        fd_set select_read_fds = read_fds;
        int select_ret = select(max_fds, &select_read_fds, NULL, NULL, NULL);
        // std::cout << "select return\n";

        if( select_ret < 0 ){
             perror_and_exit("select error");
        }
        else if( FD_ISSET(client_socket, &select_read_fds) ){
            /* recieve client and send to server */
            std::string client_data = str::read(client_socket, 1024, true);
            if( client_data.empty() ){
                /* if any socket is closed, then data can be relayed, end of all connection */
                return;
            }

            write_all(server_socket, client_data.c_str(), client_data.size());

            std::cout << "<Content: client> size: " << client_data.size();
            if( client_data.size() > 10 ){
                std::cout << "\n" << client_data.substr(0, 10) << " ... " << std::endl;
            }
            else{
                std::cout << "\n" << client_data << std::endl;
            }
        }
        else if( FD_ISSET(server_socket, &select_read_fds) ){
            /* recieve server and send to client */
            std::string server_data = str::read(server_socket, 1024, true);
            if( server_data.empty() ){
                /* if any socket is closed, then data can be relayed, end of all connection */
                return;
            }

            write_all(client_socket, server_data.c_str(), server_data.size());

            std::cout << "<Content: server> size: " << server_data.size();
            if( server_data.size() > 10 ){
                std::cout << "\n" << server_data.substr(0, 10) << " ... " << std::endl;
            }
            else{
                std::cout << "\n" << server_data << std::endl;
            }
        }
    }
}

void open_and_parse_firewall_rule(Socks4FirewallRules& firewall_rules, std::string filename){
    /*
     * setting file format:
     * [permit|deny] [c|b] [CIDR]
     * ex.
     * permit        c     140.113.0.0/24
     */
    std::fstream setting_file;
    setting_file.open(filename, std::ios::in);

    while( 1 ){
        /* one_line_rule will be each line in setting_file
         */
        std::string one_line_rule;
        if( !std::getline(setting_file, one_line_rule) ){
            break;
        }
        std::string one_line_rule_copy = one_line_rule;

        std::string permit_str = fetch_word(one_line_rule, SPILT_CHARS);
        std::string command_code_str = fetch_word(one_line_rule, SPILT_CHARS);
        std::string CIDR_ip_str = fetch_word(one_line_rule, "/");
        std::string CIDR_netmask_str = fetch_word(one_line_rule, SPILT_CHARS);

        SingleSocks4FirewallRule this_rule;
        this_rule.start_ip_nbyte = ip_string_to_nbyte(CIDR_ip_str);
        this_rule.netmask = std::stoi(CIDR_netmask_str);

        bool is_permit;
        char command_code;
        if( permit_str == "permit" )
            is_permit = true;
        else if( permit_str == "deny" )
            is_permit = false;
        else{
            std::cout << "[socks.conf] syntax error" << one_line_rule_copy << std::endl;
            continue; /* syntax error */
        }
        if( command_code_str == "c" || command_code_str == "b" )
            command_code = command_code_str[0];
        else{
            std::cout << "[socks.conf] syntax error" << one_line_rule_copy << std::endl;
            continue; /* syntax error */
        }

        this_rule.is_permit = is_permit;
        this_rule.kind = command_code;

        firewall_rules.add_rule_at_last(this_rule);
    }

    setting_file.close();
}

void set_nonblocking_flag(socketfd_t fd){
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
