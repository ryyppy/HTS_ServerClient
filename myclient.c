/* myclient.c */
#include "myclient.h"

//PROTOKOLLE Client -> Server
/**
 * SEND\n[Sender 8]\n[Receiver 8]\n[Title 80]\n[Attachmentsize]\n[Attachmentname(Size>0)]\n[Attachmentcontent(Size>0)]\n[Message]\n.\n
 * LIST\n
 * READ\n[mailnumber]\n
 * DEL\n[mailnumber]\n
 * LOGOUT\n
**/

/**
 * Hauptprogramm des Clients fuer eine Einzelverbindung mit dem Server.
 * Falls bereits ein Client mit dem Server verbunden ist, wartet das Programm bis ein Socket frei wird
 * -- Kommandozeilenparameter
 * 1: IP des Servers
 * 2: Port des Servers
 *
 * Es wird standardmaeszig der Port 6543 bzw. der uebergebene Port + IP fuer die Verbindung herangezogen
 *
 * Nachrichten werden clientseitig gepuffert und in einem Satz ueber den Socket zum Server gesendet.
 */
int main (int argc, char **argv) {
	int my_socket;
  	char buffer[BUF];
  	uint port = PORT;

  	//Credentials
  	int loggedin = 0;
  	char username[9] = "\0";

  	struct sockaddr_in address;
  	//int size;

  	if( argc < 2 || argc > 3){
    	printf("Usage: %s IP [PORT=%d]\n", argv[0], PORT);
     	exit(EXIT_FAILURE);
  	}

  	if(argc == 3)
  		port = strtol(argv[2], NULL, 10);

  	if ((my_socket = socket (AF_INET, SOCK_STREAM, 0)) == -1){
     	perror("Socket error");
     	return EXIT_FAILURE;
  	}
  
  	memset(&address,0,sizeof(address));
  	address.sin_family = AF_INET;
  	address.sin_port = htons (port);
  	inet_aton (argv[1], &address.sin_addr);

  	if (connect ( my_socket, (struct sockaddr *) &address, sizeof (address)) == 0){
     	printf ("Connection with server (%s) established\n", inet_ntoa (address.sin_addr));

     	//size=recv(my_socket,buffer,BUF-1, 0);
     	//recv(my_socket, buffer, sizeof(buffer)-1, 0);
     	readline(my_socket, buffer, sizeof(buffer));

     	if(!strncmp(buffer, "IPLOCKED", 8)){
     		//Restzeit auslesen
     		readline(my_socket, buffer, strlen(buffer));

     		time_t t = strtol(buffer, NULL, 10);
     		int minutes = t/60;
     		int hours = minutes/60;
     		int seconds = (t - (hours * 3600) - (minutes * 60));

     		puts("-----------SORRY--------------");
     		puts("Your IP is locked due too many login attemps!");
     		printf("Hours: %d - Minutes: %d - Seconds: %d\n", hours, minutes, seconds);

     		//Verhindern, dass der Login-Process gestartet wird und die Schleife unterbrechen damit der Socket schliesst
     		loggedin = -1;
     	}
     	else{
     		puts(buffer);
     	}


  	}
  	else{
  		sprintf(buffer, "No Server found on %s:%d\n", inet_ntoa (address.sin_addr), port);
     	perror(buffer);
     	return EXIT_FAILURE;
  	}

  	while(1){
		//Falls der Benutzer nicht eingeloggt ist muss er sich einloggen
		while(!loggedin){
			loggedin = login_procedure(my_socket, username);
			
			if(loggedin == -1)
				break;

			if(loggedin == 0){
				printf("\n[Press Enter to continue]");
				getchar();
			}
		}
		
		if(loggedin == -1)
			break;

		system("clear");
		//Ab hier ist er eingeloggt und kann Befehle ausfuehren
		printf("HELLO %s !\n\n--------- YOUR COMMAND--------\n",username);
		printf("LIST\nSEND\nREAD\nDEL\nLOGOUT\nQUIT\n--> ");
		
		bzero(buffer, sizeof(buffer));

     	fgets (buffer, sizeof(buffer), stdin);
		puts("------------------------------");

     	if(!strncmp(buffer, "QUIT", 4)){
    	 	break;
		}	
     	else if(!strncmp(buffer, "SEND", 4)){
			send_procedure(my_socket);
			printf("\n[Press Enter to continue]");
			getchar();
     	}
     	else if(!strncmp(buffer, "LIST", 4)){
     		list_procedure(my_socket);
     		printf("\n[Press Enter to continue]");
     		getchar();
     	}
     	else if(!strncmp(buffer, "DEL", 3)){
     		del_procedure(my_socket);
     		printf("\n[Press Enter to continue]");
     		getchar();
     	}
     	else if(!strncmp(buffer, "READ", 4)){
     		read_procedure(my_socket);
     		printf("\n[Press Enter to continue]");
     		getchar();
     	}
		else if(!strncmp(buffer, "LOGOUT", 6)){
			if(logout_procedure(my_socket)){
				loggedin = 0;
				puts("Thank you, you are logged out successfully!");
			}
			else{
				printf("Sorry, something went wrong when trying to log out! Please try again...");
			}
			printf("\n[Press Enter to continue]");
			getchar();
		}
  	} 

  	send(my_socket, "quit\n",5, 0);
  	close (my_socket);

  	puts("Connection closed successfully! Thanks and see you soon!\n");
  	return EXIT_SUCCESS;
}

