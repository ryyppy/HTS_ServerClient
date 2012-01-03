#include "myutil.h"

/**
 * Kombiniert *to_append mit *text.
 * Falls das text-Array zu klein ist, wird neuer Speicher mit realloc hinzugefuegt
 * der uebergebene **text-Parameter muss nach der Verwendung mit free() freigegeben werden!
 *
 * @param char **text - Textbasis, an dem angehaengt wird (muss ein dynamischer Pointer sein!)
 * @param char *to_append - Das Array, das an den parameter **text angehaengt werden soll
 */
int append(char **text, const char *to_append){
	if(*text == NULL){
		*text = (char *)malloc(BUF * sizeof(char));
		strcpy(*text, "\0");
	}
	int extend = (strlen(*text) + strlen(to_append));

	if(extend / BUF > 1){
		char *newptr = (char*)realloc(*text, strlen(to_append) + strlen(*text) + 1);

		if(newptr != NULL)
			return 0;

		*text = newptr;
	}

	strncat(*text, to_append, strlen(to_append));
	return 1;
}

/**
 * Wandelt alle \r und \n Endungen in ein \0 um
 * - char *str: Der String, dessen Endung manipuliert werden soll
 */
int remove_escapes(char *str){
	int i;
	int count = 0;

	for(i=0; i < strlen(str); i++){
		if(str[i] == '\n' || str[i] == '\r'){
			str[i] = '\0';
			count++;
		}
	}

	return count;
}

/**
 * Liest vom Stream fd zeilenweise Daten (\n Endung) und speichert die Zeile in *vptr
 * Es wird die Zeile mit maxlen beschraenkt.
 */
ssize_t readline (int fd, void *vptr, size_t maxlen){
 ssize_t   n, rc ;
 char      c, *ptr ;
 ptr = vptr ;
 for (n = 1 ; n < maxlen ; n++) {
   again:
     if ( (rc = read(fd,&c,1)) == 1) {
       *ptr++ = c ;
       if (c == '\n')
         break ;                  // newline ist stored, like fgets()
     } else if (rc == 0) {
         if (n == 1)
           return (0) ;           // EOF, no data read
         else
           break ;                // EOF, some data was read
     } else {
         if (errno == EINTR)
           goto again ;
         return (-1) ;            // error, errno set by read()
     } ;
 } ;

 *ptr = 0 ;                       // null terminate like fgets()

 return (n) ;
}

/**
 * Utilitymethode um den SEND-Befehl aus einer Mail-Struktur zusammenzusetzen
 * *mail - Eine bereits allocierte Mail-Struktur, die alle Daten beinhaltet, die in die SEND-Message gepackt werden sollen
 * socket - Der Socket Ÿber den die Message gesendet werden soll
 *
 * RETURN: 1, falls die Mail gesendet wurde, ansonsten 0
 */
int send_mail(int socket, Mail *mail){
	char *message = NULL;

	if(mail->a_size > 0)
		asprintf(&message, "SEND\n%s\n%s\n%s\n%ld\n%s\n%s\n%s\n.\n", mail->sender, mail->receiver, mail->title, mail->a_size, mail->a_name, mail->a_content, mail->message);

	if(mail->a_size == 0)
		asprintf(&message, "SEND\n%s\n%s\n%s\n%ld\n%s\n.\n",mail->sender, mail->receiver, mail->title, mail->a_size, mail->message);

	if(message != NULL){
		send(socket, message, strlen(message), 0);
		free(message);
		return 1;
	}
	return 0;
}

/**
 * Liest aus dem angegebenen Socket die Daten in eine Mail-Struktur
 * *mail - Bereits allocierte Mail-Struktur, die noch keine Daten gesetzt hat. Die Daten werden aus dem Socket gelesen.
 * socket - Der Socket, aus dem die Daten gelesen werden
 */
int receive_mail(int socket, Mail *mail){
	char buffer[BUF] = "\0";
	int err = 0;

	bzero(buffer, sizeof(buffer));
	readline(socket, buffer, sizeof(buffer));
	strncpy(mail->sender, buffer, (sizeof(mail->sender))-1);
	remove_escapes(mail->sender);

	bzero(buffer, sizeof(buffer));
	readline(socket, buffer, sizeof(buffer));
	strncpy(mail->receiver, buffer, (sizeof(mail->receiver))-1);
	remove_escapes(mail->receiver);

	bzero(buffer, sizeof(buffer));
	readline(socket, buffer, sizeof(buffer));
	strncpy(mail->title, buffer, sizeof(mail->title)-1);
	remove_escapes(mail->title);

	//Attachment Size auslesen
	bzero(buffer, sizeof(buffer));
	readline(socket, buffer, sizeof(buffer)-1);
	remove_escapes(buffer);
	mail->a_size = strtol(buffer, NULL, 10);

	//Attachment auslesen falls es eine gibt
	if(mail->a_size > 0){
		bzero(buffer, sizeof(buffer));

		//Attachment Name auslesen
		readline(socket, buffer, sizeof(buffer)-1);
		remove_escapes(buffer);
		strncpy(mail->a_name, buffer, sizeof(mail->a_name)-1);

		//Attachment Content streamen
		mail->a_content = (char *)malloc(mail->a_size * sizeof(char)+1);
		recv(socket, mail->a_content, mail->a_size + 1, 0);
		mail->a_content[strlen(mail->a_content)-1] = '\0';
	}

	mail->message = (char *)malloc(BUF * sizeof(char));
	bzero(mail->message, sizeof(mail->message));

	//Message zeilenweise auslesen bis zum '.'
	while(1){
		bzero(buffer, sizeof(buffer));
		readline(socket, buffer, BUF);

		if(!strncmp(buffer, ".\n", strlen(buffer)))
			break;

		if(!append(&mail->message, buffer)){
			perror("Memory for message could not be allocated! Abort mail-receiving...");
			err = 1;
			break;
		}
	}

	//Letztes "\n" am Ende der Message wegkuerzen
	mail->message[strlen(mail->message)-1] = '\0';

	if(err)
		return 0;

	return 1;
}
