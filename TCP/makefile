#CS438 MP1 Makefile
#by Yumen Tsao

CC = gcc
INC = -I. -Ilibs
FLAGS = -O0 -Wall -Wextra -Werror -Wno-unused -Wno-sign-compare -g -std=gnu99 -pthread
DFLAG=

all: tcp_sender tcp_receiver

tcp_sender: sender_main.c
	$(CC) $(FLAGS) $(INC) $^ -o $@

tcp_receiver: receiver_main.c
	$(CC) $(FLAGS) $(INC) $^ -o $@

clean:
	$(RM) -r *.o tcp_sender tcp_receiver

