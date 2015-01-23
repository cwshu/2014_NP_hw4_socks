#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <cstring>

#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <fcntl.h>

#include "socket.h"
#include "io_wrapper.h"
#include "httplib.h"
#include "sockslib.h"

class Request{
public:
    SocketAddr req_server_addr;
    std::string batch_file;
    int id;

    bool is_server_connect;
    socketfd_t connected_server_fd;
    bool is_batch_file_open;
    std::fstream batch_file_stream;
    /* support request over socks server */
    bool is_use_socks;
    SocketAddr socks_server_addr;

    Request();
    Request(const std::string& host, int port, const std::string& batch_file, int id);
    Request(const Request& copy);
    void connect_server_and_initial(bool is_nonblocking);
    void send_socks_request();
    void open_batch_file();
    bool send_batch_file_data_to_server_once();
    std::string read_server_response(int count, bool is_nonblocking);

    Request& operator= (const Request& copy);
};

namespace cgi{
    std::map<std::string, std::string> http_get_parameters();
}

void print_http_header();
void print_html_before_content(const std::vector<Request>& all_requests);
void print_html_content(int id, std::string msg);
void print_html_after_content();

int main(int argc, char *argv[]){
    /*
     * 1. parse env QUERY_STRING
     * 2. connect to server, and print response
     *    2.1. connect to server (non-blocking)
     *    2.2. read data from batch file, and send data as input to server
     *    2.3. get response from server, and print as html_version to stdout
     */

    /*
     * 1. parse env QUERY_STRING
     *    
     * data structure transformation
     *   env[QUERY_STRING]: string with key/value pair data
     *   query_parameters: map (key/value data structure)   
     *   all_requests: vector of every Request (key/value pair of hi, pi, and fi, (shi, spi))
     */
    std::map<std::string, std::string> query_parameters;
    query_parameters = cgi::http_get_parameters();

    std::vector<Request> all_requests;
    for( int i = 1; i <= 5; i++ ){
        std::string host_name = "h" + std::to_string(i);
        std::string port_name = "p" + std::to_string(i);
        std::string batch_file_name = "f" + std::to_string(i);
        std::string socks_host_name = "sh" + std::to_string(i);
        std::string socks_port_name = "sp" + std::to_string(i);

        if( query_parameters.count(host_name) > 0 ){
            Request this_request;

            std::string host = query_parameters[host_name];
            int port_hbytes = std::stoi(query_parameters[port_name]);
            std::string batch_file = query_parameters[batch_file_name];

            this_request = Request(host, port_hbytes, batch_file, all_requests.size());

            if( query_parameters.count(socks_host_name) ){
                std::string socks_host = query_parameters[socks_host_name];
                int socks_port_hbytes = std::stoi(query_parameters[socks_port_name]);

                this_request.is_use_socks = true;
                this_request.socks_server_addr = SocketAddr(socks_host, socks_port_hbytes);

                /* std::cout << "Request: " 
                          << host << " " << port_hbytes << " " << batch_file << " " 
                          << socks_host << " " << socks_port_hbytes << std::endl; */
            }
            all_requests.push_back(this_request);
        }
    }

    std::fstream err_log;
    err_log.open("cgi_error.log", std::fstream::out);
    for( const auto& req: all_requests ){
        err_log << req.req_server_addr.ipv4_addr_str << std::endl;
        err_log << req.req_server_addr.port_hbytes << std::endl;
        err_log << req.batch_file << std::endl;
    }

    /* part2 */

    /* initialization of every Request
     * 1. make an connection
     * 2. open batch_file
     */

#if 0
    // single connection
    Request req = all_requests[0];

    req.connect_server_and_initial(false);
    req.open_batch_file();
    req.send_batch_file_data_to_server();

    print_html_before_content(all_requests);

    while( 1 ){
        // req.send_batch_file_data_to_server_once();
        std::string msg;
        msg = req.read_server_response(1024, false);
        if( msg.empty() ){
            close(req.connected_server_fd);
            break;
        }
        nl2br(msg);
        print_html_content(req.id, msg);
    }

    print_html_after_content();
#endif

    /* std::cout << "this request: " 
              << all_requests[0].req_server_addr.ipv4_addr_str << " " 
              << all_requests[0].req_server_addr.port_hbytes << " " 
              << all_requests[0].batch_file << std::endl 
              << "use socks? " << all_requests[0].is_use_socks << ", "
              << all_requests[0].socks_server_addr.ipv4_addr_str << " " 
              << all_requests[0].socks_server_addr.port_hbytes << std::endl; */

    int request_num = all_requests.size();

    for( auto& req: all_requests ){
        req.open_batch_file();
        req.connect_server_and_initial(true);
    }

    /* set (read_fds, max_fd) for [ connected_server_fd in all_requests ] */
    fd_set read_fds;
    int max_fd = -1;
    FD_ZERO(&read_fds);
    for( const auto& req: all_requests ){
        if( req.is_server_connect ){
            FD_SET(req.connected_server_fd, &read_fds);
            if( max_fd < req.connected_server_fd ){
                max_fd = req.connected_server_fd;
            }
        }
    }
    max_fd += 1;

    /* send data to/recv from server and print out message in html format */
    err_log << "request_num start:" << request_num << std::endl;

    print_http_header();
    print_html_before_content(all_requests);

    while( request_num > 0 ){
        fd_set select_read_fds = read_fds;
        int s_ret = 0;
        s_ret = select(max_fd, &select_read_fds, NULL, NULL, NULL);

        err_log << "\nSELECT return: " << s_ret << std::endl;
        err_log << "request_num:" << request_num << std::endl;

        if( s_ret < 0 )
            perror_and_exit("select error");

        for( auto& req: all_requests ){
            if( FD_ISSET(req.connected_server_fd, &select_read_fds) ){

                err_log << "fd " << req.connected_server_fd << " can be read\n";

                std::string msg = req.read_server_response(1024, true);

                err_log << "msg: " << msg << "\n";

                if( msg.empty() ){
                    /* request server has no more response, it closes connection */
                    err_log << "FD_CLR: " << req.req_server_addr.ipv4_addr_str << std::endl;

                    FD_CLR(req.connected_server_fd, &read_fds);

                    err_log << "FD_CLR finish: " << req.req_server_addr.port_hbytes << std::endl;

                    close(req.connected_server_fd);
                    req.is_server_connect = false;
                    request_num--;
                    err_log << "request_num after clear:" << request_num << std::endl;
                    continue;
                }
                nl2br(msg);
                print_html_content(req.id, msg);

                // write to socket.
                req.send_batch_file_data_to_server_once();
            }
        }
    }

    err_log << "end of requests\n";

    print_html_after_content();

    err_log << "print_html_after_content() finish\n";

    err_log.close();
    return 0;
}

