#include "sockslib.h"

/* Sock4Request method implementations */
Sock4Request::Sock4Request(){
    version = 4;
    command_code = SOCKS4_ERROR;
}

void Sock4Request::print(){
    std::cout << "SOCKS4 request\n";
    std::cout << "VN: " << (int)version << ", CD: " << (int)command_code;
    std::cout << ", DST IP: " << dest_addr.ipv4_addr_str << ", DST PORT: " << dest_addr.port_hbytes;
    std::cout << ", USERID: " << userid << "\n";


    std::cout << "Src = ";
    std::cout << client_addr.ipv4_addr_str << "(" << client_addr.port_hbytes << "), ";
    std::cout << "Dst = ";
    std::cout << dest_addr.ipv4_addr_str << "(" << dest_addr.port_hbytes << ")\n";
}

/* Sock4Response method implementations */
Sock4Response::Sock4Response(){
    version = 0;
    result_code = SOCKS4_ERROR;
}

Sock4Response::Sock4Response(unsigned char result_code){
    version = 0;
    this->result_code = result_code;
}

void Sock4Response::to_buf(unsigned char* buf){
    print();

    buf[0] = version;
    buf[1] = result_code;

    uint16_t* port_nbytes_ptr = (uint16_t*)(buf+2);
    uint32_t* ipv4_nbytes_ptr = (uint32_t*)(buf+4);
    dest_addr.get_sockaddr(ipv4_nbytes_ptr, port_nbytes_ptr);
}

void Sock4Response::print(){
    std::cout << "SOCKS4 response\n";
    std::cout << "VN: " << (int)version << ", CD: " << (int)result_code;
    std::cout << ", DST IP: " << dest_addr.ipv4_addr_str << ", DST PORT: " << dest_addr.port_hbytes;
    std::cout << "\n";
}
