#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <assert.h>

#define size_of_cache 10
#define SIZEOFBUFFER 2048000

#define add 0
#define delete 2
#define check 1
#define Update 10

fd_set fds_master, readfds;
int cache_entry_num = 0;

struct demand_line { //Request line format
	char * address;
	char * document;
	time_t Expiry;
	struct tm lastmodified;
	struct tm* lastaccessed;
};

typedef struct demand_line demandline;

struct cache { //Cache structure
	struct cache *previous;
	struct cache *next;
	demandline demand;	
};

typedef struct cache cache;
cache *front = NULL;

void convert(char * input) { //Format input
	char* new = input, *prev = input;
	
	while (*prev != 0) {
		*new = *prev++;
		if (*new != ' ') new++;
	}
	*new = 0;
}

demandline Decode(char* input) { //Decode the input web request
	
	char *token;
	token = strtok(strdup(input), "\r\n");
	demandline demand;
	
	while (token != NULL)
	{
		char *temp = malloc(sizeof(char) * strlen(token));
		temp = token;
		if (strncmp(temp, "GET", 3) == 0) {
			char * temp2 = temp + 4;
			temp2[strlen(temp2) - 8] = '\0';
			demand.document = temp2;
		}
		else if (strncmp(temp, "Host: ", 5) == 0) 
			demand.address = temp + 6;
		token = strtok(NULL, "\r\n");
	}
	demand.Expiry = 0;
	convert(demand.address);
	convert(demand.document);
	return demand;

}

int Cache_check(int flag, demandline request) { //Perform the required operation on cache
	
	cache *currentpage, *temppage1, *temppage2;
	int counter;

	if (flag == add){	//Add an entry to cache
		
		currentpage = (cache*)malloc(sizeof(cache));
		
		currentpage->demand.document = request.document;
		currentpage->demand.address = request.address;
		currentpage->demand.lastmodified= request.lastmodified;
		currentpage->demand.Expiry = request.Expiry;
		
		time_t timenow;
		time(&timenow);
		struct tm *tim = gmtime(&timenow);
		currentpage->demand.lastaccessed = tim; 
		
		if (front == NULL) {	//If no entry is present
			currentpage->previous = NULL;
			currentpage->next = NULL;
			front = currentpage;
		}
		else { 
			front->previous = currentpage;
			currentpage->previous = NULL;
			currentpage->next = front;
			front = currentpage;
		}
		cache_entry_num++;
		printf ("Entry num is %d \n", cache_entry_num);
		
		if (cache_entry_num == size_of_cache + 1) { //LRU eviction
			
			currentpage = front;
			for (counter = 0; counter < size_of_cache - 1; counter++)
				currentpage = currentpage->next;
			currentpage->next = NULL;
			cache_entry_num = size_of_cache;
			printf("LRU Item evicted from Cache \n");
		}
		printf("Item added to Cache \n");
		return 0;
	}
	else if (flag == delete){ //Delete an entry from cache
		printf("Adding this entry at the last. (Refresh). Delete followed by add \n");
	
		currentpage = front;
		while (currentpage != NULL) {
			if (!strcmp(currentpage->demand.document, request.document) && !strcmp(currentpage->demand.address, request.address)) {
				
				temppage1 = currentpage->previous;
				temppage2 = currentpage->next;
				
				if (temppage1 != NULL)
					temppage1->next = temppage2; 
				else 
					front = temppage2;
				
				if (temppage2 != NULL)
					temppage2->previous = temppage1;
				
				cache_entry_num--;
				printf("Item deleted from Cache \n");
				break;
			}
			currentpage = currentpage->next;
		}
		
	}
 
    else if (flag == check){	//Check if its a cache hit or miss
		
		currentpage = front;
		while (currentpage != NULL) {
			if (!strcmp(currentpage->demand.address, request.address) && !strcmp(currentpage->demand.document, request.document))	//To find the cache entry
			{
				time_t present;
				time(&present);
				char * buff = malloc(100 * sizeof(char));
				
				struct tm *tim = gmtime(&present);
				//printf("Present time is %lld\n",mktime(tim));
				strftime(buff, 80, "%a, %d %b %Y %H:%M:%S %Z", tim);
				
				printf("Present time is: %s\n", buff);
				printf("Item present in Cache\n");
			    
				if (currentpage->demand.Expiry ==  0){  //if Expired header is missing
					printf ("No Expiry header \n");
					
					if (currentpage->demand.lastmodified.tm_mon >= 0){ //If Last-Modified header present
						if (!(tim->tm_mday - currentpage->demand.lastaccessed->tm_mday) && !(tim->tm_mon - currentpage->demand.lastmodified.tm_mon)){
							printf ("Not expired \n");
							return 0;
						}
						else{
							printf ("Expired \n");
							return 1;
						}
					}
					else{												//If Last-Modified header absent
						printf("No Last modified header as well \n");
						if (!(tim->tm_mday - currentpage->demand.lastaccessed->tm_mday)){
							printf ("Not expired \n");
							return 0;
						}
						else{
							printf ("Expired \n");
							return 1;
						}
					}
				}
				
				if (mktime(tim) > currentpage->demand.Expiry) {			//If expired header present
					printf("Expired \n");
					return 1;
				}
				else { 
					printf("Not expired \n");
					return 0;
				}
			}
			currentpage = currentpage->next;
		}
		return 2; 

	}
	
	else if (flag == Update){	//update an entry in the cache
	    currentpage=front;
		while (currentpage != NULL) {
			if (!strcmp(currentpage->demand.address, request.address) && !strcmp(currentpage->demand.document, request.document)) // Cache Hit
			{
				time_t present2;
				time(&present2);
				struct tm *timeupd = gmtime(&present2);
				currentpage->demand.lastaccessed = timeupd;
			}
			currentpage = currentpage->next;
		}
	}
	else
		return 1;
	
}


