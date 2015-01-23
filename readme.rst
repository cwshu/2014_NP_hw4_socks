NP hw4 - SOCKS4 server and client support SOCKS4 protocol
---------------------------------------------------------

project directorys
------------------
- makefile
 
  - build script of make (gnu make)

- src/ 
  
  - all source codes of project(server and client)

- note/

  - specification of SOCKS4, SOCKS4a, SOCKS5 protocol.

  - SOCKS4 protocol notes

  - hw4 specification

- testing/

  - test_http_client.py

    - HTTP client support SOCKS4 protocol, use for testing socks4d(server).

  - run_cgi.py

    - the client support CGI protocol, use for testing request_server.cgi(cgi program).

  - form_for_request_server_cgi.html

    - html form for request_server.cgi

  - {socks4d, request_server_cgi}.gdb_cmds

    - gdb initialization commands of debugging 2 program, ``gdb -x $(program).gdb_cmds`` to use it.

  - batch_files/

    - testing files for request_server.cgi
