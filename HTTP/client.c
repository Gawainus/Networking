#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#define STRSIZE 1024 // max number of bytes we can get at once

typedef struct request_t
{
	char * hostname;
	char * port;
	char * path;
}request_t;

//Helpers
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa);

int fill_request( struct request_t * req, char * info );

char * ff_name( char * path );

void destroy_request( struct request_t * req );

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr,"usage: client hostname\n");
		exit(1);
	}

	//Delarations
	//flags
	int start_prt;
	//counters
	int i;

	//sockets
	int sfd;
	int numbytes;
	int rv;
	int req_len;
	int file_len;

	char * req_msg;
	char * body;
	char buf[STRSIZE];

	struct addrinfo hints;
	struct addrinfo * servinfo; 
	struct addrinfo * p;

	char s[INET6_ADDRSTRLEN];

	//request struct
	struct request_t * req;

	//content directory
	struct stat fstatus;
	char cwd[STRSIZE];
	FILE * fp;

	//Initiations
	req = calloc( 1, sizeof(request_t) );
	fill_request( req, argv[1] );

	memset( &hints, 0, sizeof(hints) );
	hints.ai_family = AF_UNSPEC; //Can use IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM; //TCP
	
	if( (rv = getaddrinfo(req->hostname, 
		req->port, &hints, &servinfo)) != 0)
	{
		fprintf( stderr, "getaddrinfo: %s\n", gai_strerror(rv) );
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next)
	{
		if ((sfd = socket(p->ai_family, p->ai_socktype,
			p->ai_protocol)) == -1)
		{
			perror("client: socket");
			continue;
		}
		
		if( connect(sfd, p->ai_addr, p->ai_addrlen) == -1 )
		{
			close(sfd);
			perror("client: connect");
			continue;
		}
		break;
	}

	if (p == NULL)
	{
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	req_msg = calloc( STRSIZE, sizeof(char) );

	snprintf( req_msg, STRSIZE, "GET %s HTTP/1.1\r\nHost: %s\r\nAccept: */*\r\nConnection: close\r\n\r\n",
		req->path, req->hostname);

	printf("%s", req_msg);
	req_len = strlen(req_msg);

	inet_ntop(p->ai_family, 
		get_in_addr((struct sockaddr *)p->ai_addr),
		s, sizeof(s));
	
	printf("client: connecting to %s\n", s);
	
	freeaddrinfo(servinfo); // all done with this structure

	if( !getcwd( cwd, sizeof(cwd) ) )
	{
		perror("getcwd:");
		exit(1);	
	}

	strcat( cwd, "/output" );
	
	fp = fopen( cwd, "w" );

	if( write( sfd, req_msg, req_len ) == -1 )
	{
		perror("send");
		exit(1);
	}

	memset( &buf, 0, sizeof(buf) );
	start_prt = 0;
	
	while(1)
	{
		if ((numbytes = read(sfd, buf, STRSIZE-1 ) ) == -1)
		{
			perror("recv");
			exit(1);
		}

		if( numbytes <= 0)
		{
			break;
		}

		if(start_prt)
		{
			fprintf(fp, "%s", buf);
		}
		else
		{
			char * now = strstr( buf, "\r\n\r\n");
			
			if(!now)
			{
				continue;
			}
			
			now += 4;
			fprintf(fp, "%s", now);
			start_prt = 1;
		}
		
		bzero(&buf, sizeof(buf) );
	}

	fclose(fp);
	close(sfd);
	destroy_request(req);
	return 0;
}

//Helpers
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) 
	{
		return &( ((struct sockaddr_in*)sa)->sin_addr );
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int fill_request( struct request_t * req, char * info )
{
	int hostname_len = 0;
	int portnum_len = 0;
	int port_specified = 0;
	char * temp;

	if( (req->path = strstr(info, ":")) )
	{
		port_specified = 1;
		temp = info;
		
		while( temp != req->path )
		{
			hostname_len++;
			temp++;
		}
		
		req->hostname = strndup( info, hostname_len );		

		req->path = strstr(info, "/");

		while( temp+1 != req->path )
		{
			portnum_len++;
			temp++;
		}

		req->port = strndup( info+hostname_len+1, portnum_len );
		req->path = strdup( info+hostname_len+portnum_len+1 );
	}
	else
	{
		req->port = strdup("80");
		temp = info;
		req->path = strstr(info, "/");

		while( temp != req->path )
		{
			hostname_len++;
			temp++;
		}
		
		req->hostname = strndup( info, hostname_len );		
		req->path = strdup( info+hostname_len );
	}
	return port_specified;
}

void destroy_request( struct request_t * req )
{
	free(req->hostname);
	free(req->port);
	free(req->path);
	free(req);
}
