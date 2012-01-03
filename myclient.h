#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <time.h>

#include "myutil.h"

#define PORT 6543

//Prozeduren, die eine Arbeitseinheit darstellen
int login_procedure(int socket, char *username);
void send_procedure(int socket);
void list_procedure(int socket);
void del_procedure(int socket);
void read_procedure(int socket);
int logout_procedure(int socket);