/**
* Fragt den Benutzer ueber STDIN nach dem Benutzernamen + zugehoerigen Passwort
* Liefert 1, falls sich der Benutzer erfolgreich eingeloggt hat, 0 falls ein Loginversuch fehlgeschlagen hat
* und -1, falls der Server die IP aufgrund von zu vielen Loginversuchen gesperrt hat
**/
int login_procedure(int socket, char *username){
	char buffer[BUF] = "\0";
	char response[BUF] = "\0";
	char uname[9] = "\0";
	char *password = NULL;
	char *message = NULL;
	int size = 0;
	
	system("clear");
	puts("----------------YOUR LOGIN------------------");
	printf("Username: ");
	fgets(buffer, sizeof(buffer), stdin);
	strncpy(uname, buffer, sizeof(uname)-1);
	remove_escapes(uname);
	
	password = getpass("Password: ");
	asprintf(&message, "LOGIN\n%s\n%s\n", uname, password);
	
	//Passwort komplett aus dem Speicher loeschen
	bzero(password, sizeof(password));
	
	send(socket, message,strlen(message), 0);
	free(message);
	
	size = recv(socket,response,sizeof(response), 0);
	
	//Falls der Login erfolgreich war
	if(!strncmp(response, "OK", 2)){
		strcpy(username, uname);
		return 1;
	}
	
	puts("--------------------------------------------");

	//Falls der Server die IP gesperrt hat
	if(!strncmp(response, "LOCKED", 6)){
		puts("Too many wrong login-attempts. IP blocked!");
		return -1;
	}

	//Falls der Login unerfolgreich war
	if(!strncmp(response, "ERR", 3)){
		puts("Wrong username or password. Try again...");
		return 0;
	}

	//Falls der Server eine andere Antwort sendet als geplant
	printf("\nA non-common response occurred due login -> %s\n", response);
	return 0;
}

/**
 * Startet den Dialog zur Sende-Prozedur fuer den eingeloggten Benutzer
 * Es muessen zuerst die Empfaenger angegeben werden (max. MAX_RECEIVERS Empfaenger -> siehe myutil.h), es koennen dabei
 * keine doppelten Empfaenger eingegeben werden. Ausserdem werden max. 8 Zeichen pro Empfaenger akzeptiert.
 * Danach muss der Betreff eingegeben werden, der max. 80 Zeichen lang sein darf.
 * Danach folgt die Nachricht, die unlimitiert beschrieben werden kann und sie wird mit ".\n" abgeschlossen.
 * Es kann ein Attachment angegeben werden, indem ein gueltiger Pfad angegeben wird.
 * Nach dem Senden wird ueberprueft ob der Server "OK\n" oder "ERR\n" zuruecksendet.
 */
