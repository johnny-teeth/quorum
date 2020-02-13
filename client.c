#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>

#include "client_ui.h"
#include "client.h"

int port = 7776;
extern char * my_addr;


/* connect to coordinator and connect to replicate 
 * run UI after connection is complete
 */
void connect_server(int port, char * addr) {
    char * tmp1, * tmp2;
	struct sockaddr_in local_serve, update_serve;
    loc_sock = socket(AF_INET, SOCK_STREAM, 0);
	int rdsz, i = 0;
	char * ipport;
    
    fprintf(stdout, "connecting %d\n", port);
	local_serve.sin_family = AF_INET;
	local_serve.sin_port = htons(port);
	local_serve.sin_addr.s_addr = inet_addr(addr);
    
	if(connect(loc_sock, (struct sockaddr *)&local_serve, sizeof(local_serve)) < 0 ) {
		fprintf(stderr, "Cannot connect to server: connect_serve %s\n", addr);
		return;
	}
    fprintf(stdout, "connected\n");

    ipport = malloc(128);
    update_serve.sin_family = AF_INET;
    if ((rdsz = read(loc_sock, ipport, 128)) <= 0){
        fprintf(stdout, "Read failed: connect_server [2]\n");
        return;
    }
    fprintf(stdout, "%s\n", ipport);
    
    while(ipport[i] != ':')
        i++;
    tmp2 = malloc(i + 1);
    memcpy(tmp2, ipport, i);
    tmp2[i] = '\0';
    tmp1 = ipport + i + 1;
    fprintf(stdout, "%s %s\n", tmp1, tmp2);
    
    update_serve.sin_addr.s_addr = inet_addr(tmp1);
    update_serve.sin_port = htons(atoi(tmp2));
	free(ipport);
    
	close(loc_sock);
    memset(&loc_sock, 0, sizeof(int));
	loc_sock = socket(AF_INET, SOCK_STREAM, 0);
	if(connect(loc_sock, (struct sockaddr *)&update_serve, sizeof(update_serve)) < 0) {
		fprintf(stderr, "Cannot connect to replicate: connect_serve\n");
	}
    fprintf(stdout, "Using sock: %d\n", loc_sock);
	run_ui(loc_sock);
}

int main (int argc, char ** argv) {
	// client -[sqr] -p [portnum] -a [ipaddr]
	int i, port = 0;
	char * addr = NULL;

	if (argc < 2) {
		fprintf(stdout, "%s -p [port] -a [ipaddr]\n", argv[0]);
		exit(0);
	}
	
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-p") == 0) {
			port = atoi(argv[++i]);
		} else if (strcmp(argv[i], "-a") == 0) {
			addr = strdup(argv[++i]);
		}
	}	

	if (port > 0 || addr != NULL) {
		connect_server(port, addr);
	}
    
    free(addr);
	return 0;
}
