#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#define BUF 1024
#define MAX_RECEIVERS 10

//Mail-Struktur
typedef struct {
	int mailnum;
	char sender[9];
	char receiver[MAX_RECEIVERS * 10]; //Multiple Receivers seperated by ';'
	char title[81];
	char *message;
	char *a_content; //Attachment Content
	char a_name[100]; //Attachment Filename
	long a_size;	//Attachment Groesse
}Mail;

//Stringoperationsmethoden
int append(char **text, const char *to_append);
int remove_escapes(char *str);
ssize_t readline (int fd, void *vptr, size_t maxlen);

//Protokoll-Lese- und Schreibe-Funktionen
int receive_mail(int socket, Mail *mail);
int send_mail(int socket, Mail *mail);
