#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <dlfcn.h>

#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <signal.h>
//global vars declaration

#define BACKLOG 10
#define STRSIZE 1024

static char* ok_response =
  "HTTP/1.0 200 OK\r\n";

static char* bad_request_response = 
  "HTTP/1.0 400 Bad Request\r\n"
  "Content-type: text/html\r\n"
  "\r\n"
  "<html>\n"
  " <body>\n"
  "  <h1>400 Bad Request</h1>\n"
  "  <p>This server did not understand your request.</p>\n"
  " </body>\n"
  "</html>\n";

static char* not_found_response = 
  "HTTP/1.0 404 Not Found\r\n"
  "Content-type: text/html\r\n"
  "\r\n"
  "<html>\n"
  " <body>\n"
  "  <h1>404 Page Not Found</h1>\n"
  "  <p>The requested URL %s was not found on this server.</p>\n"
  " </body>\n"
  "</html>\n";

static char* bad_method_response = 
  "HTTP/1.0 501 Method Not Implemented\r\n"
  "Content-type: text/html\r\n"
  "\r\n"
  "<html>\n"
  " <body>\n"
  "  <h1>Method Not Implemented</h1>\n"
  "  <p>The method %s is not implemented by this server.</p>\n"
  " </body>\n"
  "</html>\n";

//global fd
int new_fd;
int sockfd;

//global char *
char * hostname;
char * http_req;

//struct
typedef struct rp_t
{
	size_t len;
	char * length;
	char * tp;
	char * body;
}rp_t;

//handles child process
void sigchld_handler( int signo );
//get sockaddr, IP4 or IPv6
void * get_in_addr( struct sockaddr *sa );

//init the struct carries info to send
void init_rp( struct rp_t * rp );
void destroy_rp(struct rp_t *rp);

//main handler
void handle_cnt(int new_fd);
void handle_get(int new_fd, char* url);

//assemble the info
void assemble(char * path, struct rp_t * rp);

//wrap up all the send funcs
int send_all( int new_fd, struct rp_t * rp );

int main( int argc, char ** argv )
{
	if( argc != 2)
	{
		perror("usage: port number");
		exit(1);
	}

	//declarations
	char buf[STRSIZE];

	//defined ptrs
	char * loc;

	//allocated ptrs
	hostname = calloc( STRSIZE, sizeof(char) );
	http_req = calloc( STRSIZE, sizeof(char) );

	struct addrinfo hints;
	struct addrinfo * servinfo;
	struct addrinfo * p;

	struct sockaddr_storage their_addr;

	socklen_t sin_size;
	struct sigaction sa;
	int yes = 1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	memset( &hints, 0, sizeof hints );
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	gethostname( hostname, STRSIZE );

	printf("hostname: %s\n", hostname);

	if( ( rv = getaddrinfo( NULL, argv[1], &hints, &servinfo ) ) != 0 )
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror( rv ) );
		return 1;
	}

	//loop through all the results and bind to the first we can
	for( p = servinfo; p != NULL; p = p->ai_next )
	{
		if( (sockfd = socket( p->ai_family, p->ai_socktype,
			p->ai_protocol) ) == -1 )
		{
			perror( "server: failed to socket()" );
			continue;
		}

		if( setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int) ) == -1 )
		{
			perror("setsockopt");
			exit(1);
		}


		if( bind( sockfd, p->ai_addr, p->ai_addrlen ) == -1 )
		{
			close( sockfd );
			perror("server: failed to bind()");
			continue;
		}
		break;
	}

	if( p == NULL )
	{
		fprintf(stderr, "server: NULL after bind()\n");
		return 2;
	}

	freeaddrinfo( servinfo );

	if( listen( sockfd, BACKLOG ) == -1 )
	{
		perror("failed to listen()");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; //reap all the dead process
	sigemptyset( &sa.sa_mask );
	sa.sa_flags = SA_RESTART;

	if( sigaction ( SIGCHLD, &sa, NULL ) == -1 )
	{
		perror("failed to sigaction()");
		exit(1);
	}

	printf("server: initialized, waiting for connections ...\n");

	while(1)
	{
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		pid_t cpid;

		if( new_fd == -1 )
		{
			perror ("accept");
			exit(1);
		}

		inet_ntop( their_addr.ss_family,
			get_in_addr( (struct sockaddr *)&their_addr ),
			s,sizeof s );

		printf("server: got connection from %s\n", s);

		cpid = fork();
		if( !cpid ) //child process
		{
			close (STDIN_FILENO);
    		//close (STDOUT_FILENO);
    		close (sockfd);
    		handle_cnt(new_fd);
    		close (new_fd);
    		exit (0);
		}
		else if( cpid )
		{
			printf("server: job handled, waiting for the next connection ...\n");
			close(new_fd); //parent do not need it
		}
		exit(0);
	}
	return 0;
}

