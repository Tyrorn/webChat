#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>


//#define BUFSIZ

static int lookup_host_ipv4(const char *hostname, struct in_addr *addr) {
	struct hostent *host;
	
	/*look up hostname, find first IPv4 entry*/
	host = gethostbyname(hostname);
	while(host) {
		if(host->h_addrtype == AF_INET &&
			host->h_addr_list &&
			host->h_addr_list [0]) {
				memcpy(addr,host->h_addr_list[0],sizeof(*addr));
				return 0;
			}
			host=gethostent();
		}
		return -1;
	}
	
	int client_connect(const char *hostname, unsigned short port) {
		struct sockaddr_in addr;
		int fd, r;
		
		/*look up hostnamme*/
		r = lookup_host_ipv4(hostname, &addr.sin_addr);
		if (r!=0) {/*handle erroe*/}
		
		fd = socket(AF_INET,SOCK_STREAM, 0);
		if (fd<0) {
			printf("Client socket creation failed... \n");
			exit(1);
			}
		
		memset(&addr, '\0', sizeof(addr));
			
		/*connect to server*/
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		r = connect(fd, (struct sockaddr *) &addr, sizeof(addr));
		if (r!= 0) { 
			printf("Connection with server failed\n");
			}
		else {
			printf("Connection successful \n");
		}
		return fd;
	}
int main (int argc, char * argv[]) {
	int port = atoi(argv[2]),count_fd;
	//char host = ;
	char buffer[BUFSIZ];
	fd_set readfds;
	int fd = (client_connect(argv[1],port));        //receiving port from terminal, using localhost
	printf("connecting to server %s:%d\n",argv[1],port);
	//setbuf(stdout,NULL);
	while (1)
	{
		FD_ZERO(&readfds);                              //set file descriptor(fd) for users keyboard, and socket connected 
		FD_SET(fd,&readfds);
		FD_SET(STDIN_FILENO,&readfds);
		//printf("Type: ");
		fflush(stdout);
		if(STDIN_FILENO>fd)                             //find highest fd 
		{
			count_fd = STDIN_FILENO;
		}else
		{
			count_fd = fd;
		}			
		select(count_fd+1,&readfds,NULL,NULL,NULL);     //make sure both fds are correctly watched
		if(FD_ISSET(fd, &readfds))                      //if user receives something from server 
		{
			read(fd,buffer,BUFSIZ);
			printf("%s \n",buffer);
			bzero(buffer,sizeof(buffer));
		}
		if(FD_ISSET(STDIN_FILENO, &readfds))            //response if user types something to be sent
		{
			
			if(fgets(buffer,sizeof(buffer)-1,stdin)==NULL)
			{
           		printf("disconnecting from server\n");
				exit(1);
			}
			if(strcmp(buffer,"/exit\n") ==0)
			{
				printf("disconnecting from server\n");
				exit(1);
			}
			send(fd,buffer,strlen(buffer),0);
			bzero(buffer,sizeof(buffer));
			
		}
	}	
}
	
