#ifndef _HELPERS_H
#define _HELPERS_H 

//helpers.h from lab8

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

#define MAX_CLIENTS 30
#define BUFLEN 1550

struct message
{
    char IP_CLIENT_UDP[16];
    uint16_t port;
    char topic[50];
    int type;
    char content[1500];
    bool valid;
};

#endif
