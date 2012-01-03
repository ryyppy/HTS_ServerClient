/* myserver.c */
#include "myserver.h"


pthread_mutex_t write_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Hauptprogramm
 * Hoert auf eine Socketverbindung und stellt den Socket fuer die Grundfunktionen bereit
 * Bei Verbindungseingang wird ein neuer Thread erzeugt, der den Client mit dem Server kommunizieren laesst
 *
 * -- Kommandozeilenparameter
 * 1: Port des Servers
 */
int main (int argc, char **argv) {
	int create_socket, new_socket;
	socklen_t addrlen;
	uint port = PORT;

	if(argc > 2){
		printf("Usage: %s PORT\n", argv[0]);
		return EXIT_FAILURE;
	}

	if(argc == 2){
		port = strtol(argv[1], NULL, 10);
	}


	//Serverordner
	char spooldir [50] = "spool";
	char lockdir [50] = "lock";

	//Speicherbereiche fuer neue Verbindungseingaenge
	struct sockaddr_in address, cliaddress;
	time_t resttime;
	int lock_ret = -1;

	char buffer[BUF] = "\0";
	char ip[BUF] = "\0";

	//LDAP Laufvariablen
	LDAP *ld;

	/* setup LDAP connection */
	if ((ld=ldap_init(LDAP_HOST, LDAP_PORT)) == NULL){
		perror("ldap_init failed");
		return EXIT_FAILURE;
	}

	if(prepare_server(spooldir, lockdir) != 0){
		perror("Spool and/or Lock-Directory could not be created or dont exist");
		return EXIT_FAILURE;
	}

	create_socket = socket (AF_INET, SOCK_STREAM, 0);

	bzero(&address, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons (port);

	if (bind ( create_socket, (struct sockaddr *) &address, sizeof (address)) != 0) {
		sprintf(buffer, "Could not open socket on %s:%d", inet_ntoa (address.sin_addr), port);
		perror(buffer);
		return EXIT_FAILURE;
	}
	listen (create_socket, 5);

	printf("Server is listening on %s:%d\n", inet_ntoa (address.sin_addr), (int) ntohs(address.sin_port));
	addrlen = sizeof (struct sockaddr_in);

	while (1) {
		printf("Waiting for connections...\n");
		new_socket = accept ( create_socket, (struct sockaddr *) &cliaddress, &addrlen );

		if (new_socket > 0)
		{
			//Ueberpruefung ob der Client gesperrt ist
			printf("Client with address %s:%d tries to connect...\n", inet_ntoa(cliaddress.sin_addr),(int) ntohs(cliaddress.sin_port));

			strncpy(ip, inet_ntoa(cliaddress.sin_addr), sizeof(ip)-1);
			lock_ret = check_ip_lock(ip, lockdir, &resttime);

			//Falls das File noch gelockt ist
			if(lock_ret == 1){
				bzero(buffer, sizeof(buffer));
				sprintf(buffer, "IPLOCKED\n%ld\n", (long int)resttime);
				send_msg_to_client(new_socket, buffer);
				close(new_socket);
				continue;
			}

			if(lock_ret == 0){
				unlock_ip(ip, lockdir);
			}

			//IP nicht gesperrt -> Thread erstellen
			ThreadArguments arg;
			arg.socket = new_socket;
			arg.spooldir = spooldir;
			arg.cliaddress = cliaddress;
			arg.ld = ld;
			arg.lockdir = lockdir;

			pthread_attr_t attr;
			pthread_t new_thread;

			//Attribute fuer einen DAEMON-Threads initialisieren und setzen
			pthread_attr_init(&attr);
			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

			pthread_create(&new_thread, &attr, thread_run, &arg);
		}
	}
	ldap_unbind(ld);
	close (create_socket);
	pthread_exit(NULL);

	printf("Server stopped...");
	return EXIT_SUCCESS;
}

/**
 * Run-Delegate fuer einen Serverthread
 * Bekommt als Argument eine ArgumentStructur, in der Systemordnerpfade und Socketdescriptor enthalten sind
 * - void* arg : ThreadArguments *arg (ThreadArguments-Struktur mit entsprechenden Parametern)
 */
void *thread_run(void* arg){
	ThreadArguments *ta = (ThreadArguments *)arg;

	//Connection-Laufvariablen
	int socket = ta->socket;
	char *spooldir = ta->spooldir;
	char *lockdir = ta->lockdir;

	struct sockaddr_in cliaddress = ta->cliaddress;

	LDAP *ld = ta->ld;

	char buffer[BUF];
	int size;

	//Informationen fuer Login
	char username[10] = "\0";
	char maildir[255];
	int attempts = 0;
	int loggedin = 0;
	char ip[BUF] =  "\0";

	strncpy(ip, inet_ntoa (cliaddress.sin_addr), sizeof(ip)-1);
	printf ("Client %s:%d authorized to operate\n", ip,ntohs(cliaddress.sin_port));

	strcpy(buffer,"Welcome to myserver, Please enter your command:\n");
	send(socket, buffer, strlen(buffer),0);

	do {

		bzero(buffer, sizeof(buffer));
		size = readline(socket, buffer, sizeof(buffer)-1);

		if( size > 0)
		{
			//Falls der Benutzer noch nicht eingeloggt ist
			if(!strncmp(buffer, "LOGIN", 5) && !loggedin){
				//Falls der Login erfolgreich ist wird der validierte username verwendet
				if(login_procedure(socket, ld, username) || FAKE_LOGIN){
					sprintf(maildir, "%s/%s", spooldir, username);
					loggedin = 1;
					send_msg_to_client(socket, "OK\n");
				}
				else{
					attempts++;

					if(attempts == 3){
						lock_ip(ip, lockdir);
						send_msg_to_client(socket, "LOCKED\n");
						break;
					}

					send_msg_to_client(socket, "ERR\n");
				}
			}
			else{
				if(!loggedin){
					//Falls vorher nicht eingeloggt wurde, wird ERR zu jedem anderen BEFEHL zurueckgesendet
					send_msg_to_client(socket, "ERR\n");
				}
				else if(!strncmp(buffer, "LOGOUT", 6)){
					loggedin = 0;
					bzero(username, sizeof(username));
					bzero(maildir, sizeof(maildir));
					send_msg_to_client(socket, "LOGOUT\n");
				}
				else if(!strncmp(buffer, "SEND", 4)){
					send_procedure(socket, spooldir, username);
				}
				else if(!strncmp(buffer, "LIST", 4)){
					list_procedure(socket, spooldir, username);
				}
				else if(!strncmp(buffer, "READ", 4)){
					read_procedure(socket, spooldir, username);
				}
				else if(!strncmp(buffer, "DEL", 3)){
					del_procedure(socket, spooldir, username);
				}
				else{
					sprintf(buffer, "Unknown Command: %s", buffer);
					send_msg_to_client(socket, buffer);
				}
			}

		}
		else if (size == 0)
		{
			printf("Client closed remote socket\n");
			break;
		}
		else
		{
			perror("recv error");
			return NULL;
		}
	} while (strncmp (buffer, "quit", 4)  != 0);

	printf ("Client disconnected from %s:%d...\n", inet_ntoa (cliaddress.sin_addr),ntohs(cliaddress.sin_port));
	close (socket);
	
	pthread_exit(NULL);
	return NULL;
}

/**
 * Ueberprueft, ob der Spool- und Lock-Ordner existiert
 * Falls nicht, wird dieser angelegt
 * - char * spooldir - Mailordner-Pfad, die bei nicht-existenz angelegt wird
 * - char * lockdir - Lockordner-Pfad, der bei nicht-existenz angelegt wird
 */
int prepare_server(char *spooldir, char *lockdir){
	struct stat st;
	int ret = 0;

	if(stat(spooldir, &st) == -1)
		ret = mkpath(spooldir, 0777);

	if(stat(lockdir, &st) == -1)
		ret = mkpath(lockdir, 0777);

	return ret;
}

/**
 * Sendet ueber den uebergebenen Socket einen String
 * - int socket - Filedescriptor zum Socket
 * - char* msg - String, der gesendet werden soll
 */
void send_msg_to_client(int socket, char *msg){
	write(socket, msg, strlen(msg));
}

/**
 * Startet die Login-Prozedur
 * Es werden folgende Daten am Server erwartet:
 * <Benutzername>\n
 * <Passwort>\n
 *
 * Nach erfolgreichen Login wird der Name des eingeloggten Nutzers in *username gespeichert und 1 zurueckgeliefert.
 * Bei LDAP-Fehlern oder ungueltige Username/PW Kombination wird 0 zurueckgeliefert.
 *
 * - *username - char-Array mit mindst. 8 Zeichen, in den der Name des eingeloggten Benutzers gespeichert wird.
 */
int login_procedure(int socket, LDAP *ld, char *username){
	//LDAP Laufvariablen
	LDAPMessage *result = NULL, *e = NULL;
	int rc = 0;
	int ret = 0;

	char *attribs[3];

	attribs[0]=strdup("uid");
	attribs[1]=strdup("cn");
	attribs[2]=NULL;

	//Readvariablen
	char uname[10];
	char password[255];
	char filter[15];

	//Username und Passwort aus dem Socket lesen
	readline(socket, uname, sizeof(uname));
	remove_escapes(uname);
	
	readline(socket, password, sizeof(password));
	remove_escapes(password);

	//Fakelogin Hilfe um ohne LDAP einen Login-Namen zu haben
	if(FAKE_LOGIN){
		strcpy(username, uname);
		free(attribs[0]);
		free(attribs[1]);
		return 1;
	}

	//Schritt 1 : Anonymes Login
	rc = ldap_simple_bind_s(ld,NULL,NULL);

	//Falls die LDAP verfuegbar ist
	if (rc == LDAP_SUCCESS){
		//Schritt 2 : Benutzernamen in LDAP herausfiltern
		sprintf(filter, "(uid=%s)", uname);
		rc = ldap_search_s(ld, SEARCHBASE, SCOPE, filter, attribs, 0, &result);

		//Falls hier Success, dann existiert der eingegebene Benutzername in der LDAP
		if (rc == LDAP_SUCCESS){
			//Da nur max. 1 Eintrag existieren kann wird nur der erste Entry verwendet
			e = ldap_first_entry(ld, result);

			//Schritt 3: Konkreter Loginversuch, falls der erste Eintrag zurueckgeliefert wurde
			if(e != NULL){
				rc = ldap_simple_bind_s(ld, ldap_get_dn(ld, e), password);
				if(rc == LDAP_SUCCESS){
					//Eingeloggten Username im uebergebenen Char-Array speichern
					strcpy(username, uname);
					ret = 1;
				}
			}

		}
	}

	//Allocierten Speicher freigeben
	if(result != NULL)
		ldap_msgfree(result);

	free(attribs[0]);
	free(attribs[1]);
	bzero(password, sizeof(password));

	return ret;
}

/**
 * Der Aufruf dieser Funktion laesst das Programm in den SEND-Modus switchen
 * DH. das Programm wartet auf folgende mit '\n'-getrennte Strings (Socket):
 * <Sender>\n (Platzhalter fuer Sender - wird hier allerdings nicht benoetigt)
 * <Receiver [max 8 Zeichen]>;<Receiver>;....;<Receiver>\n (max. MAX_RECEIVERS -> myutil.h)
 * <Title [max. 80 Zeichen]>\n
 * <Attachment-size [in Bytes]>\n
 * <Attachment-name falls [size > 0]>\n
 * <Attachment-content falls [size > 0]>\n
 * <Message [x Zeilen]>\n
 * .\n
 *
 * Danach wird im Spool-Ordner ein Ordner fuer den Empfaenger angelegt (falls dieser noch nicht existiert)
 * und die Nachricht in einer Datei (numeriert) in diesem Ordner gespeichert.
 * - int socket - Socket-Descriptor, ueber den die Daten fuer die Mail abgerufen werden
 * - char *spooldir - Spool-Dir-Pfad, in dem die Mails in der entsprechenden Hierarchie gespeichert werden
 * - char *username - Name des Users, der die Mail versendet (deswegen entfaellt das <Sender>-Feld)
 * Der Server sendet dem angebundenen Client ein "OK\n" falls die Nachricht gespeichert wurde, ansonsten "ERR\n"
 */
void send_procedure(int socket, char *spooldir, char *username){
	int error = 0;
	
	//Nachricht allocieren
	Mail *mail = (Mail *)malloc(sizeof(Mail));
	mail->a_content = NULL;
	mail->message = NULL;


	//Mail als Datei speichern, falls bis jetzt keine Fehler aufgetreten ist
	if(receive_mail(socket, mail)){
		strncpy(mail->sender, username, sizeof(mail->sender) - 1); //Sender setzen

		//Fuer jeden Empfaenger ein eigenes Nachrichtenfile anlegen
		int i = 0;
		char* single_receiver;
		single_receiver = strtok (mail->receiver,";");
		while (single_receiver != NULL && i < MAX_RECEIVERS)
		{
			char dir[255];
			char filename[255];

			sprintf(dir, "%s/%s", spooldir, single_receiver);

			int count;
			struct stat st;
			DIR *d;
			FILE *fp;

			//Falls der Ordner nicht existiert fuer den User, wird ein Ordner angelegt
			if(stat(dir, &st) == -1)
				mkdir(dir, 0777);


			//Files zaehlen und neues File mit "Count + 1" Filenamen erstellen
			if((d = opendir(dir)) != NULL){
				for(count = -2;  readdir(d) != NULL; count++);
				closedir(d);

				//Integer in String umwandeln (Count ist gleichzeitig der Filename)
				sprintf(filename, "%d", count);

				//Name der Zieldatei fuer die Schreiboperationen zusammenfuegen (SPOOL_DIR/USERNAME/1,2,3....)
				char *target = malloc(strlen(dir) + strlen(filename) + 1);
				sprintf(target, "%s/%s", dir, filename);

				//Mail in Datei schreiben
				pthread_mutex_lock(&write_mutex);
				fp = fopen(target, "w");

				//Je nachdem ob ein Attachment existiert, das File beschreiben
				if(mail->a_size > 0){
					fprintf(fp, "%s\n%s\n%ld\n%s\n%s\n%s", mail->sender,mail->title,mail->a_size, mail->a_name, mail->a_content, mail->message);
				}
				else{
					fprintf(fp, "%s\n%s\n%ld\n%s", mail->sender,mail->title,mail->a_size, mail->message);
				}

				fclose(fp);
				pthread_mutex_unlock(&write_mutex);

				free(target);
			}


			//Naechsten receiver ermitteln
			single_receiver = strtok (NULL, ";");
		}
	}

	//Aufraeumen
	if(mail->a_content != NULL)
		free(mail->a_content);

	if(mail->message != NULL)
		free(mail->message);

	free(mail);

	if(!error){
		send_msg_to_client(socket, "OK\n");
	}
	else{
		send_msg_to_client(socket, "ERR\n");
	}
}

/**
 * Der Aufruf dieser Funktion versetzt den Server in den LIST-Modus
 * Der Server liefert Informationen ueber die Mailbox des eingeloggten Users
 * Falls keine Mails existieren liefert er "0\n" zurueck, ansonsten folgenden Output:
 * <Anzahl der Mails>\n
 * <Mail1 - Betreff>\n
 * ....
 * <Mailn - Betreff>\n
 *
 * - int socket - Socket-Descriptor, auf dem gelesen und geschrieben wird
 * - char *spooldir - Spool-Dir-Pfad, von dem die Mails der Benutzer gelesen werden
 * - char *username - Name des eingeloggten Users, dessen Emails gelistet werden sollen
 */
void list_procedure(int socket, char* spooldir, char *username){
	char buffer[BUF];
	char userdir[255];

	char *message = (char *)malloc(BUF * sizeof(char));
	char fullpath[255];

	struct stat st;
	DIR *d;
	FILE *fp;
	struct dirent *dp;

	sprintf(userdir, "%s/%s", spooldir,username);

	if(stat(userdir, &st) == -1)
			sprintf(message, "0\n");

	if((d = opendir(userdir)) != NULL){
		int count;
		for (count=-2; (readdir (d)) != NULL; count++);
		sprintf(message, "%d\n", count);

		rewinddir(d);
		while ((dp = readdir (d)) != NULL) {
			sprintf(fullpath, "%s/%s", userdir, dp->d_name);

			fp = fopen(fullpath, "r");

			int line = 1;
			while ((fgets(buffer, sizeof(buffer), fp)) != NULL) {
				if(line == 2){
					append(&message, buffer);
					break;
				}
				line++;
			}
			fclose(fp);
		}
		closedir(d);
	}
	send_msg_to_client(socket, message);

	if(message != NULL)
		free(message);
}

/***
 * Der Aufruf dieser Funktion versetzt den Server in den READ-Modus
 * Hier erwartet der Server vom Socket folgende Informationen (mit '\n' getrennt):
 * <Sender>\n (Platzhalter fuer Sender - wird hier allerdings nicht benoetigt)
 * <Receiver [max 8 Zeichen]>\n
 * <Title [max. 80 Zeichen]>\n
 * <Attachment-size [in Bytes]>\n
 * <Attachment-name falls [size > 0]>\n
 * <Attachment-content falls [size > 0]>\n
 * <Message [x Zeilen]>\n
 *
 * - int socket - Socket-Descriptor, mit dem am Socket gelesen und geschrieben wird
 * - char *spooldir - Spool-Dir-Pfad, in dem die Mails gesucht und gelesen werden
 * - char *username - Benutzername, dessen Mail ausgelesen wird
 *
 * Bei erfolgreichem Lesen der Mail werden die Informationen im folgenden Format am Socket geschrieben:
 * SEND\n
 * <Sender [max. 8 Zeichen]>\n
 * <EmpfŠnger>\n
 * <Betreff [max. 80 Zeichen]>\n
 * <Attachment-size [in Bytes]>\n
 * <Attachment-name falls [size > 0]>\n
 * <Attachment-content falls [size > 0]>\n
 * <Nachricht [x Zeilen]>\n
 * .\n
 *
 * Bei einem Fehler wird "ERR\n" gesendet
 */
void read_procedure(int socket, char *spooldir, char *username){
	char buffer[BUF];

	char userdir[255];
	Mail * mail;
	int msg_num;

	int error=1;

	char fullpath[255];

	struct stat st;
	DIR *d;
	FILE *fp;
	struct dirent *dp;

	sprintf(userdir, "%s/%s", spooldir,username);

	readline(socket, buffer, sizeof(buffer)-1);
	msg_num = strtol(buffer,NULL,10) - 1; //Minus 1, da nullbasierend

	if((stat(userdir, &st) != -1) && ((d = opendir(userdir)) != NULL)){
		int count;
		for (count=-2;(dp = readdir (d)) != NULL;count++) {
			if(count < 0 || count != msg_num)
				continue;

			sprintf(fullpath, "%s/%s", userdir, dp->d_name);

			if((fp = fopen(fullpath, "r")) != NULL){
				bzero(buffer, sizeof(buffer));
				mail = (Mail *)malloc(sizeof(Mail));
				mail->a_content = NULL;
				mail->message = NULL;
				strcpy(mail->receiver, username);
				mail->message = (char *)malloc(sizeof(char) * BUF);

				//Die Maildatei auslesen und in der Mailstruktur speichern
				fgets(buffer, sizeof(buffer)-1, fp);
				remove_escapes(buffer);
				strncpy(mail->sender, buffer, sizeof(mail->sender)-1);

				fgets(buffer, sizeof(buffer)-1, fp);
				remove_escapes(buffer);
				strncpy(mail->title, buffer, sizeof(mail->title)-1);

				fgets(buffer, sizeof(buffer)-1, fp);
				remove_escapes(buffer);
				mail->a_size = strtol(buffer, NULL, 10);


				if(mail->a_size > 0){
					bzero(buffer, sizeof(buffer));

					//Attachment Name auslesen
					fgets(buffer, sizeof(buffer)-1, fp);
					remove_escapes(buffer);
					strncpy(mail->a_name, buffer, sizeof(mail->a_name)-1);

					//Attachment Content auslesen
					mail->a_content = (char *)malloc(mail->a_size * sizeof(char) + 1);
					fread (mail->a_content,1,mail->a_size, fp);
					mail->a_content[strlen(mail->a_content)] = '\0';
					fread(buffer, 1,1, fp); //Ein unnoetiges \n auslesen
				}

				mail->message = (char *)malloc(BUF * sizeof(char));
				while(1){
					bzero(buffer, sizeof(buffer));
					if(fread(buffer, 1, sizeof(buffer)-1, fp) <= 0)
						break;

					append(&mail->message, buffer);
				}

				//Mail an den Client zuruecksenden
				send_mail(socket, mail);

				//Aufraeumen1
				if(mail->message != NULL)
					free(mail->message);

				if(mail->a_content != NULL)
					free(mail->a_content);

				free(mail);
				fclose(fp);
				error=0;
				break;
			}
		}
		closedir(d);
	}

	if(error)
		send_msg_to_client(socket, "ERR\n");
}

/***
 * Der Aufruf dieser Funktion versetzt den Server in den DEL-Modus
 * Hier erwartet der Server vom Socket folgende Informationen (mit '\n' getrennt):
 * <Mail-Nummer>\n
 *
 * - int socket - Socket-Descriptor, mit dem am Socket gelesen und geschrieben wird
 * - char *spooldir - Spool-Dir-Pfad, in dem die Mails gesucht und herausgeloescht werden
 * - *char username - Benutzername, der diesen DEL-Befehl zum Loeschen eigener Emails verwendet
 * Falls die gewuenschte Mail nicht geloescht werden kann liefert der Server ueber den Socket ein "ERR\n" zurueck, ansonsten ein "OK\n"
 */
void del_procedure(int socket, char *spooldir, char *username){
	char buffer[BUF];
	char userdir[255];
	int msg_num;
	int error=1;

	char fullpath[255];

	struct stat st;
	DIR *d;
	struct dirent *dp;

	sprintf(userdir, "%s/%s", spooldir,username);

	bzero(buffer, sizeof(buffer));
	readline(socket, buffer, BUF);
	msg_num = atoi(buffer)-1;

	if((stat(userdir, &st) != -1) && ((d = opendir(userdir)) != NULL)){
		int count;
		for (count=-2;(dp = readdir (d)) != NULL;count++) {
			if(count < 0 || count != msg_num)
				continue;

			sprintf(fullpath, "%s/%s", userdir, dp->d_name);

			if(remove(fullpath) == 0){
				error=0;
				send_msg_to_client(socket, "OK\n");
			}
			break;
		}
	}
	closedir(d);

	if(error)
		send_msg_to_client(socket, "ERR\n");
}
/**
 * UTILITY FUNCTIONS
 */

/**
 * Utilityfunktion fuer mkpath
 */
static int do_mkdir(const char *path, mode_t mode)
{
    struct stat st;
    int status = 0;

    if (stat(path, &st) != 0)
    {
        /* Directory does not exist */
        if (mkdir(path, mode) != 0)
            status = -1;
    }
    else if (!S_ISDIR(st.st_mode))
    {
        errno = ENOTDIR;
        status = -1;
    }

    return(status);
}

/**
** mkpath - ensure all directories in path exist
** Algorithm takes the pessimistic view and works top-down to ensure
** each directory in path exists, rather than optimistically creating
** the last element and working backwards.
*/
int mkpath(const char *path, mode_t mode){
    char *pp;
    char *sp;
    int  status;
    char *copypath = strdup(path);

    status = 0;
    pp = copypath;
    while (status == 0 && (sp = strchr(pp, '/')) != 0)
    {
        if (sp != pp)
        {
            /* Neither root nor double slash in path */
            *sp = '\0';
            status = do_mkdir(copypath, mode);
            *sp = '/';
        }
        pp = sp + 1;
    }
    if (status == 0)
        status = do_mkdir(path, mode);
    free(copypath);
    return (status);
}

int check_ip_lock(char *ip, char *lockdir, time_t *rest_seconds){
    FILE *fp;
    char *path = NULL;
    char buffer[BUF];

    time_t systime;
    time_t iptime;

    int ret = -1; // -1 = Not locked , 0 = Locked but time expired, 1 = locked
    systime = time(NULL);

    asprintf(&path, "%s/%s", lockdir, ip);

    if((fp = fopen(path, "r")) != NULL){
		fread(buffer, 1, sizeof(buffer), fp);
		iptime = strtol(buffer, NULL, 10);
		fclose(fp);

		//1320502739 - 1320500953 = 1768
		*rest_seconds = ((unsigned long int)iptime - (unsigned long int)systime);
		ret = ((*rest_seconds > 0 && *rest_seconds <= IP_LOCK_SECONDS) ? 1 : 0);
    }

    free(path);
    return ret;
}

/**
 * Erstellt ein Lockfile im Lock-Dir fuer die angegebene IP
 * - char *ip - IP, die gesperrt werden soll
 * - char *lockdir - Pfad des Lockdirs, in dem die Lockfiles enthalten sind
 */
void lock_ip(char *ip, char *lockdir){
    FILE *fp;
    char *path = NULL;
    time_t systime = time(NULL);

    systime += IP_LOCK_SECONDS;

    asprintf(&path, "%s/%s", lockdir, ip);

    if((fp = fopen(path, "w")) != NULL){
    	fprintf(fp, "%ld", systime);
    	fclose(fp);
    }

    free(path);
}

/**
 * Hebt das Lockfile dass mit lock_ip erstellt wurde auf.
 * - char *ip - IP, die entsperrt werden soll
 * - char *lockdir - Pfad des Lockdirs, in dem das Lockfile enthalten ist
 * - Liefert 1 bei Erfolg, ansonsten 0
 */
int unlock_ip(char *ip, char *lockdir){
	char fullpath[BUF];

	sprintf(fullpath, "%s/%s", lockdir, ip);
	return !remove(fullpath);
}
