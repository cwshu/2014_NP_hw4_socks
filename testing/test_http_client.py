"""
https://github.com/Anorov/PySocks

installation:
    pip install PySocks

or in venv:
    python3 -m venv socks
    source socks/bin/activate # or activate.fish, just depend on your shell.
    pip install PySocks
"""

"""
import socket
import socks
s = socks.socksocket()
s.set_proxy(socks.SOCKS4, "127.0.0.1", 54445)
s.connect(("140.113.41.202", 80))
"""

import socks
import socket                                                                                                         
import urllib.request                                                                                                 
socks.set_default_proxy(socks.SOCKS4, "127.0.0.1", 54445)
socket.socket = socks.socksocket                                                                                      
urllib.request.urlopen("http://www.nctu.edu.tw/")  