/* class Request */
Request::Request(){
    is_server_connect = false;
    is_batch_file_open = false;
    is_use_socks = false;
}

Request::Request(const std::string& host, int port, const std::string& batch_file, int id){
    this->req_server_addr = SocketAddr(host, port);
    this->batch_file = batch_file;
    this->id = id;

    is_server_connect = false;
    is_batch_file_open = false;
    is_use_socks = false;
}

Request::Request(const Request& copy){
    *this = copy;
}

Request& Request::operator= (const Request& copy){
    if( this != &copy ){
        this->req_server_addr = copy.req_server_addr;
        batch_file = copy.batch_file;
        id = copy.id;

        is_server_connect = false;
        is_batch_file_open = false;

        is_use_socks = copy.is_use_socks;
        socks_server_addr = copy.socks_server_addr;
    }
    return *this;
}

void Request::connect_server_and_initial(bool is_nonblocking){
    /* connect server */
    SocketAddr connect_addr = req_server_addr;
    if( is_use_socks ){
        connect_addr = socks_server_addr;
    }

    connected_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if( connected_server_fd < 0 )
        perror_and_exit("create socket error");
    if( socket_connect(connected_server_fd, connect_addr) < 0 )
        perror_and_exit("connect to " + connect_addr.ipv4_addr_str + ":" + std::to_string(connect_addr.port_hbytes) + " error");
    is_server_connect = true;

    /* initialization */
    /* if using socks4 protocol, send socks4 request and receive response */
    if( is_use_socks ){
        Sock4Request socks4_request(socks_server_addr, 4, SOCKS4_CONNECT, req_server_addr);
        std::cout << "req_server_addr: " << req_server_addr.ipv4_addr_str << " " 
                  << req_server_addr.port_hbytes << std::endl;
        unsigned char buf[1024]; 
        socks4_request.to_byte_stream(buf);

        // send socks4 request
        write_all(connected_server_fd, buf, 9); // len of sock4 request = 9 without userid

        // recv socks4 response
        int read_size = 0;
        while(read_size < 8){
            int size = read(connected_server_fd, buf, 8-read_size);
            if( size < 0 ){
                perror_and_exit("read error");
            }
            read_size += size;
        }
        int command_code = buf[1];
        if( command_code != SOCKS4_SUCCESS ){
            is_server_connect = false;
            perror_and_exit("socks connection error");
        }
    }

    if( is_nonblocking ){
        int flags = fcntl(connected_server_fd, F_GETFL, 0);
        fcntl(connected_server_fd, F_SETFL, flags | O_NONBLOCK);
    }
}

