#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <regex.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#define SIZEBUF 4194304


int main(int argc,char *argv[])
{

FILE *filename;
int sockfd,connectfd,port,readbytes=1, i=0, j=0,k=0;
struct sockaddr_in source_address;
struct hostent *server;
char name_domain[1000], message_buff[1500], buffer_receiver[512], Document[1000], URL[2000];

if (argc != 4){        
		printf(" usage: ./client <proxy address> <proxy port> <URL to retrieve>\n");
        exit(1);
}

if((sockfd = socket(AF_INET, SOCK_STREAM,0)) <0){        
		printf( "ERROR: Socket creation error \n");
        exit(1);
}

else {
        port = atoi(argv[2]);
        strcpy(URL,argv[3]);
        server = gethostbyname(argv[1]);
}
if (server ==NULL)
 {
 printf("Host does not exist\n");
 exit(1);
}

printf("The input URL is  %s \n", argv[3]);

while ( URL[i] != '/') {   	//extract the hostname 
	name_domain[k++] = URL[i++];
}

i++;

while (i < strlen(URL) ) //Extract the resource/document name
         {    Document[j++] = URL[i++];
         }
printf("Domain name: %s\n",name_domain);
printf("Document: %s\n",Document);

bzero((char *)&source_address, sizeof(source_address));
bcopy ((char *)server->h_addr,(char *)&source_address.sin_addr.s_addr,server->h_length);
source_address.sin_port = htons(port);
source_address.sin_family = AF_INET;

if ((connectfd = connect(sockfd, (struct sockaddr*) &source_address, sizeof(source_address))) < 0) {
        printf("Error Connecting\n");
        exit(1);
}

sprintf(message_buff, "GET /%s HTTP/1.0\r\nHost: %s\r\nUser-Agent: HTMLGET 1.0\r\n\r\n",Document,name_domain);

printf("%s\n",message_buff);

if (send(sockfd ,message_buff,sizeof(message_buff),0) == -1){
	fprintf(stderr, "error in sending\n");
    exit(1);
}

filename=fopen("recvfile","w+");

while(readbytes!=0){ //receive data from proxy and save it in my local directory.
    readbytes = recv(sockfd, buffer_receiver, sizeof(buffer_receiver), 0);
    fwrite(buffer_receiver, 1,  sizeof(buffer_receiver) , filename);
}

fclose(filename);
close(sockfd);
return(1);

}
