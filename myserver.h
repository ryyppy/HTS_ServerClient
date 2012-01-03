#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <pthread.h>
#include <ldap.h>
#include "myutil.h"

#define PORT 6543
#define FAKE_LOGIN 0
#define IP_LOCK_SECONDS 30 * 60

//LDAP Konstante
#define LDAP_HOST "ldap.technikum-wien.at"
#define LDAP_PORT 389
#define SEARCHBASE "dc=technikum-wien,dc=at"
#define SCOPE LDAP_SCOPE_SUBTREE

typedef struct {
	int socket;
	char *spooldir;
	char *lockdir;
	struct sockaddr_in cliaddress;
	LDAP *ld;
}ThreadArguments;

//Setup-Funktion
int prepare_server(char *spooldir, char *lockdir);

//Thread-Funktionen
void *thread_run(void *arg);

//Grundfunktionen mit internem Socketstream-Handling
int login_procedure(int socket, LDAP *ld, char *username);
void send_procedure(int socket, char *spooldir, char *username);
void list_procedure(int socket, char *spooldir, char *username);
void read_procedure(int socket, char *spooldir, char *username);
void del_procedure(int socket, char *spooldir, char *username);

//Funktionen fuer die IP-Sperre
int check_ip_lock(char *ip, char *lockdir, time_t *rest_seconds);
void lock_ip(char *ip, char *lockdir);
int unlock_ip(char *ip, char *directory);

//Utility-Funktionen
void send_msg_to_client(int socket, char *msg);
int mkpath(const char *path, mode_t mode);
