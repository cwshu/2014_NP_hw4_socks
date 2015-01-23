#ifndef __SOCKSLIB_H__
#define __SOCKSLIB_H__

#include <iostream>
#include <string>

#include "socket.h"
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

    Sock4Request();
    Sock4Request(SocketAddr client_addr, unsigned char version, unsigned char command_code, SocketAddr dest_addr);
    void to_byte_stream(unsigned char* buf);
    void print();
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

    Sock4Response();
    Sock4Response(unsigned char result_code);
    Sock4Response(unsigned char result_code, SocketAddr& dest_addr);
    void to_buf(unsigned char* buf);
    void print();
};

#endif /* end of include guard: __SOCKSLIB_H__ */
