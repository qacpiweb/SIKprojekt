#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/stat.h>

#define MSG_SIZE 80
#define MAX_CLIENTS 95
#define MYPORT 7400
int flaga=0;

void exitClient(int fd, fd_set *readfds, char fd_array[], int *num_clients){
    int i;
    
    close(fd);
    FD_CLR(fd, readfds); //clear the leaving client from the set
    for (i = 0; i < (*num_clients) - 1; i++)
        if (fd_array[i] == fd)
            break;          
    for (; i < (*num_clients) - 1; i++)
        (fd_array[i]) = (fd_array[i + 1]);
    (*num_clients)--;
}

void wyslijPlik(int gn)
{
    char sciezka[512] = "temp.tmp";
    long dl_pliku, wyslano, wyslano_razem, przeczytano;
    struct stat fileinfo;
    FILE* plik;
    unsigned char bufor[1024];
    
    memset(sciezka, 0, 512);
    
    printf("Potomny: klient chce plik %s\n", sciezka);
    
    if (stat(sciezka, &fileinfo) < 0)
    {
        printf("Potomny: nie moge pobrac informacji o pliku\n");
        return;
    }
    
    if (fileinfo.st_size == 0)
    {
        printf("Potomny: rozmiar pliku 0\n");
        return;
    }
    
    printf("Potomny: dlugosc pliku: %d\n", fileinfo.st_size);
    
    dl_pliku = htonl((long) fileinfo.st_size);
    
    if (send(gn, &dl_pliku, sizeof(long), 0) != sizeof(long))
    {
        printf("Potomny: blad przy wysylaniu wielkosci pliku\n");
        return;
    }
    
    dl_pliku = fileinfo.st_size;
    wyslano_razem = 0;
    plik = fopen(sciezka, "rb");
    if (plik == NULL)
    {
        printf("Potomny: blad przy otwarciu pliku\n");
        return;
    }
    
    while (wyslano_razem < dl_pliku)
    {
        przeczytano = fread(bufor, 1, 1024, plik);
        wyslano = send(gn, bufor, przeczytano, 0);
        if (przeczytano != wyslano)
            break;
        wyslano_razem += wyslano;
        printf("Potomny: wyslano %d bajtow\n", wyslano_razem);
    }
    
    if (wyslano_razem == dl_pliku)
        printf("Potomny: plik wyslany poprawnie\n");
    else
        printf("Potomny: blad przy wysylaniu pliku\n");
    fclose(plik);
    return;    
}

void odbierzPlik(int gn)
{
	FILE *fp;
	fp = fopen("temp.tmp", "w+");
    char bufor[1025];
	long dl_pliku, odebrano, odebrano_razem;
	if (recv(gn, &dl_pliku, sizeof(long), 0) != sizeof(long))
    {
        printf("Blad przy odbieraniu dlugosci\n");
        printf("Moze plik nie istnieje?\n");
        return;
    }
    dl_pliku = ntohl(dl_pliku);
    printf("Plik ma dlugosc %d\n", dl_pliku);
    odebrano_razem = 0;
    while (odebrano_razem < dl_pliku)
    {
        memset(bufor, 0, 1025);
        odebrano = recv(gn, bufor, 1024, 0);
        if (odebrano < 0)
            break;
        odebrano_razem += odebrano;
        fputs(bufor, fp);
		fclose(fp);
    }
    if (odebrano_razem != dl_pliku)
        printf("*** BLAD W ODBIORZE PLIKU ***\n");
    else
        printf("*** PLIK ODEBRANY POPRAWNIE ***\n");
	return;
}

