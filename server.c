#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

#define MAX_CLIENTS 100
#define BUFFER_SZ 1107

#define RED_COLOR 	 "\x1b[31m"
#define GREEN_COLOR  "\x1b[32m"
#define YELLOW_COLOR "\x1b[33m"
#define COLOR_RESET  "\x1b[0m"

#define HELP_MESSAGE  "  See all active users: " YELLOW_COLOR "\"status\"" COLOR_RESET "\n  \
Send a message to all users: " YELLOW_COLOR "\"all\"" COLOR_RESET "\n  \
Send a message to a specific user: " YELLOW_COLOR "\"user user_name\"" COLOR_RESET "\n  \
Send a message to a gruop of users: " YELLOW_COLOR "\"group user_name1 user_name2 ...\"" COLOR_RESET "\n"
					  
static _Atomic unsigned int cli_count = 0; //pot fi modificate simultan de doua thread-uri 
static int uid = 10;
int myGlobal = 0;

/* Client structure */
typedef struct{
	struct sockaddr_in address;
	int sockfd;
	int uid;
	char name[32];
} client_t;

client_t *clients[MAX_CLIENTS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;




/* Add new client to queue */
void queue_add(client_t *cl){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i < MAX_CLIENTS; ++i){
		if(!clients[i]){
			clients[i] = cl;
			break;
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

/* Remove client to queue */
void queue_remove(int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i < MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid == uid){
				clients[i] = NULL;
				break;
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

/* Send message to all clients except sender */
void send_message(char *s, int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i<MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid != uid){
				if(write(clients[i]->sockfd, s, strlen(s)) < 0){
					perror("ERROR: write to descriptor failed");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

/* Send message to a specific user */
void send_private_message(char *s, char *name){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i<MAX_CLIENTS; ++i){
		if(clients[i]){
			if(strcmp(clients[i]->name,name)==0){
				if(write(clients[i]->sockfd, s, strlen(s)) < 0){
					perror("ERROR: write to descriptor failed");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

void send_error_client_message(char *s, int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i<MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid == uid){
				if(write(clients[i]->sockfd, s, strlen(s)) < 0){
					perror("ERROR: write to descriptor failed");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

/* Show all active users */
char active_users[256]="";
char *get_active_users(int uid){
	pthread_mutex_lock(&clients_mutex);

    strcpy(active_users,"");
	for(int i=0; i<MAX_CLIENTS; ++i){
		if(clients[i])
        {
            if(clients[i]->uid != uid)
            {
                strcat(active_users,clients[i]->name);
                strcat(active_users,"\n");
            }
		}
	}

    strcat(active_users,"\0");

	pthread_mutex_unlock(&clients_mutex);
    return active_users;
}

//color message
char color_message[BUFFER_SZ+16]="";
char *get_color_message(char *s, char *color){
	strcpy(color_message,color);
	strcat(color_message,s);
	strcat(color_message,COLOR_RESET"\n");
	return color_message;
}

int check_user_already_exists(char *user_name){
    pthread_mutex_lock(&clients_mutex);
    for(int i=0; i<MAX_CLIENTS; ++i){
        if(clients[i])
        {
            if(strcmp(clients[i]->name,user_name)==0)
            {
                pthread_mutex_unlock(&clients_mutex);
                return 1;
            }

        }

    }
    pthread_mutex_unlock(&clients_mutex);
    return 0;
}



/* Handle all communication with the client */
void *handle_client(void *arg){
	char buff_out[BUFFER_SZ];	
	char name[32];
	int leave_flag=0;

	cli_count++;
	client_t *cli = (client_t *)arg;

	// Name
	if(recv(cli->sockfd, name, 32, 0)<=0)
	{
		printf("Error\n");
		leave_flag = 1;
	} 
	else
	if(check_user_already_exists(name))
	{
		leave_flag=2;
	}
		else{
		strcpy(cli->name, name);
		sprintf(buff_out, "%s has joined", cli->name);
		strcpy(buff_out,get_color_message(buff_out,GREEN_COLOR));
		printf("%s", buff_out);
		send_message(buff_out, cli->uid);
	}
	bzero(buff_out, BUFFER_SZ); 
	int ok=1;
	if(leave_flag==2)
	{
		ok=0;
		send_error_client_message("Name_Already_Exists\n", cli->uid);
	}

	int k=0;


	while(1){
		if (leave_flag && leave_flag!=2) {
			break;
		}
	
		
		while(k<2 && ok==0) {
			if(recv(cli->sockfd, name, 32, 0))
			{
				k++;
				if(check_user_already_exists(name))
				{
				
				
					send_error_client_message("Name_Already_Exists\n", cli->uid);
				}
				else
				{

					strcpy(cli->name, name);
					sprintf(buff_out, "%s has joined", cli->name);
					strcpy(buff_out,get_color_message(buff_out,GREEN_COLOR));
					printf("%s", buff_out);
					send_message(buff_out, cli->uid);
					ok=1;
					k=4;
					leave_flag=0;
				
				}
			}
		}

		if(ok==0) break;
		bzero(buff_out, BUFFER_SZ); 
		int receive = recv(cli->sockfd, buff_out, BUFFER_SZ, 0);

		if (receive > 0){
			if(strlen(buff_out) > 0){
			
                char command[BUFFER_SZ];
                char *message;
                char *user;

				//ex buff_out: user ana \n ion: Hi ana 

                message = strtok (buff_out,"\n");	// message=user ana
                strcpy(command,message);			// command=user ana
                message = strtok (NULL,"");			// message=ion: Hi ana 

                printf ("Command:%s\n",command);
                printf ("Message:%s\n",message);
                
				if(strcmp(command,"help")==0)
					{
						send_private_message(HELP_MESSAGE, cli->name);
					}

				else
                if(strcmp(command,"all")==0)
				    send_message(message, cli->uid);

                else
                    if(strcmp(command,"status")==0)
                    {
                        char users_online[256];
                        strcpy(users_online,get_active_users(cli->uid));
                        send_private_message(users_online,cli->name);
                    }

                else
                    if(strstr(command,"user")>0)
                    {
                        user = strtok (command," ");	//user= user
						user = strtok (NULL," ");		//user= ana
                        if(user==NULL)
						{
							char err_msg[40];
							strcpy(err_msg,"Please enter a name\n");
							strcpy(err_msg,get_color_message(err_msg,RED_COLOR));
							send_private_message(err_msg,cli->name);
						}
						else
						{
							
						if(check_user_already_exists(user)==1)
                        	send_private_message(message,user);
						else
							{
								char err_msg[40];
								strcpy(err_msg,user);
								strcat(err_msg," doesn't exist\n");
								strcpy(err_msg,get_color_message(err_msg,RED_COLOR));
								send_private_message(err_msg,cli->name);
								
							}
						}
                    }

                else
                    if(strstr(command,"group")>0)
                    {

                        user = strtok (command," ");
						user = strtok (NULL," ");	

						if(user==NULL)
						{
							char err_msg[40];
							strcpy(err_msg,"Please enter a name\n");
							strcpy(err_msg,get_color_message(err_msg,RED_COLOR));
							send_private_message(err_msg,cli->name);
						}

						

                        while(user!=NULL){
							if(check_user_already_exists(user)==0)
                        	{
								char err_msg[40];
								strcpy(err_msg,user);
								strcat(err_msg," doesn't exist\n");
								strcpy(err_msg,get_color_message(err_msg,RED_COLOR));
								send_private_message(err_msg,cli->name);
							}
							else{
								send_private_message(message,user);
							}
							user=strtok(NULL, " ");
                        }
                    }
				else
					{
						char command_error[40];
						sprintf(command_error, "Command does't exist");
						strcpy(command_error,get_color_message(command_error,RED_COLOR));
						send_private_message(command_error,cli->name);
					}
			}

		} else if (receive == 0){
			sprintf(buff_out, "%s has left", cli->name);
			strcpy(buff_out, get_color_message(buff_out,RED_COLOR));
			printf("%s", buff_out);
			send_message(buff_out, cli->uid);
			leave_flag = 1;
		} else {
			printf("ERROR: -1\n");
			leave_flag = 1;
		}

		bzero(buff_out, BUFFER_SZ);
	}

  	/* Delete client from queue and yield thread */
	close(cli->sockfd);
  	queue_remove(cli->uid);
 	free(cli);
  	cli_count--;
  	pthread_detach(pthread_self());

	return NULL;
}

int main(int argc, char **argv){
	if(argc != 2){
		printf("Usage: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char *ip = "127.0.0.1";
	int port = atoi(argv[1]);
	int option = 1;
	int listenfd = 0, connfd = 0;
  	struct sockaddr_in serv_addr;
  	struct sockaddr_in cli_addr;
  	pthread_t tid;

	/* Socket settings */
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(ip);
	serv_addr.sin_port = htons(port);

  	/* Ignore pipe signals */
	signal(SIGPIPE, SIG_IGN);

	if(setsockopt(listenfd, SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR),(char*)&option,sizeof(option)) < 0){
		perror("ERROR: setsockopt failed");
    return EXIT_FAILURE;
	}

	/* Bind */
  	if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("ERROR: Socket binding failed");
    return EXIT_FAILURE;
  	}

  	/* Listen */
  	if (listen(listenfd, 10) < 0) {
    perror("ERROR: Socket listening failed");
    return EXIT_FAILURE;
	}

	printf("=== WELCOME TO THE CHATROOM ===\n");

	while(1){
		socklen_t clilen = sizeof(cli_addr);
		connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

		

		/* Client settings */
		client_t *cli = (client_t *)malloc(sizeof(client_t));
		cli->address = cli_addr;
		cli->sockfd = connfd;
		cli->uid = uid++;

		/* Add client to the queue and fork thread */
		queue_add(cli);
		pthread_create(&tid, NULL, &handle_client, (void*)cli);

		/* Reduce CPU usage */
		sleep(1);
	}

	return EXIT_SUCCESS;
}
