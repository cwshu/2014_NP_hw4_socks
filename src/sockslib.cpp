#include "sockslib.h"

/* Sock4Request method implementations */
Sock4Request::Sock4Request(){
    version = 4;
    command_code = SOCKS4_ERROR;
}

Sock4Request::Sock4Request(SocketAddr client_addr, unsigned char version, unsigned char command_code, SocketAddr dest_addr){
    this->client_addr = client_addr;   
    this->version = version;   
    this->command_code = command_code;   
    this->dest_addr = dest_addr;   
}

void Sock4Request::to_byte_stream(unsigned char* buf){
    buf[0] = version;
    buf[1] = command_code;
    *(uint16_t*)(buf+2) = htons(dest_addr.port_hbytes);
    *(uint32_t*)(buf+4) = ip_string_to_nbyte(dest_addr.ipv4_addr_str);
    buf[8] = '\0';
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

Sock4Response::Sock4Response(unsigned char result_code, SocketAddr& dest_addr){
    version = 0;
    this->result_code = result_code;
    this->dest_addr = dest_addr;
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