int main(int argc, char *argv[]) {
   int i=0;
   int result;
   int port;
   int num_clients = 0;
   int server_sockfd, client_sockfd;
   struct sockaddr_in server_address;
   int addresslen = sizeof(struct sockaddr_in);
   int fd;
   char fd_array[MAX_CLIENTS];
   fd_set readfds, testfds, clientfds;
   char msg[MSG_SIZE + 1];     
   char kb_msg[MSG_SIZE + 10]; 
      

   /*Serwer==================================================*/
   if(argc==1 || argc == 3){
     if(argc==3){
       if(!strcmp("-p",argv[1])){
         sscanf(argv[2],"%i",&port);
       }else{
         printf("Nieprawidlowe wywolanie\n");
         exit(0);
       }
     }else port=MYPORT;
     
     printf("\n*** Uruchamianie serwera (wpisz quit aby zakonczyc): \n");
     fflush(stdout);

     /* Socket dla serwera */
     server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
     server_address.sin_family = AF_INET;
     server_address.sin_addr.s_addr = htonl(INADDR_ANY);
     server_address.sin_port = htons(port);
     bind(server_sockfd, (struct sockaddr *)&server_address, addresslen);

     /* Kolejka polaczen, inicjalizacja zestawu deskryptorow */
     listen(server_sockfd, 1);
     FD_ZERO(&readfds);
     FD_SET(server_sockfd, &readfds);
     FD_SET(0, &readfds);  /* Klawiatura */

     /*  Oczekiwanie na klientow */
     while (1) {
        testfds = readfds;
        select(FD_SETSIZE, &testfds, NULL, NULL, NULL);
                    
        /* Jesli nastepuje aktywnosc sprawdzamy gdzie przy uzyciu FD_ISSET */
        for (fd = 0; fd < FD_SETSIZE; fd++) {
           if (FD_ISSET(fd, &testfds)) {
              
              if (fd == server_sockfd) { /* Zaakceptowanie nowego polaczenia */
                 client_sockfd = accept(server_sockfd, NULL, NULL);
                
                                
                 if (num_clients < MAX_CLIENTS) {
                    FD_SET(client_sockfd, &readfds);
                    fd_array[num_clients]=client_sockfd;
                    /*ID klienta*/
                    printf("Klient %d polaczony\n",num_clients++);
                    fflush(stdout);
                    
                    sprintf(msg,"M%2d",client_sockfd);
                    /*wyslij ID klienta */
                    send(client_sockfd,msg,strlen(msg),0);
                 }
                 else {
                    sprintf(msg, "Zbyt wiele klientow. Sprobuj pozniej.\n");
                    write(client_sockfd, msg, strlen(msg));
                    close(client_sockfd);
                 }
              }
              else if (fd == 0)  {  /* obsluga klawiatury */                 
                 fgets(kb_msg, MSG_SIZE + 1, stdin);
                 if (strcmp(kb_msg, "quit\n")==0) {
                    sprintf(msg, "Serwer sie zamyka\n");
                    for (i = 0; i < num_clients ; i++) {
                       write(fd_array[i], msg, strlen(msg));
                       close(fd_array[i]);
                    }
                    close(server_sockfd);
                    exit(0);
                 }
                 else {
                    sprintf(msg, "M%s", kb_msg);
                    for (i = 0; i < num_clients ; i++)
                       write(fd_array[i], msg, strlen(msg));
                 }
              }
              else if(fd) {  /*obsluga aktywnosci klientow*/
                 //odczyt z otwartego sockeetu
                 result = read(fd, msg, MSG_SIZE);
                 
                 if(result==-1) perror("read()");
                 else if(result>0){
                    /*odczyt id klienta*/
                    sprintf(kb_msg,"M%2d",fd-4);
                    msg[result]='\0';
                    
					if(msg[0]=='p' && msg[1]=='l' && msg[2]=='i' && msg[3] == 'k')
					{
						flaga=1;
					}
					
                    /*sklejenie id i wiadomosci w jeden string*/
					strcat(kb_msg," ");
                    strcat(kb_msg,msg);                                        
                    
                    /*wypisanie do innych klientow*/
					if(flaga==0)
					{
						for(i=0;i<num_clients;i++){
						   if (fd_array[i] != fd)  /*nie piszemy do tego kto wyslal wiadomosc*/
							  write(fd_array[i],kb_msg,strlen(kb_msg));
						}
						/*wypisanie na serwerze*/
						printf("%s",kb_msg+1);
					}
					else if (flaga==1)
					{						
						odbierzPlik(client_sockfd);
						for(i=0;i<num_clients;i++){
						   if (fd_array[i] != fd)  
							  wyslijPlik(fd_array[i]);
						}
						flaga=0;
					}						
                     /*wylaczenie klienta*/
                    if(msg[0] == 'X'){
                       exitClient(fd,&readfds, fd_array,&num_clients);
                    }   
                 }                                   
              }                  
              else { 
                 exitClient(fd,&readfds, fd_array,&num_clients);
              }//if
           }//if
        }//for
     }//while
  }//koniec kodu serwera

}//main