void handle_cnt(int new_fd)
{
	char buffer[512];
	ssize_t bytes_read;

	bytes_read = read( new_fd, buffer, sizeof (buffer) - 1 );
  	
  	if(bytes_read > 0)
  	{
    	buffer[bytes_read] = '\0';

    	char method[sizeof (buffer)];
    	char url[sizeof (buffer)];
    	char protocol[sizeof (buffer)];
    	char dir[sizeof (buffer)];

    	getcwd( dir, sizeof buffer );    	
    	sscanf (buffer, "%s %s %s", method, url, protocol);
	    strcat(dir, url);

	    while (strstr (buffer, "\r\n\r\n") == NULL)
      	{
      		bytes_read = read(new_fd, buffer, sizeof (buffer) );
    	}
    
    	if (bytes_read == -1)
    	{
      		close (new_fd);
      		return;
    	}
    	
    	if (strcmp (protocol, "HTTP/1.0") && strcmp (protocol, "HTTP/1.1"))
    	{
      		write(new_fd, bad_request_response, sizeof(bad_request_response) );
    	}
    	else if (strcmp (method, "GET"))
    	{
      		char response[STRSIZE];
		    snprintf (response, STRSIZE, bad_method_response, method);
      		write(new_fd, response, strlen (response));
    	}
    	else 
	    {
	    	handle_get(new_fd, dir);
  		}
  	}
  	else if (bytes_read == 0)
    	;
  	else
  	{
    	perror ("read");
    	exit(1);
  	}
}

void handle_get(int new_fd, char* url)
{
	struct server_module* module = NULL;
	struct stat st;

  	if (*url == '/' && !strstr(url, "..") && (stat( url, &st ) != -1) )
  	{
    	char target[128];
    	struct rp_t rp;
    	init_rp(&rp);

    	assemble( url, &rp );
    	send_all( new_fd, &rp );

    	printf("%s%s", ok_response, rp.tp);
    	destroy_rp(&rp);
	}
	else
  	{
		char response[STRSIZE];
    	snprintf (response, sizeof (response), not_found_response, url);
    	write(new_fd, response, strlen (response));
    	printf("%s", not_found_response);
	}
}

void assemble(char * path, struct rp_t * rp)
{
	FILE * fp = fopen( path, "rb+");
                
    fseek( fp, 0, SEEK_END );
    rp->len = ftell(fp);
    rewind(fp);
    
    //snprintf(rp->length, STRSIZE, "Content-length: %lu\r\n", rp->len);
    snprintf( rp->tp, STRSIZE, "Content-type: text/html\r\n\r\n" );
    
    rp->body = malloc( rp->len );
    fread( rp->body, 1, rp->len, fp);
    fclose(fp);
}

int send_all( int new_fd, struct rp_t * rp )
{
	if( write(new_fd, ok_response, strlen(ok_response) ) == -1 )
	{
		perror("failed to write response line.");
		return 1;
	}

	/*if( write( new_fd, rp->length, strlen(rp->length) ) == -1 )
	{
		perror("failed to write response length field.");
		return 1;
	}*/

	if( write( new_fd, rp->tp, STRSIZE ) == -1 )
	{
		perror("failed to write response connection field.");
		return 1;
	}

	if( write( new_fd, rp->body, rp->len ) == -1 )
	{
		perror("failed to write response body.");
		return 1;
	}
	return 0;
}

void * get_in_addr( struct sockaddr *sa )
{
	if( sa->sa_family == AF_INET )
	{
		return &( ((struct sockaddr_in *)sa)->sin_addr );
	}
	return &( ((struct sockaddr_in6 *)sa)->sin6_addr ) ;
}

void sigchld_handler( int s )
{
	while( waitpid( -1, &s, WNOHANG ) > 0 );
}

void init_rp( struct rp_t * rp )
{
	rp->length = calloc( STRSIZE, sizeof(char) );
	rp->tp = calloc( STRSIZE, sizeof(char) );
}

void destroy_rp(struct rp_t *rp)
{
	free(rp->length);
	free(rp->tp);
	free(rp->body);
}