void Send(int clifd, demandline  demand) {	//Send data to client
	
	char Doc[strlen(demand.document)];
	char buffer[512];
	int k;
	char * file = malloc(strlen(demand.address) + strlen(demand.document) + 1);
	
	strcpy(Doc, demand.document);
	strcpy(file, demand.address);
	
	for (k = 0; k < strlen(Doc); k++) {
		if (!strncmp(Doc + k, "/", 1)) 
			 strncpy(Doc + k, "_", 1);
	}
	strcat(file, Doc);
	convert(file);
	FILE *tempfile = fopen(file, "rb");
	
	if (tempfile != NULL) 
		while (fread(buffer, sizeof(buffer), 1, tempfile)>0) 
			if ((send(clifd, buffer, sizeof(buffer), 0)) == -1) {
				printf("Error : Send error");
				exit(1);
			}
			
	FD_CLR(clifd, &fds_master);
	close(clifd);
}


void receive_server(int webserverfd, demandline demand, int clifd, int flag) { //receive  from server
	
	char * file_name = malloc(strlen(demand.address) + strlen(demand.document) + 1);
	char document[strlen(demand.document)], content[SIZEOFBUFFER], string_temp[10];
	int nbytes, i, expire_check = 0 , webcode_check = 0;
	char *Expiry = NULL, *webcode= NULL, *lastmodified = NULL, *temp_webcode = NULL, *temp_content = NULL;
	
	strcpy(file_name, demand.address);
	strcpy(document, demand.document);
	for (i = 0; i<strlen(document); i++) {
		if (strncmp(document + i, "/", 1) == 0) 
			strncpy(document + i, "_", 1);
	}
	strcat(file_name, document);
	convert(file_name);

	sprintf(string_temp, "tempfile%d", clifd); 
	FILE *tempfile = fopen(string_temp, "wb");
	
	memset(content, 0, SIZEOFBUFFER);
	
	while ((nbytes = recv(webserverfd, content, SIZEOFBUFFER, 0)) > 0) {	
		if (webcode_check == 0) { //extract the webcode
			webcode_check = 1;
			temp_webcode = strtok(strdup(content), "\r\n");
			*(temp_webcode + 12) = '\0';
			webcode = temp_webcode + 9;	
		}
		if (expire_check == 0) { //Extract the Last-Modified and Expires field
			temp_content = strtok(strdup(content), "\r\n");
			while (temp_content != NULL) {
				if (strncmp(temp_content, "Last-Modified: ", 15) == 0) {
					lastmodified = temp_content + 15;
				}
				if( (strncmp(temp_content, "Expires: ", 9) == 0) || (strncmp(temp_content, "expires: ", 9) == 0)){
					expire_check = 1;
					Expiry = temp_content + 9;
					break;
				}
				temp_content = strtok(NULL, "\r\n");
			}
		}
		
		fwrite(content, 1, nbytes, tempfile);
		memset(content, 0, SIZEOFBUFFER);
	}

	fclose(tempfile);
	
	struct tm tim;
	bzero((void *)&tim, sizeof(struct tm));
	printf("Expiry date of page is : %s\n", Expiry);
	
	
	if (strcmp(webcode, "304") != 0) { //Saving the extracted value to request structure (Only if response ! = 304)
		
		if (Expiry != NULL && strcmp(Expiry, "-1") != 0) { 
			strptime(Expiry, "%a, %d %b %Y %H:%M:%S %Z", &tim);
			demand.Expiry = mktime(&tim);
		}
		else if (lastmodified != NULL && strcmp(lastmodified, "-1") != 0) {
			printf("Last modified date of page is : %s\n", lastmodified);
			strptime(lastmodified, "%a, %d %b %Y %H:%M:%S %Z", &tim);
			demand.lastmodified = tim;
			demand.Expiry=0;
		}
		else{
			demand.Expiry=0;
			demand.lastmodified.tm_mon=-1;
		}
    }
	close(webserverfd);
	printf("Response from webserver %s\n", webcode);
	
	if (nbytes < 0) {
		perror("Receive Error!\n");
		exit(0);
	}
	else {	//Data from server . Update Cache entry and save the file
		if (strcmp(webcode, "304") != 0) { //Only if a non-304 code is received, the file is saved.
			if (access(file_name, F_OK) != -1)
				if (remove(file_name) != 0) 
					printf("Error: File can't be deleted \n"); 
			if (rename(string_temp, file_name) != 0) 
				printf("Error: File can't be renamed\n");
		}
		if (strcmp(webcode, "304") == 0)
			Cache_check(Update, demand);
		
		if (strcmp(webcode, "304") != 0) {  //Delete and add because it deletes the accessed value from any position and adds it to the last to make it most recently used
			if (flag == 0) 
				Cache_check(delete, demand);
			if (flag == 1)
				Cache_check(delete, demand);
			Cache_check(add, demand);
		}
		
		Send(clifd, demand);
		printf("Transferred page to the client \n\n");
		printf("#####################Complete########################\n");
	}
}


