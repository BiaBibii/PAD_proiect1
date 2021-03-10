#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>


#define LENGTH_MSG 1024
#define OUR_LENGTH_MSG 20
#define LENGTH_NAME 32
#define LENGTH_CMD 50

//colors
#define RED_COLOR 	 "\x1b[31m"
#define YELLOW_COLOR "\x1b[33m"
#define COLOR_RESET  "\x1b[0m"
#define CYAN_COLOR   "\x1b[36m"


volatile sig_atomic_t flag = 0; // volatile=se poate schimba in afara codului

int esec=0;
int sockfd = 0;	//a file descriptor
char name[32];
char names_server[276];
pthread_mutex_t mesajErr = PTHREAD_MUTEX_INITIALIZER;

void str_trim_lf (char* arr, int length) { 
  int i;
  for (i = 0; i < length; i++) { // trim \n
    if (arr[i] == '\n') {
      arr[i] = '\0';
      break;
    }
  }
}

void catch_ctrl_c(int sig) {	//punem flag pe 1 si iesim din program
    flag = 1;
}

//**********************//
//     SEND MESSAGE     //
//**********************//
void send_msg_handler() {
	char command[LENGTH_CMD] = {};  							//comanda utilizatorului
	char message[LENGTH_MSG] = {};								//mesajul utilizatorului
	char our_message[OUR_LENGTH_MSG+15+20] = {};
	char buffer[OUR_LENGTH_MSG+15+20 + LENGTH_NAME + LENGTH_CMD +1] = {};	//comanda+mesaj ce vor fi trimise la sever

  	while(1){
		fgets(command, LENGTH_CMD, stdin);

		//introducem use
		if(strcmp(command, "use\n") == 0)
		{ 
    		printf(CYAN_COLOR ">" COLOR_RESET " Command:");
    		fgets(command, LENGTH_CMD, stdin);

			//status si help
			if(strcmp(command, "status\n") == 0 || strcmp(command, "help\n") == 0 )
			{
				sprintf(buffer, "%s\n",command);			//punem comanda in buffer
      			send(sockfd, buffer, strlen(buffer), 0);	//trimitem comanda la server
			}
			else

				//nu exista comanda
				if(strcmp(command, "all\n")!=0 && strstr(command, "user")==NULL && strstr(command, "group")==NULL)
				{
					sprintf(buffer, "%s\n",command);			//punem comanda in buffer
      				send(sockfd, buffer, strlen(buffer), 0);
				}

				//all, user sau group
				else
				{
					printf(CYAN_COLOR ">" COLOR_RESET " Message:");

					strcpy(message,"");
					fgets(message, LENGTH_MSG, stdin);
					if(strlen(message)>OUR_LENGTH_MSG)
					{
						printf(RED_COLOR"Message too long, won't be send complete\n"COLOR_RESET);
						strncpy(our_message,message,OUR_LENGTH_MSG);
						strcat(our_message,RED_COLOR" (message incomplete)"COLOR_RESET);
					}
					else
					strcpy(our_message,message);
			
					
					//trimitem mesajul la server
					sprintf(buffer,"%s%s: %s\n",command, name, our_message);	//punem comanda \n name: mesaj in buffer
					send(sockfd, buffer, strlen(buffer), 0);					//trimitem mesajul catre server
				}

		}
	}


}

//**********************//
//   RECIEVE MESSAGE    //
//**********************//
void recv_msg_handler() {
	char message[LENGTH_MSG] = {};

  	while(1) 
	{
		int receive = recv(sockfd, message, LENGTH_MSG, 0);	//apel ce primeste mesajul de la server
    	if (receive > 0) 
		{
			if((strstr(message,"Name_Already_Exists"))  )
			{
		    	strcpy(names_server,message);
                esec++;
		  	}
			else	
				printf("%s", message);
		}
     	else 
			if (receive == 0) 
				break;
  		
		memset(message, 0, sizeof(message));	
   	}
	
   
}

//**********************//
//         MAIN         //
//**********************//
int main(int argc, char **argv){
	if(argc != 2)
	{
		printf("Usage: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}


	char *ip = "127.0.0.1";
	int port = atoi(argv[1]);

	signal(SIGINT, catch_ctrl_c);	//SIGINT- intrrupe un proces cand punem CTRL+C

	/* Enter the name */
	printf("Please enter your name: ");
  	fgets(name, 32, stdin);
	str_trim_lf(name, strlen(name)); 

	/* Socket settings */
	struct sockaddr_in server_addr;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
  	server_addr.sin_family = AF_INET;
  	server_addr.sin_addr.s_addr = inet_addr(ip);
  	server_addr.sin_port = htons(port);


	// Connect to Server
  	int err = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
  	if (err == -1) 
	{
		printf("ERROR: connect\n");
		return EXIT_FAILURE;
	}



	// Send name
	send(sockfd, name, 32, 0);
   

    /* thread pentru primirea mesajului */
    pthread_t recv_msg_thread;
  	if(pthread_create(&recv_msg_thread, NULL, (void *) recv_msg_handler, NULL) != 0)
	{
		printf("ERROR: pthread\n");
		return EXIT_FAILURE;
	}

	
	
	/* 2 incercari pt nume ce exista */
	sleep(1);	
    int k=0;
	if(esec==1)
		while(strstr(names_server,"Name_Already_Exists"))
		{
			strcpy(names_server,"");
            k++;
            if(k==3) return EXIT_FAILURE;
			printf("Name already exists, try again: ");
  	        fgets(name, 32, stdin);
	        str_trim_lf(name, strlen(name));
            send(sockfd, name, 32, 0);
			sleep(1);
		}
	

	printf("\n			!!! WELCOME TO THE CHATROOM !!!\n");
	printf("=== Write " YELLOW_COLOR "\"use\"" COLOR_RESET " to use our commands, please type " YELLOW_COLOR  "\"help\"" COLOR_RESET  " to see the commands ===\n");
	

	/* thread pentru trimiterea mesajului */
	pthread_t send_msg_thread;
  	if(pthread_create(&send_msg_thread, NULL, (void *) send_msg_handler, NULL) != 0){
		printf("ERROR: pthread\n");
  		return EXIT_FAILURE;
	}


	while (1)
		if(flag){
			printf("\nBye\n");
			break;
		}

	close(sockfd);
	return EXIT_SUCCESS;
}
