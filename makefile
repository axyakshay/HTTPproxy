client.o:
	gcc -Wall   -o  client  http_client.c
	gcc -Wall   -o  proxy  Proxy_Server.c
clean:
	rm -rf proxy
	rm -rf client