void Request::open_batch_file(){
    /* open batch_file */
    batch_file_stream.open(batch_file, std::fstream::in);
    if( !batch_file_stream ){
        perror_and_exit("open batch_file " + batch_file + " error");
    }

    is_batch_file_open = true;
}

bool Request::send_batch_file_data_to_server_once(){
    if( !is_batch_file_open )
        return false;
    if( !is_server_connect )
        return false;

    std::string command;
    if( std::getline(batch_file_stream, command) ){
        command += "\n";
        int w_size = write_all(connected_server_fd, command.c_str(), command.length());
        if( w_size < 0 ){
            perror_and_exit("write error");
        }
        nl2br(command);
        print_html_content(id, "<b>" + command + "</b>");
        return true;
    }
    batch_file_stream.close();
    is_batch_file_open = false;
    return false;
}

std::string Request::read_server_response(int count, bool is_nonblocking){
    if( !is_server_connect )
        return "";

    std::string msg = str::read(connected_server_fd, count, is_nonblocking);
    return msg;
}

namespace cgi{
    /* cgi-related api */
    std::map<std::string, std::string> http_get_parameters(){
        std::string query_string = getenv("QUERY_STRING");
        std::vector<std::string> parameter_vector;

        while( 1 ){
            /* bad smell string split, but it works */
            std::size_t spliter = query_string.find_first_of('&');
            if( spliter == std::string::npos ){
                parameter_vector.push_back(query_string);
                break;
            }

            std::string parameter = query_string.substr(0, spliter);
            query_string = query_string.substr(spliter+1, std::string::npos);
            parameter_vector.push_back(parameter);
        }

        std::map<std::string, std::string> query_parameters;
        for( const auto& parameter: parameter_vector ){
            std::size_t spliter = parameter.find_first_of('=');
            if( spliter == std::string::npos ){
                continue;
            }
            std::string key = parameter.substr(0, spliter);
            std::string value = parameter.substr(spliter+1, std::string::npos);
            if( value.empty() )
                continue;

            query_parameters[key] = value;
        }

        return query_parameters;
    }

}

void print_http_header(){
    std::cout << "Content-Type: text/html\r\n";
    std::cout << "\r\n";
}

void print_html_before_content(const std::vector<Request>& all_requests){
    int request_num = all_requests.size();

    std::string output;
    output += "<html>";
    output += "<head>";
    output += "    <meta http-equiv=\"Content-Type\" content=\"text/html; charset=big5\" />";
    output += "    <title>Network Programming Homework 3</title>";
    output += "</head>";
    output += "<body bgcolor=#336699>";
    output += "    <font face=\"Courier New\" size=2 color=#FFFF99>";
    output += "        <table width=\"800\" border=\"1\">";
    output += "            <tr>";

    for( int i = 0; i < request_num; i++ ){
         output += "<td>" + all_requests[i].req_server_addr.ipv4_addr_str + "</td>";
    }

    output += "            </tr>";
    output += "            <tr>";

    for( int i = 0; i < request_num; i++ ){
         output += "<td valign=\"top\" id=\"m" + std::to_string(i) + "\"></td>";
    }

    output += "            </tr>";
    output += "        </table>";
    output += "    </font>";

    std::cout << output;
}

void print_html_content(int id, std::string msg){
    std::cout << "<script>document.all['m" + std::to_string(id) + "'].innerHTML += '" + msg + "';</script>";
}

void print_html_after_content(){
    std::string output;
    output += "</body>";
    output += "</html>";
    std::cout << output;
}

