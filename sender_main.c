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

//the length of the packet, packt = header + buffer
#define PKTLEN 14 * 1024
//length of header, sequence number and length of the content
#define HEADLEN sizeof(unsigned long long int) + sizeof(int) 
//the content in the packet after the header
#define BUFLEN ((PKTLEN-sizeof(unsigned long long int))-sizeof(int))
#define LL sizeof(unsigned long long int)
//constants used to define strings
#define STRLENL 1024
#define STRLENS 128
#define CWINIT 1
#define THRESHOLD 100
#define SMALL 2 * 1024 * 1024

//constants definition
int threshold;
int cw;
int num_pkts;
int sockfd;
int error;
int send_base = 0;

unsigned long long int sum;
unsigned long long int seq = 0;
unsigned long long int next = 0;
unsigned long long int ack_recv = -1;

char buf[BUFLEN];
char recved[LL];

int * recv_bool;

void ** packed;

struct addrinfo *p;

pthread_mutex_t mutex;

void packer(FILE * frp);
void transmitter();
void acker();
void update(unsigned long long int ack_recv);

void reliablyTransfer1(char* hostname, char * port,
		char* filename, unsigned long long int bytesToTransfer);
void reliablyTransfer2(char* hostname, char * port,
        char* filename, unsigned long long int bytesToTransfer);

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa);

int main(int argc, char** argv)
{
	unsigned short int udpPort;
	unsigned long long int numBytes;
	
	if(argc != 5)
	{
		fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
		exit(1);
	}
	numBytes = atoll(argv[4]);
    if(numBytes > SMALL)
    {
        reliablyTransfer1(argv[1], argv[2], argv[3], numBytes);
    }
    else
    {
        reliablyTransfer2(argv[1], argv[2], argv[3], numBytes);
    }

} 

void reliablyTransfer1(char* hostname, char* port,
		char* filename, unsigned long long int bytesToTransfer)
{
    struct addrinfo hints, *servinfo;
    int rv;
    int numbytes;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo( hostname, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return;
    }

    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "talker: failed to bind socket\n");
        return;
    }

    //set timeout
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(struct timeval));
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));

    //open the file and determine the size
    FILE* frp = fopen( filename, "r");
    fseek( frp, 0, SEEK_END );
    sum = ftell(frp);
    if(bytesToTransfer < sum)
        sum = bytesToTransfer;
    printf("bytesToTransfer:%llu\n", sum);
    rewind(frp);

    //form an seq array for acks
    if(sum%BUFLEN == 0)
    {
        num_pkts = sum/BUFLEN;
    }
    else
    {
        num_pkts = sum/BUFLEN + 1;
    }

    if( num_pkts < CWINIT )
    {
        cw = num_pkts;
    }
    else
    {
        cw = CWINIT;
    }
    
    printf("num_pkts:%d\ncw:%d\n", num_pkts, cw );
    recv_bool = calloc( num_pkts, sizeof(int));
  
    //declare buf and set 0
    memset(buf, '\0', BUFLEN);
    memcpy(buf, &sum, LL);


    packed = calloc( num_pkts, sizeof(void *));

    int i;
    for( i = 0; i < num_pkts; i ++)
    {
        packed[i] = NULL;
    }

    pthread_t *tid;
    if( ( tid = calloc( 3, sizeof( pthread_t ) ) ) == NULL )
    {
        perror("Failed to allocate space for thread IDs");
    }

    pthread_mutex_init( &mutex, NULL );

    //creating threads to work
    if( (error = pthread_create( tid, NULL, (void *)packer, (void *)frp) ) )
    {
        fprintf(stderr, "Failed to create thread: %s\n", strerror(error) );
    }

    while(1)
    {
        memcpy(buf, &sum, LL);

        if ((numbytes = sendto(sockfd, buf, LL, 0, p->ai_addr, p->ai_addrlen)) == -1)
        {
            perror("talker: sendto");
            return;
        }
        printf("this is how many bytes To Transfer:%llu\n", sum);


        numbytes = recvfrom(sockfd, recved, LL, 0, p->ai_addr, &p->ai_addrlen);
        printf("numbytes:%d\n", numbytes);
        
        if( !strcmp( recved, "START!!!") )
        {
            puts("GOT START!");
            break;
        }
        memset(recved, '\0', STRLENS);
    }

    puts("Start transferring");
    
    if( (error = pthread_create( tid + 1, NULL, (void *)transmitter, NULL) ) )
    {
        fprintf(stderr, "Failed to create thread: %s\n", strerror(error) );
    }
    if( (error = pthread_create( tid + 2, NULL, (void *)acker, NULL) ) )
    {
        fprintf(stderr, "Failed to create thread: %s\n", strerror(error) );
    }
    
    for( i = 0; i < 2; i++ )
    {
        if( pthread_equal( pthread_self(), tid[i] ) )
            continue;
        if( ( error = pthread_join( tid[i], NULL ) ) )
            fprintf(stderr, "Failed to join thread %d: %s\n", i,strerror(error) );
    }
    pthread_mutex_destroy( &mutex );

    printf("totol sent:%llu\n", seq);

    for( i = 0; i < num_pkts; i++)
    {
        recv_bool[i];
    }

    fclose(frp);
    freeaddrinfo(servinfo);
    free(tid);
    free(packed);
    free(recv_bool);
    close(sockfd);
}

void packer( FILE *frp )
{
    int num_read;
    int i;

    for( i = 0; i < num_pkts; i++ )
    {
        num_read = fread( buf, 1, BUFLEN, frp);
        
        if( num_read <= 0 || sum-seq <= 0 )
            break;
        if(sum-seq < num_read)
            num_read = sum-seq;

        packed[i] = malloc(PKTLEN);
        //preparing pkt
        memset(packed[i], '\0', PKTLEN);
        memcpy(packed[i], &seq, LL);
        memcpy(packed[i] + LL, &num_read, sizeof(int));
        memcpy(packed[i] + HEADLEN, buf, BUFLEN);
        
        printf("pkt i:%d size:%d\n", i, num_read);

        seq += num_read;
    }
    return;
}

