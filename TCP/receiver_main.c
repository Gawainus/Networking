#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>

#define PKTLEN 14 * 1024
#define HEADLEN sizeof(unsigned long long int) + sizeof(int)
#define BUFLEN ((PKTLEN-sizeof(unsigned long long int))-sizeof(int))
#define LL sizeof(unsigned long long int)
#define STRLENL 1024
#define STRLENS 128
#define SLOTS 128
#define SMALL 2 * 1024 * 1024

int inorder_front;
int num_pkts;
int sockfd;
int writer_done = 0;
int pkt_size;


unsigned long long int sum;
unsigned long long int seq;
unsigned long long int ack;


int * recv_bool;
int * written_bool;

char buf[PKTLEN];

pthread_mutex_t mutex;

typedef struct pkt
{
    int tgt;
    int size;
    void * content;
}pkt_t;

pkt_t ** recv_buffer;

struct sockaddr_storage their_addr;
socklen_t addr_len;

void receiver();
void writer(FILE * fwp);
void reliablyReceive(char * port, char* destinationFile);
void update_inorder_front(int i);
void *get_in_addr(struct sockaddr *sa);

int main(int argc, char** argv)
{
	unsigned short int udpPort;
	
	if(argc != 3)
	{
		fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
		exit(1);
	}
	
	reliablyReceive( argv[1], argv[2] );
}

void reliablyReceive( char * port, char* destinationFile)
{
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    int error;
    
    char s[INET6_ADDRSTRLEN];

    memset(&hints, '\0', sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("listener: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return;
    }

    freeaddrinfo(servinfo);

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(struct timeval));
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));

    printf("listener: waiting to recvfrom...\n");

    addr_len = sizeof their_addr;

    FILE* fwp = fopen( destinationFile, "w");

    while(1)
    {
        memset(buf, '\0', PKTLEN);
        numbytes = recvfrom(sockfd, buf, PKTLEN, 0, (struct sockaddr *)&their_addr, &addr_len);
        memcpy(&sum, buf, LL);

        if( sum != 0 )
        {
            puts("GOT SUM");
            break;
        }
    }
    if(sum > SMALL)
    {
        if(sum%BUFLEN == 0)
        {
            num_pkts = sum/BUFLEN;
        }
        else
        {
            num_pkts = sum/BUFLEN + 1;
        }
        printf("num_pkts:%d\n", num_pkts);

        recv_bool = calloc(num_pkts, sizeof(int));
        written_bool = calloc(num_pkts, sizeof(int));
        recv_buffer = calloc(num_pkts, sizeof(pkt_t*));


        pthread_t *tid;
        if( ( tid = calloc( 2, sizeof( pthread_t ) ) ) == NULL )
        {
            perror("Failed to allocate space for thread IDs");
        }

        pthread_mutex_init( &mutex, NULL );
        //creating threads to work
        if( (error = pthread_create( tid, NULL, (void *)receiver, NULL) ) )
        {
            fprintf(stderr, "Failed to create thread: %s\n", strerror(error) );
        }
        if( (error = pthread_create( tid+1, NULL, (void *)writer, (void *)fwp) ) )
        {
            fprintf(stderr, "Failed to create thread: %s\n", strerror(error) );
        }

        puts("threads created!");

        int i;
        for( i = 0; i < 2; i++ )
        {
            if( pthread_equal( pthread_self(), tid[i] ) )
                continue;
            if( ( error = pthread_join( tid[i], NULL ) ) )
                fprintf(stderr, "Failed to join thread %d: %s\n", i,strerror(error) );
        }
        
        pthread_mutex_destroy( &mutex );
        puts("");
        //printf("total revc:%llu\n", ack);
        free(tid);
    }
    else
    {
        while(1)
        {
            if ((numbytes = sendto(sockfd, "START!!!", 8, 0, (struct sockaddr *)&their_addr, addr_len) ) == -1)
            {
                perror("talker: sendto");
                return;
            }
            
            numbytes = recvfrom(sockfd, buf, PKTLEN, 0, (struct sockaddr *)&their_addr, &addr_len);
            memcpy( &seq, buf, LL);

            if(seq == 0)
                break;
        }

        while(1)
        {   
            if(seq == ack)
            {
                fwrite(buf+HEADLEN, 1, pkt_size, fwp);
                ack += pkt_size;
            }

            memset(buf, '\0', PKTLEN);
            memcpy(buf, &ack, LL);
            if ((numbytes = sendto(sockfd, buf, LL, 0, (struct sockaddr *)&their_addr, addr_len) ) == -1)
            {
                perror("talker: sendto");
                return;
            }

            if( sum <= ack )
                break;

            numbytes = recvfrom(sockfd, buf, PKTLEN, 0, (struct sockaddr *)&their_addr, &addr_len);
            memcpy( &seq, buf, LL);
            memcpy( &pkt_size, buf+LL, sizeof(int));
        }

        puts("");
        printf("total revc:%llu\n", ack);
        fclose(fwp);
    }
    close(sockfd);
    return;
}