void send_procedure(int socket){
	int i = 0; //Laufvariable fuer die maximale Receivereingabe

	//Fuer die Receiver Zwischenspeicherung
	char r[9];
	char *all_receiver = (char *)malloc(MAX_RECEIVERS * 10 * sizeof(char));

	Mail *mail = (Mail *)malloc(sizeof(Mail));
	mail->a_content = NULL;
	mail->message = NULL;
	strcpy(mail->receiver, "\0");
	
	FILE *fp = NULL;

	char response[BUF] = "\0";
	char buffer[BUF] = "\0";
	
	for(i=0; i < MAX_RECEIVERS; i++){
		printf("%d. receiver (stop with 'enter'): ", i+1);
		fgets(buffer, sizeof(buffer)-1, stdin);
		remove_escapes(buffer);

		//Verhindern dass kein Receiver angegeben wurde
		if(!strcmp(buffer, "") && i <= 0){
			puts("Please enter at least 1 receiver");
			i--;
			continue;
		}

		if(!strcmp(buffer, "")){
			break;
		}


		strncpy(r, buffer, sizeof(r)-1);

		if(strstr(all_receiver, r) != NULL){
			puts("You may enter each receiver once");
			i--;
			continue;
		}

		if(i > 0)
			append(&all_receiver, ";");


		r[8] = '\0';
		append(&all_receiver, r);
	}
	strncpy(mail->receiver, all_receiver, sizeof(mail->receiver)-1);

	//Es muss ein Titel angegeben werden
	while(1){
		bzero(buffer, sizeof(buffer));

		printf("Title: ");
		fgets(buffer, sizeof(buffer)-1, stdin);
		strncpy(mail->title, buffer, sizeof(mail->title)-1);
		remove_escapes(mail->title);

		if(strlen(mail->title) > 0)
			break;

		puts("You have to enter a title");
	}

	//Eingabe der Nachricht
	mail->message = (char *)malloc(BUF * sizeof(char));
	puts("Message (end with '.'): ");
	while(1){
		bzero(buffer, sizeof(buffer));
		fgets(buffer, sizeof(buffer), stdin);

		//Bei Nachrichtenende '.' die Eingabe stoppen
		if(!strncmp(buffer, ".\n", strlen(buffer)))
			break;

		append(&mail->message, buffer);
	}

	//Letztes \n der Messge wegkuerzen
	mail->message[strlen(mail->message)-1] = '\0';
	
	while(1){
		bzero(buffer, sizeof(buffer));
		printf("Path to attachment file (blank for skipping): ");
		fgets(buffer, sizeof(buffer)-1, stdin);

		//Newlines herausloeschen
		remove_escapes(buffer);
		
		//Falls Leereingabe -> Kein Attachment
		if(buffer[0] == '\0'){
			break;
		}

		//File oeffnen
		fp = fopen(buffer, "r");
		
		//Filecontent auslesen falls moeglich
		if(fp != NULL){
			strcpy(mail->a_name, basename(buffer));

			//Filegroesse 
			fseek (fp , 0 , SEEK_END);
			mail->a_size = ftell (fp);
  			rewind (fp);
			
			mail->a_content = (char *)malloc (sizeof(char) * mail->a_size);
			if(mail->a_content == NULL){
				printf("File too big, not enough memory...");
			}
			
			if(fread(mail->a_content,1,mail->a_size, fp) != mail->a_size){
				printf("Error in reading file...");
			}	
			else{
				break;
			}
			
			fclose(fp);
		}	
	}
	
	//SEND Befehl zusammenbauen und an den Server senden
	send_mail(socket, mail);

	//Antwort empfangen und ausgeben
	recv(socket,response,sizeof(response), 0);	

	puts("\n------------------------------");
	printf("\nAnswer from Server: %s", response);
	
	//Aufraeumen
	if(mail->message != NULL)
		free(mail->message);
		
	if(mail->a_content != NULL)
		free(mail->a_content);

	free(all_receiver);
	free(mail);
}

/**
 * Gibt die Anzahl der Mails in der Mailbox des eingeloggten Users aus und danach
 * Zeilenweise und nummeriert die Emails mit Betreff.
 */
void list_procedure(int socket){
	int messages = 0;
	char buffer[BUF] = "\0";
	char message[] = "LIST\n";

	send(socket, message, strlen(message), 0);

	readline(socket, buffer, sizeof(buffer)-1);
	remove_escapes(buffer);

	//Zuerst den Messagecount empfangen und ausgeben
	messages = strtol(buffer, NULL, 10);
	printf("Message count: %d\n", messages);

	//Die Betreffs der Nachrichten zeilenweise ausgeben
	if(messages > 0){
		int i = 0;
		for(i = 1; i <= messages; i++){
			bzero(buffer, sizeof(buffer));
			readline(socket,buffer, sizeof(buffer)-1);
			printf("%d. Message: %s",i, buffer);
		}
	}
}

/**
 * Loescht die Email mit der Mailnummer, die vom eingeloggten Benutzer eingegeben wird
 * Nach dem Senden wird ueberprueft ob der Server "OK\n" oder "ERR\n" zuruecksendet und gibt eine entsprechende Meldung aus
 */