void StatusCheck(demandline demand, int clifd, int flag) { //Check the expiry status and perform operations accordingly.
	
	struct addrinfo *p , *servinfo, hints;
	int webserverfd, resolution_fd ;
	char *document = demand.document, *Message, *format;
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	
	if ((resolution_fd = getaddrinfo(demand.address, "80", &hints, &servinfo)) != 0) {
		printf("Error: Getaddrinfo - Name resolution can't be done \n");
		exit(1);
	}

	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((webserverfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("Error : Socket can't be created");
			continue;
		}
		if (connect(webserverfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(webserverfd);
			perror("Error: Connect error");
			continue;
		}
		break;
	}
	freeaddrinfo(servinfo);
	
	if (document == NULL) document = "";
	else if (*document == '/') document++;
	
	cache *iterator = front;
	while (iterator != NULL) { 
		if (!strcmp(iterator->demand.address, demand.address) && !strcmp(iterator->demand.document, demand.document)) 
			break;
		iterator = iterator->next;
	}
	printf("status ([0] = Not expired , [1]= Expired, [2] = New page request) :  %d\n", flag);
	
	if (iterator != NULL && flag==0) {
		
		printf("\nCache hit. Initiating transfer to client\n");
		Send(clifd, demand);
		printf("Transferred page to the client \n\n");
		printf("#####################Complete########################\n");
	}
	
	else if (flag == 2) {
		
		printf("\nGET request to web server \n");
		format = "GET /%s HTTP/1.0\r\nHost: %s\r\nUser-Agent: %s\r\n\r\n";
		
		Message = (char *)malloc(strlen(format) + strlen(document)+ strlen(demand.address) + strlen("Team21") - 5);
		sprintf(Message, format, document, demand.address, "Team21");
		
		if ((send(webserverfd, Message, strlen(Message), 0)) == -1) {
			perror("Error: Send failed");
			exit(1);
		}
		else 
			receive_server(webserverfd, demand, clifd, flag); 

	}
	else { //conditional Get
	    printf("\nConditional GET!!!! \n");
        format = "GET /%s HTTP/1.0\r\nHost: %s\r\nIf-Modified-Since: %s\r\n\r\n";
		char * buff2 = malloc(100 * sizeof(char));
		
		if (iterator->demand.Expiry){ //If expired header present, include expiry date in the conditional GET request
			/*buff2= ctime(&iterator->demand.Expiry);
			
			//assert(0);
			struct tm tim20;
	        //bzero((void *)&tim20, sizeof(struct tm));
			//assert(0);
			printf ("%s is \n", buff2);
			//assert(0);
			for (int i=0; buff2[i]!='\0' ; i++)
				if (buff2[i] == ':'){
					printf ("oh yes %d\n", i);
					prev= i-2;
					den = atoi(buff2[prev]);
					printf ("den loop is %d \n", den);
					prev=prev+1;
				    den2= atoi(buff2[prev]);
					printf ("den2 loop is %d \n", den2);
					den=den*10+den2;
					printf ("den loop final is %d \n", den);
					break;
				}
			printf ("den is %d \n", den);
			//strptime(buff2, 80, "%a %b %d %H:%M:%S %Y", &tim20);
			//assert(0);
			*/
			struct tm *timept = gmtime(&iterator->demand.Expiry);
			//printf("the hour is %d \n", timept->tm_hour);
			
			strftime(buff2, 80, "%a, %d %b %Y %H:%M:%S %Z", timept);
			printf("Sending expiry time for conditional GET to server. Expire time is :  %s\n", buff2);
		}
		else if (iterator->demand.lastmodified.tm_mon >0){ //If expired header absent, include lastmodified date in the conditional GET request
			strftime(buff2, 80, "%a, %d %b %Y %H:%M:%S %Z", &iterator->demand.lastmodified);
			printf("Sending lastmodified time for conditional GET to server.  last modified time is: %s\n", buff2);
		}
		else{ //If expired header and lastmodifiedabsent, include lastaccessed in the conditional GET request
			strftime(buff2, 80, "%a, %d %b %Y %H:%M:%S %Z", iterator->demand.lastaccessed);
			printf("Sending lastaccessedtime for conditional GET to server. lastaccessed time is: %s\n", buff2);
		}
			
		Message = (char *)malloc(strlen(format)+strlen(demand.address)+strlen(document)+strlen(buff2));
		sprintf(Message, format, document, demand.address, buff2);
		printf("\n");
		printf ("Conditional GET Message to server : %s \n", Message);
		
		if ((send(webserverfd, Message, strlen(Message), 0)) == -1) {
			perror("Error: Send failed");
			exit(1);
		}
		else 
			receive_server(webserverfd, demand, clifd, flag); 
	}
}


int main(int argc, char const *argv[])	//main file
{
	int i, nbytes, fdmax, serversockfd, clifd, flag;
	char buffer[SIZEOFBUFFER];
	unsigned int clilen;
	struct sockaddr_in serveraddr, clientaddr;
	struct hostent *server;
	

	if (argc != 3) {
		printf("ERROR: Improper arguments. Please enter in this format (./server <IP> <Portnum>)\n");
		exit(1);
	}
	
	unsigned int port;
	port = atoi(argv[2]);
	server = gethostbyname(argv[1]);
	
	if (server == NULL){
		fprintf(stderr, "ERROR,no such host\n");
		exit(0);
	}
	  
	bzero((char*)&serveraddr, sizeof(struct sockaddr_in));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(port); 
	bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);
	
    serversockfd = socket(AF_INET, SOCK_STREAM, 0) ;
	
	if (serversockfd < 0)  {
		printf("Error: Socket creation error \n");
		exit(1);
	}
	
	//Enables reuse of socket
	int enable = 1;
	setsockopt(serversockfd, SOL_SOCKET, SO_REUSEADDR,(const void *)&enable , sizeof(int));
	  	
	if (bind(serversockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0 ) {
		printf("ERROR: Socket Binding error \n");
		exit(1);
	}

	if ((listen(serversockfd, 10)) < 0) {
		printf("ERROR: Socket Listen error \n");
		exit(1);
	}

	printf("Proxy Server has started successfully \n\n");
	printf("#####################################################\n");
	
	FD_ZERO(&fds_master);
	FD_ZERO(&readfds);
	FD_SET(serversockfd, &fds_master);
	fdmax = serversockfd;
	
	while (1) {
		
		readfds = fds_master;

		if (select(fdmax + 1, &readfds, NULL, NULL, NULL) == -1) {
			printf("ERROR : Select error");
		    exit(1);
		}
		
		for (i = 0; i < fdmax + 1; i++) 
		{
			if (FD_ISSET(i, &readfds)) 
			{
				if (i == serversockfd)  //new connection
				{
					clilen = sizeof(clientaddr);
					clifd = accept(serversockfd, (struct sockaddr*)&clientaddr, &clilen);
					
					if (clifd  == -1) 
					{
						printf("ERROR: Select error \n");
						exit(1);
					}
					else 
					{
						FD_SET(clifd, &fds_master);
						printf("Client connection successful \n\n");
						if (clifd > fdmax)
							fdmax = clifd;
					}
				}
				else //existing connection
				{
					if ((nbytes = recv(i, buffer, SIZEOFBUFFER, 0))<0) {
						printf ("ERROR : Receive error \n");
						exit(1);
					}
					else if (nbytes>0){
						if (strncmp(buffer, "GET", 3) == 0) {
							demandline demand = Decode(buffer);
							printf("Address of the Document is: %s\n", demand.address);
							printf("Document is: %s\n\n", demand.document);
							flag = Cache_check(check, demand);
							if (flag == 2) 
								printf("Item not present in Cache \n");
							StatusCheck(demand, i, flag);
						}
					}
				}
			}
		}
	}
	close(serversockfd);
	return 0;
}