void receiver()
{
    int i;
    int numbytes;

    unsigned long long int seq; //the current seq that sender is giving
    unsigned long long int ack; //the current seq that sender is giving

    while(1)
    {
        //I hereby request pkt 0 from you! By heaven I charge thee, send!
        if ((numbytes = sendto(sockfd, "START!!!", 8, 0, (struct sockaddr *)&their_addr, addr_len) ) == -1)
        {
            perror("talker: sendto");
            return;
        }
        
        numbytes = recvfrom(sockfd, buf, PKTLEN, 0, (struct sockaddr *)&their_addr, &addr_len);
        memcpy( &seq, buf, LL);
        memcpy( &pkt_size, buf+LL, sizeof(int));
        
        if(seq == 0)
            break;
    }
    puts("start receiving");

    while(1)
    {
        puts("Spinning!");

        printf("seq:%llu\n", seq);
        printf("size:%d\n", pkt_size);

        if( seq%BUFLEN )
        {
            i = seq/BUFLEN +1;
            printf("pkt %d\n", i);
            puts("last one");
        }
        else
        {
            i = seq/BUFLEN;
        }

        if(!recv_bool[i])
        {
            recv_bool[i] = 1;
            printf("pkt: %d bool: %d\n", i, recv_bool[i]);

            recv_buffer[i] = calloc(1, sizeof(pkt_t));
            recv_buffer[i]->content = calloc( 1, BUFLEN);

            recv_buffer[i]->tgt = i;
            memcpy( &(recv_buffer[i]->size), buf+LL, sizeof(int));
            memcpy( recv_buffer[i]->content, buf+HEADLEN, BUFLEN);
            
            pthread_mutex_lock(&mutex);
            update_inorder_front(i);
            pthread_mutex_unlock(&mutex);

        }

        ack = seq + pkt_size;
        memset(buf, '\0', PKTLEN);
        memcpy(buf, &ack, LL);

        if ((numbytes = sendto(sockfd, buf, LL, 0, (struct sockaddr *)&their_addr, addr_len) ) == -1)
        {
            perror("talker: sendto");
            return;
        }
        
        do
        {
            numbytes = recvfrom(sockfd, buf, PKTLEN, 0, (struct sockaddr *)&their_addr, &addr_len);
            if( writer_done )
            {
                puts("I should leave as well!!!");
                break;
            }
        }while(numbytes <= 0);
        
        memcpy( &seq, buf, LL);
        memcpy( &pkt_size, buf+LL, sizeof(int));
    }
}

void writer( FILE * fwp )
{
    int i;
    int written_front = 0;
    while(1)
    {
        pthread_mutex_lock(&mutex);

        for( i = written_front; i < inorder_front; i++ )
        {
            fwrite( (recv_buffer[i])->content, 1, recv_buffer[i]->size, fwp);
            written_bool[i] = 1;
            written_front++;
            //printf("writer:%s\n", (char *)recv_buffer[i]->content);

            free(recv_buffer[i]->content);
            if( i >= num_pkts - 1)
            {
                fclose(fwp);
                writer_done = 1;
                strcpy(buf,"EXIT!");
                int j;
                for( j = 0; j < 10; j++ )
                {
                    sendto(sockfd, buf, 5, 0, (struct sockaddr *)&their_addr, addr_len);
                }
                puts("Writer is done!!");
                return;   
            }
        }
        written_front = inorder_front;
        pthread_mutex_unlock(&mutex);

    }
}

void update_inorder_front(int i)
{
    if( i == 0 )
    {
        inorder_front = 1;
        return;
    }

    if(i > inorder_front +1)
        return;

    if(recv_bool[i-1])
    {
        inorder_front = i;
        while( i < num_pkts)
        {
            if(recv_bool[i])
                i++;
            else
                break;
        }
        inorder_front = i;
    }
    return;
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
