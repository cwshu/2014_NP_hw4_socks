socks4 protocol
===============
socks4 protocol 是幫 client 跟 application server 間代傳資料的服務, 
client 先連到 socks server, socks server 再連到 application server.

- 使用方式
  
    client 先 send 一個 socks request 給 socks server, 
    如果 socks request 通過的話, socks server 就幫 client 跟 application server 間代傳(reply)資料.

socks request
-------------
2 type: connect and bind

1. connect
    - client 想要連線到 application server, 使用 application server 的服務.

2. bind
    - 送 2 次 response
    - 1st response 的 DST_IP/DST_PORT, 為 socks server 上, 已經準備好給 application server 連線的 socket address.

    - socks server 要確認
        - send bind request 的 client 是否正在連線中
        - 連線到準備好的 socket 上的程式 address 是否是 bind request 寫的 (application server).

authentication
--------------
- src ip/port
- dst ip/port
- user id
- identd (user of client process and OS)

request and response spec
-------------------------
- request
    - VN(1)
        - socks protocol version: 4
    - CD(1)
        - command code: connect is 1, bind is 2
    - DST_PORT(2)
    - DST_IP(4)
    - USERID(variable length)
    - end of NULL char
    - p.s. DST_IP/DST_PORT 就是 application server 的 IP address 跟 port.

- response
    - VN(1)
        - socks protocol version: 0
    - CD(1)
        - result code
            - 90: [success] request granted
            - 91: [failed] request rejected or failed
            - 92: [identd failed] request rejected because SOCKS server cannot connect to identd on the client
            - 93: [userid failed] request rejected because the client program and identd report different user-ids
    - DST_PORT(2)
    - DST_IP(4)

Misc
----
- time limit in CONNECT/BIND
    - 2 mins
