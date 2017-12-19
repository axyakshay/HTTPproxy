Implementation of HTTP Command Line Client and HTTP Proxy based on HTTP-1.0 protocol
Team number 21
Team Members: Akshay Gajanan Hasyagar, Karthikeyan Kathiresan


To run the program:
1)Run makefile-  make 
2)Run Proxy    ./proxy <ip_address> <port>
3)Run Client   ./client <proxy address> <proxy port> <URL to retrieve> 

************************************** Please enter the URL starting with www. *******************************************************

#Proxy_Server.c - Based on HTTP-1.0 protocol. It establishes the proxy server, connects with all the client and responds to 
their request by either fetching url or getting it from cache and sending it back.
It determines if cache page is fresh by date in the Expires, Last-Modified, or Date header in conjunction
with an If-Modified-Since message header line in a GET request(Conditional GET - 304 response)

#http_client.c - This is the client file, which issues a HTTP request to proxy server and saves the received webpage.