void transmitter()
{

    int numbytes;
    int send_cap = send_base + cw;
    int tgt;
    
    while(1)
    {
        send_cap = (send_base + cw) < num_pkts ? send_base + cw : num_pkts;
        
        int i;
        
        printf("send_base: %d\n", send_base);
        if( send_base >= num_pkts-1 )
            break;

        for( i = send_base; i < send_cap; i++)
        {
            if( packed[i] != NULL && !recv_bool[i])
            {
                if ((numbytes = sendto(sockfd, packed[i], PKTLEN, 0, p->ai_addr, p->ai_addrlen)) == -1)
                {
                    perror("talker: sendto");
                    return;
                }
                printf("sending pkt: %d\n", i);
            }
        }
    }
    puts("transmitter done!!!");
    return;
}

void acker()
{
    int numbytes;
    while(1)
    {
        memset(recved, '\0', LL);
        numbytes = recvfrom(sockfd, recved, LL, 0, p->ai_addr, &p->ai_addrlen);
                
        if(!strncmp(recved, "EXIT!", 5))
        {
            return;
        } 
        if( numbytes > 0 )
        {
            memcpy(&ack_recv, recved, LL);
            
            pthread_mutex_lock(&mutex);
            update(ack_recv);
            pthread_mutex_unlock(&mutex);
        }
    }
}

void update( unsigned long long int ack_recv )
{
    int tgt;

    if(ack_recv%BUFLEN)
    {
        tgt = ack_recv/BUFLEN;
    }
    else
    {
        tgt = ack_recv/BUFLEN-1;
        if(tgt >= 0)    
        {
            recv_bool[tgt]++;
            if(recv_bool[send_base])
            {
                if(cw < THRESHOLD )
                    cw++;
                printf("congestion window:%d\n", cw);
            }
        }
    }

    while( recv_bool[send_base] && send_base <num_pkts)
    {
        send_base++;
    }
    send_base--;

    if( recv_bool[tgt] > 2)
    {
        cw /= 2; 
    }
}

void reliablyTransfer2(char* hostname, char * port,
        char* filename, unsigned long long int bytesToTransfer)
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo( hostname, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return;
    }

    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "talker: failed to bind socket\n");
        return;
    }

    //set timeout
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));

    //make the file path

    //open the file and determine the size
    FILE* frp = fopen( filename, "r");
    fseek( frp, 0, SEEK_END );
    unsigned long long int sum = ftell(frp);
    if(bytesToTransfer < sum)
        sum = bytesToTransfer;
    printf("bytesToTransfer:%llu\n", sum);
    rewind(frp);

    //declare buf and set 0
    char buf[BUFLEN];
    memset(buf, '\0', BUFLEN);
    memcpy(buf, &sum, LL);

    char pkt[PKTLEN];
    memset(pkt, '\0', PKTLEN);

    int num_read;
    unsigned long long int seq = 0;
    unsigned long long int next = 0;

    char recved[LL];

    unsigned long long int ack_expect;
    unsigned long long int ack_recv = -1;
    unsigned long long temp;

    while(1)
    {
        memcpy(buf, &sum, LL);
        if ((numbytes = sendto(sockfd, buf, LL, 0, p->ai_addr, p->ai_addrlen)) == -1)
        {
            perror("talker: sendto");
            return;
        }
        memcpy(&temp, buf, LL);

        printf("this is how many bytes To Transfer:%llu\n", temp);

        numbytes = recvfrom(sockfd, recved, LL, 0, p->ai_addr, &p->ai_addrlen);
        printf("numbytes:%d\n", numbytes);
        
        ack_expect = 0;

        if( !strcmp( recved, "START!!!") )
        {
            puts("GOT START!");
            break;
        }
        memset(recved, '\0', STRLENS);
    }

    puts("Start transferring");

    while(1)
    {
        num_read = fread( buf, 1, BUFLEN-1, frp);
        
        if( num_read <= 0 || sum-seq <= 0 )
            break;
        if(sum-seq < num_read)
            num_read = sum-seq;
        
        ack_expect = seq + num_read;
        printf("num_read:%d\n", num_read);
        printf("sequence:%llu\n", seq);
        printf("ack expected:%llu\n", ack_expect);
        buf[num_read] = '\0';

        //preparing pkt
        memset(pkt, '\0', PKTLEN);
        memcpy(pkt, &seq, LL);
        memcpy(pkt + LL, &num_read, sizeof(int));
        memcpy(pkt + HEADLEN, buf, BUFLEN);
        printf("pkt content%s\n", pkt+HEADLEN);

        while(1)
        {
            if ((numbytes = sendto(sockfd, pkt, PKTLEN, 0, p->ai_addr, p->ai_addrlen)) == -1)
            {
                perror("talker: sendto");
                return;
            }

            memset(recved, 0, LL);
            if ((numbytes = recvfrom(sockfd, recved, LL, 0, p->ai_addr, &p->ai_addrlen)) == -1) 
            {
                perror("recvfrom");
            }

            memcpy(&ack_recv, recved, LL);
            
            printf("ack_expect:%llu\n", ack_expect);
            printf("ack_recv:%llu\n", ack_recv);

            if( ack_expect == ack_recv )
                break;
        }

        puts("");
        seq += num_read;
    }
    printf("totol sent:%llu\n", seq);

    fclose(frp);
    freeaddrinfo(servinfo);
    close(sockfd);
}

void * get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
