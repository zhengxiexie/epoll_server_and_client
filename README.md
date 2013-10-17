This is a client and server sample code for a complete TCP/IP communication using epoll.

You can compile the 2 files:

gcc client.c -o client -lpthread -ggdb

gcc server.c -o server -lpthread -ggdb

adn run:
./server 6666
and on another computer run:
./client your_server_ip