void del_procedure(int socket){
	char buffer[BUF] = "\0";
	char response[BUF] = "\0";

	int number = 0;

	//Solange loopen bis eine gueltige Zahl eingegeben wurde
	while(1){
		bzero(buffer, sizeof(buffer));
		printf("Enter message-number to delete: ");
		fgets(buffer, sizeof(buffer)-1, stdin);

		number = strtol(buffer, NULL, 10);

		if(number > 0)
			break;

		puts("Message-number has to be > 0");
	}

	sprintf(buffer, "DEL\n%d\n", number);
	send(socket, buffer, strlen(buffer), 0);

	//Antwort empfangen und ausgeben
	recv(socket,response,sizeof(response), 0);

	puts("\n------------------------------");
	if(!strncmp(response, "OK", 2)){
		puts("Message deleted successfully");
		return;
	}
	if(!strncmp(response, "ERR", 3)){
		puts("Error! Message could not be deleted...");
		return;
	}

	printf("\nUncommon answer from server: %s", response);
}

/**
 * Startet die Leseprozedur fuer den eingeloggten Benutzer.
 * Der Benutzer muss eine Mail-Nummer (so wie sie im LIST-Befehl wiedergegeben werden) angeben die gelesen werden sollen.
 * Es werden folgende Informationen zur Mail angezeigt:
 * Sender, Empfaenger, Betreff, Nachricht
 *
 * Falls bei der Email ein Anhang angegeben wurde:
 * Name des Anhangs und Bytegroesse des Anhangs
 *
 * Der Benutzer kann entscheiden ob er den Inhalt des Anhangs speichern will. Die Datei wird bei Bejahung in das Arbeitsverzeichnis
 * des Clientprogramms gespeichert (mit Original-Name). Dateien mit gleichen Namen werden dabei ueberschrieben!
 */
void read_procedure(int socket){
	char buffer[BUF] = "\0";
	Mail *mail = NULL;
	FILE *fp = NULL;
	int number = 0;

	//Solange loopen bis eine gueltige Zahl eingegeben wurde
	while(1){
		bzero(buffer, sizeof(buffer));
		printf("Enter message-number to read: ");
		fgets(buffer, sizeof(buffer)-1, stdin);

		number = strtol(buffer, NULL, 10);

		if(number > 0)
			break;

		puts("Message-number has to be > 0");
	}

	sprintf(buffer, "READ\n%d\n", number);
	send(socket, buffer, strlen(buffer), 0);

	//Nachsehen ob der Server ERR oder SEND zurueckschickt
	bzero(buffer, sizeof(buffer));
	readline(socket, buffer, sizeof(buffer)-1);

	puts("\n------------------------------");
	//Falls SEND, dann wird die Email empfangen und die Informationen ausgegeben
	if(!strncmp(buffer, "SEND", 4)){
		mail = (Mail *)malloc(sizeof(Mail));
		receive_mail(socket, mail);

		printf("\nSender: %s\nReceiver:%s\nTitle: %s", mail->sender, mail->receiver, mail->title);
		puts("\n\nMessage:\n------------------------------");
		puts(mail->message);


		if(mail->a_size > 0){
			puts("\nAttachment:\n------------------------------");
			printf("Filename: %s\nSize: %ld", mail->a_name, mail->a_size);
			printf("\n\n>> Would you like to download this file? y/n (n): ");

			fgets(buffer, sizeof(buffer)-1, stdin);

			puts("\n------------------------------");
			if(buffer[0] == 'y'){
				if((fp = fopen(mail->a_name, "w")) != NULL){
					fwrite(mail->a_content, 1, mail->a_size, fp);
					fclose(fp);
					printf("\nAttachment saved in file './%s'", mail->a_name);
				}
			}
			else{
				puts("Skipped saving the attachment");
			}
		}

		return;
	}

	if(!strncmp(buffer, "ERR", 3)){
		puts("Message does not exist or could not be read...");
		return;
	}

	printf("\nUncommon answer from server: %s", buffer);
}

/**
 * Loggt den momentan eingeloggten Benutzer am Server aus.
 * Bei Erfolg wird 1 zurueckgeliefert, ansonsten 0
 */
int logout_procedure(int socket){
	char message[] = "LOGOUT\n";
	send(socket, message, strlen(message), 0);
	char response[BUF] = "\0";

	readline(socket, response, sizeof(response)-1);

	if(!strncmp(response, "LOGOUT", 6)){
		return 1;
	}
	return 0;
}
