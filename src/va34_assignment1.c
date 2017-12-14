/**
 * @va34_assignment1
 * @author  Venkata Krishnan Anantha Raman <va34@buffalo.edu>
 * @version 1.0
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @section DESCRIPTION
 *
 * This contains the main function. Add further description here....
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <sys/ioctl.h>
#include <errno.h> 
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>


#include "../include/global.h"
#include "../include/logger.h"

#define CLIENT 1
#define SERVER 2
#define FILE_TRANSFER_PORT 7676
#define VENKAT 1
/**
 * main function
 *
 * @param  argc Number of arguments
 * @param  argv The argument list
 * @return 0 EXIT_SUCCESS
 */

/*Function declartions */
void init_client (int port_number);
void init_server (int port_number);
int process_common_commands(char *token);

static int port;
static int exit_required;
struct client_str
{
	char host_name[35];
	char host_ip[16];
	int listen_port_num;
	int socket;
	int num_msg_sent;
	int num_msg_rcvd;
	int status;
	int in_use;
	int msg_sent;
	int msg_rcvd;
};
int cft_fd = 0;
int ordered_list[4];
static int active_users;
#define MAX_CLIENTS 4
struct client_str client_list[MAX_CLIENTS];
fd_set readfds;	
static int max_fd;
char ip_addr[16];
char client_list_buffer[1024];
int is_client_server = 0;
int update_only = 0;
int is_login_success =0;
static int client_tcp_fd= 0;
int list_requested =0;
int ft_fd = 0;
int main(int argc, char **argv)
{
	/*Init. Logger*/
	cse4589_init_log(argv[2]);

	/*Clear LOGFILE*/
	fclose(fopen(LOGFILE, "w"));

	/*Start Here*/
	if (argc == 3)
	{
		int port_number = atoi(argv[2]);	
		port = port_number;
		if (0 == strcmp(argv[1],"c"))
		{
			is_client_server = CLIENT;
			init_client (port_number);
		//	printf (" is a client port: %d \r\n",port_number);
		}
		else if (0 == strcmp(argv[1],"s"))
		{
			is_client_server = SERVER;
			init_server (port_number);
	//		printf (" is a server port:%d  \r\n",port_number);
		}
	}
	return 0;
}

void
init_server(int port_number)
{
	memset(ip_addr,0,16);
	get_ip_addr(ip_addr);

	int server_socket =0 ,client_socket,client_sockets[4],new_socket;
	char buffer[1024];
	
	FD_ZERO(&readfds);  

	if ((server_socket = socket (AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf ("socket call failed for server socket <server scoket :%d> \r\n",server_socket);
		exit(1);
	}	

	struct sockaddr_in sin;
	memset(&sin,0,sizeof(sin));
	int addrlen = sizeof(sin);

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(port_number);

	max_fd = server_socket + 1;
	if ((bind(server_socket, (struct sockaddr *) &sin, sizeof(sin)))<0)
	{
		printf ("socket call failed \r\n");
		exit(1);
	}
	
	listen(server_socket,5);

	int slot ;	
	while (1)
	{
		FD_ZERO(&readfds);
		FD_SET (0,&readfds);
		FD_SET(server_socket, &readfds);
		
		for (int i = 0; i<4; i++)
		{
			if (client_list[i].socket == 0)
				continue;
		
			FD_SET(client_list[i].socket, &readfds);

			if (max_fd <= client_list[i].socket)
			{
				max_fd = client_list[i].socket;
			} 
		}

//		printf ("Waiting on select \r\n");	
		select (max_fd, &readfds, NULL, NULL, NULL);

		if (FD_ISSET(server_socket, &readfds))
		{
			if ((client_socket = accept(server_socket, 
							(struct sockaddr *)&sin, (socklen_t*)&addrlen))<0)  
			{  
				perror("accept");  
				exit(EXIT_FAILURE);  
			}	 	 
			
			FD_SET(client_socket, &readfds);
			if (client_socket >= max_fd)
			{
				max_fd = client_socket+1;
			}
			char host[35];
			char service[1024];
			getnameinfo(&sin, sizeof sin, host, sizeof host, service, sizeof service, 0);
	//		printf("New connection , socket fd is %d , ip is : %s , port : %d host:%s port %d\n"					, client_socket , inet_ntoa(sin.sin_addr) , ntohs (sin.sin_port),host,sin.sin_port);  

			slot = find_next_slot(inet_ntoa(sin.sin_addr));
			int j = 0;	

		//	printf ("Get Slot number as %d \r\n",slot);
			if (slot >= 0 )
			{
				if(0 == client_list[slot].in_use)
				{
					if(client_socket >= max_fd)
						max_fd = client_socket + 1;
					memset(client_list[slot].host_name,0,35);
					memset(client_list[slot].host_ip,0,16);	
					strcpy(client_list[slot].host_name,host);
					strcpy(client_list[slot].host_ip, inet_ntoa(sin.sin_addr));
					client_list[slot].listen_port_num = ntohs(sin.sin_port);
					client_list[slot].socket = client_socket;
					client_list[slot].in_use = 1;
					sort_list();
					send_client_list(client_socket);
				//	send_update_to_all_clients();
				//	printf ("updated client list %d \r\n",slot);
				}
				else
				{
					printf ("something fishy\r\n");
				}
			}

			//send_formated_list(client_socket);
		}
		else if (FD_ISSET(0, &readfds))
		{
	//		printf (" Process Input \r\n");
			HandleServerInput();
		}
		else
		{
			for (int i=0; i<MAX_CLIENTS; i++)
			{
				if(client_list[i].socket == 0)
					continue;

				if (FD_ISSET(client_list[i].socket, &readfds))
				{
					process_message_from_client(client_list[i].socket);
				}
			}
		}					
	}
}

void
HandleServerInput ()
{
//	printf ("Handling server input \r\n");
	char arr[100];
	memset(arr,0,100);
	char *token= NULL;
	char *token_arr[5];
	int i = 0;
	
	gets(arr);

   	token = strtok(arr, " ");
   	while( token != NULL ) 
	{
		token_arr[i] = token;
		i++;
   		token = strtok(NULL," ");
  	}	
	
	int retval = process_common_commands (token_arr[0]);
	if (retval == 1)
	{
		if (strncmp(token_arr[0],"STATISTICS",10)==0)
		{
			cse4589_print_and_log("[STATISTICS:SUCCESS]\n",token);
			int count =1;
			for (int j=0;j<MAX_CLIENTS; j++)
			{
				char status[10];
				memset (status,0,10);
		
				if (!client_list[j].in_use)
					continue;
					
				if (client_list[i].status == 2)
				strcpy(status,"logged-out");
				else
				strcpy(status,"logged-in");
				cse4589_print_and_log ("%-5d%-35s%-8d%-8d%-8s\n", count, client_list[j].host_name, client_list[j].msg_sent, client_list[j].msg_rcvd, status);
				count++;
			}
			cse4589_print_and_log("[STATISTICS:END]\n",token);
		}
	}
	return;
}
int
process_common_commands(char *token)
{
	char your_ubit_name[4];

	memset(your_ubit_name,0,4);	
	strcpy(your_ubit_name,"va34");

	if (strcmp(token,"AUTHOR")==0)
	{
		cse4589_print_and_log("[%s:SUCCESS]\n",token);
		cse4589_print_and_log ("I, %s, have read and understood the course academic integrity policy.\n",your_ubit_name);
		cse4589_print_and_log("[%s:END]\n",token);
		return 0;
	}
	else if(strncmp(token,"IP",2)==0)
	{
		cse4589_print_and_log("[%s:SUCCESS]\n",token);
		cse4589_print_and_log("IP:%s\n",ip_addr);
		cse4589_print_and_log("[%s:END]\n",token);
		return 0;
	}
	else if(strcmp(token,"PORT")==0)
	{
		cse4589_print_and_log("[%s:SUCCESS]\n",token);
		cse4589_print_and_log("PORT:%d\n",port);
		cse4589_print_and_log("[%s:END]\n",token);
		return 0;
	}
	else if(strcmp(token,"LIST")==0)
	{
		if (is_client_server == SERVER)
		{
			cse4589_print_and_log("[%s:SUCCESS]\n",token);
			print_client_list();
			cse4589_print_and_log("[%s:END]\n",token);
			return 0;			
		}
		else
		{
			if (is_login_success == 0)
			{
				return 0;
			}
			cse4589_print_and_log("[%s:SUCCESS]\n",token);
			print_client_list ();
			cse4589_print_and_log("[%s:END]\n",token);
			//list_requested = 1;
			//printf ("Processing :LIST Command \r\n");
			//request_message(client_tcp_fd,token);
			return 0;
		}
	}
	else if(strcmp(token,"EXIT")==0)
	{
		if (is_login_success == 1)
		{
			request_message(client_tcp_fd,token);
		}
		exit_required = 1;
		cse4589_print_and_log("[%s:SUCCESS]\n",token);
		cse4589_print_and_log("[%s:END]\n",token);
		return 0;
	}
	else if(strcmp(token,"list")==0)
	{
		print_client_list();
	}
	return 1;
}

void process_broadcast (int socket, char *buf)
{
//	printf ("process_broadcast:%s\r\n",buf);
	char tbuff[1024];
	memset(tbuff,0,1024);
	char temp_buffer[1024];
	memset(temp_buffer,0,1024);
	strcpy(tbuff,buf);

	int i = 0;	
	char *token;
	char *token_arr[100]; 

 	token = strtok(tbuff, " ");
   	while( token != NULL ) 
	{
		token_arr[i] = token;
		i++;
   		token = strtok(NULL," ");
  	}



	int from_id = find_client_str(token_arr[1]);
	client_list[from_id].msg_sent++;

	sprintf(temp_buffer,"%-10s %15s %s\r\n","BRAD",client_list[from_id].host_ip,buf+26);
	for (int i=0; i<MAX_CLIENTS; i++)
	{
		if (!client_list[i].in_use)
			continue;

		if (i == from_id)
			continue;
		
		client_list[i].msg_rcvd++;
		int buffer_len = strlen(temp_buffer);
	//	printf ("sending on socket :%d :%s: \r\n",client_list[i].socket,temp_buffer);
		sendall(client_list[i].socket, temp_buffer,&buffer_len);
	}	
	
	cse4589_print_and_log("[RELAYED:SUCCESS]\n");
	cse4589_print_and_log("msg from:%s, to:255.255.255.255\n[msg]:%s\n",client_list[from_id].host_ip,buf+27);
	cse4589_print_and_log("[RELAYED:END]\n");
	
#if 0	
	char  header[10];

	
	printf ("<id:%d> <%s>\r\n",from_id,token_arr[1]);	
	for (int i=0; i<MAX_CLIENTS; i++)
	{
		if (!client_list[i].in_use)
			continue;

		if (i == from_id)
			continue;

		int buffer_len = strlen(buf);
		memcpy(buf,blank,10);
		memcpy(buf,hello,4);
		printf ("sending on socket :%d :%s: \r\n",client_list[i].socket,buf);
		sendall(client_list[i].socket, buf,&buffer_len);
	}	
#endif		
	return;
}	

void process_send (int socket, char *buf)
{
	char  ip[16];
	memset (ip,0,16);
	
	char *temp_buffer[1024];
	memset(temp_buffer,0,1024);

	char *token;
	char *token_arr[5];
	int i = 0;	

	strcpy(temp_buffer,buf);
  
 	token = strtok(temp_buffer, " ");
   	while( token != NULL ) 
	{
		if (i==2)
		{
			break;
		}
		token_arr[i] = token;
		i++;
   		token = strtok(NULL," ");
  	}
	
	int dest_index = find_client_str(token_arr[1]);
	client_list[dest_index].msg_sent++;

//	printf ("Destination is :%s: Dest index :%d \r\n",token_arr[1],dest_index);

	int source_index = client_find_socket (socket);
	client_list[source_index].msg_rcvd++;
//	printf ("Source Index :%d\r\n",source_index);
 	memset(temp_buffer,0,1024);
        sprintf(temp_buffer,"%-10s %15s %s\r\n","SEND",client_list[source_index].host_ip,buf+26);
	
//	printf ("going to send :%s\r\n",temp_buffer);
	int buffer_len = strlen(temp_buffer);

	cse4589_print_and_log("[RELAYED:SUCCESS]\n");
	cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n",client_list[source_index].host_ip, client_list[dest_index].host_ip,buf+26);
	cse4589_print_and_log("[RELAYED:END]\n");
	sendall(client_list[dest_index].socket, temp_buffer,&buffer_len);
	return;
}	
void process_message_from_client (int client_fd)
{
	char buf[1024];
	memset(buf,0,1024);

	recv(client_fd, buf, sizeof buf, 0);
//	printf ("recieved %s in %d \r\n",buf,client_fd);
	if (0 == strcmp(buf,"LIST"))
	{
	//	print_client_list ();
		send_formated_list(client_fd);
	}
	else if (0 == strcmp(buf,"REFRESH"))
	{
		update_only = 1;
		send_client_list(client_fd);		
		update_only = 0;
		send_formated_list(client_fd);
	}
	else if (0 == strcmp(buf,"EXIT"))
	{
		delete_client_data(client_fd);		
	}
	else if (0 == strncmp(buf,"SEND",4))
	{
		process_send(client_fd,buf);
	}
	else if (0 == strncmp(buf,"BROADCAST",9))
	{
		process_broadcast(client_fd,buf);
	}
	else if (0 == strncmp(buf,"PORT",4))
	{
		char *tok;
		char *tok_arr[10];
		int tcount = 0;
	   	tok = strtok(buf, " ");
   		while( tok != NULL ) 
		{
			while(tcount>2)
			break;

			tok_arr[tcount] = tok;
			tcount++;
   			tok= strtok(NULL," ");
		}
	//	printf ("<%s>\r\n",buf);	
		int idx = client_find_socket (client_fd);
		client_list[idx].listen_port_num = atoi(tok_arr[1]);
		sort_list();	
 		update_only = 1;
		send_client_list(client_fd);		
		update_only = 0;
	
 	}
	else if (0 == strncmp(buf,"LOGOUT",6))
	{
		int idx = client_find_socket (client_fd);
///		printf ("logging for client_fd %d id %d \r\n",client_fd,idx);
		client_list[idx].status = 2;
	}
	else
	{
//		printf ("cant do anything\r\n");
	}
	return;
}

void send_delete_client (int client_socket, int i)
{
//	printf ("send_delete_client socket %d id %d\r\n",client_socket,i);
	char buffer[1024];
	int offset = 0;	

	memset (buffer,0,1024);

	sprintf(buffer,"LIST-DELE ");
	offset = strlen(buffer);
	sprintf(buffer+offset,"%d",i);

	int buffer_len = strlen(buffer);
	sendall(client_socket, buffer, &buffer_len);
	return;		
}
void delete_client_data (int client_socket)
{
//	printf ("Delete the client data %d \r\n",client_socket);
	for (int i=0; i<MAX_CLIENTS; i++)
	{
		if (!client_list[i].in_use)
		{
//			printf ("i - %d not in use continuting \r\n",i);
			continue;
		}

		if (client_list[i].socket == client_socket)
		{
			FD_CLR(client_socket, &readfds);		
			if (client_socket+1 ==  max_fd)
				max_fd = max_fd - 1;
			memset(client_list[i].host_name,0,35);
			memset(client_list[i].host_ip,0,16);	
			client_list[i].listen_port_num = 0;
			client_list[i].in_use = 0;
			for (int j =0; j<MAX_CLIENTS; j++)
			{
				if (client_list[j].in_use == 1)
				{
				//	send_delete_client(client_list[j].socket,i);
				}
			}
			client_list[i].socket = 0;
			close(client_socket);
			return;
		}
	}	
}

void
send_update_to_all_clients ()
{	
	if (is_client_server != SERVER)
		return;

	for (int i =0; i<MAX_CLIENTS; i++)
	{
		if (client_list[i].in_use == 1)
		{
			update_only = 1;
			send_client_list(client_list[i].socket);			
			update_only = 0;
		}
	}	
	return;
}
void print_client_list ()
{
	char buffer[1024];
	int inc_buff = 0;
	int count =1;

	for (int i =0 ;i <MAX_CLIENTS; i++)
	{
		if (client_list[i].in_use)
		{
			sprintf(buffer+inc_buff,"%-5d%-35s%-20s%-8d\n", count,client_list[i].host_name, client_list[i].host_ip, client_list[i].listen_port_num);
			inc_buff = strlen(buffer);
			count++;
		}
	}
		
	cse4589_print_and_log("%s",buffer);
	return;
}

void send_formated_list (int client_socket)
{
//	printf (" send_formated_list \r\n");
	char buffer[1024];
	int count =0;
	memset (buffer,0,1024);
	
	sprintf(buffer,"PRINT-OUT ");
	int inc_buff = strlen(buffer);
	for (int i =0 ; i<MAX_CLIENTS; i++)
	{
		if (client_list[i].listen_port_num == 0)
			continue;

		if (client_list[i].in_use)
		{
			sprintf(buffer+inc_buff,"%-5d%-35s%-20s%-8d\n", count,client_list[i].host_name, client_list[i].host_ip, client_list[i].listen_port_num);
			inc_buff = strlen(buffer);
			count++;
		}
	}

	int buffer_len = strlen(buffer);
	sendall(client_socket, buffer, &buffer_len);
	return;
}

void send_client_list (int client_socket)
{
	if (is_client_server == CLIENT)
	{
	//	printf ("hello world\r\n");
	}
	int count = 1;
	char buffer[1024];
	memset (buffer,0,1024);
	int inc_buff = 0;
	int buffer_len = strlen(buffer);

	if (client_socket > 0)
	{
		for (int i =0; i<MAX_CLIENTS; i++)
		{
			if (client_list[i].in_use == 1)
			{
				sprintf(buffer,"LIST-PART ");
				inc_buff = strlen(buffer);
				sprintf(buffer+inc_buff,"%d %s %s %d %d %d %d\n", i,client_list[i].host_name, client_list[i].host_ip, client_list[i].listen_port_num,client_list[i].status,client_list[i].in_use);
				int buffer_len = strlen(buffer);
		//		printf ("<%s:>\r\n",buffer);
				sendall(client_socket, buffer, &buffer_len);
//				printf ("Successfully sent\r\n");
			}
		}
	}

	if (update_only == 1)
		return;	
	return;	
}
int find_client_str (char *client_ip)
{
	for(int i=0; i<MAX_CLIENTS; i++)
	{
		if(!client_list[i].in_use)
		{
//			printf (" Not in use : %d \r\n",i);	
			continue;
		}
		
		if(0 == strcmp(client_ip,client_list[i].host_ip))
		{
//			printf ("match found at :%d <%s><%s> \r\n",i,client_ip,client_list[i].host_ip);
			return i;
		}
	}

	return MAX_CLIENTS;	
}

int client_find_socket (int socket)
{
	for (int i=0;i<MAX_CLIENTS;i++)
	{
		if (!client_list[i].in_use)
			continue;

		if (client_list[i].socket == socket)
			return i;
	}
	return MAX_CLIENTS;	
}
void sort_list()
{
	struct client_str temp_str;

        for (int i = 1; i < MAX_CLIENTS; i++)
        for (int j = 0; j < MAX_CLIENTS - i; j++)
        {
		if (client_list[j].listen_port_num >client_list[j + 1].listen_port_num)
                {
                	temp_str = client_list[j];
                        client_list[j] = client_list[j + 1];
                        client_list[j + 1] = temp_str;
                }
        }
}

int find_next_slot (char *client_ip)
{	int i = 0;
	if (MAX_CLIENTS == find_client_str (client_ip))
	{
		for (i =0; i<MAX_CLIENTS; i++)
		{
			if(!client_list[i].in_use)
			{
				return i;
			}
		}
		
		if (i== MAX_CLIENTS)
		printf ("very wrong handle later \r\n");
	}
	return -1;
}
#if 0
int
process_connection_request (struct sockaddr *sin, int server_socket)
{
       char remoteIP[INET6_ADDRSTRLEN];

	int sin_len = sizeof(struct sockaddr); 
	int client_socket = 0;
	if ((client_socket = accept(server_socket, sin, &sin_len))<0)  
	{  
        	perror("accept");  
                exit(EXIT_FAILURE);  
        }
	
	int slot = find_next_slot;
	/* New connection */
	return client_socket;

}
#endif
void
init_client(int port_number)
{
	memset(ip_addr,0,16);
	get_ip_addr(ip_addr);
 	struct sockaddr_in server_addr;

	fd_set read_fs ,write_fs;

#if 0
	ft_fd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in sin;
	memset(&sin,0,sizeof(sin));
	int addrlen = sizeof(sin);

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(FILE_TRANSFER_PORT);
	if ((bind(ft_fd, (struct sockaddr *) &sin, sizeof(sin)))<0)
	{
		printf ("socket call failed \r\n");
		exit(1);
	}
//	printf("bind successfull \r\n");	
	listen(ft_fd,2);
//	printf ("listensuccessfull\r\n");

#endif	
	while (1)
	{
		if (exit_required == 1)
			break;

		int action_handle = wait_on_select(&read_fs);
		if (action_handle == 0)
		{
			HandleClientInput ();
		}
		else if(action_handle == client_tcp_fd)
		{
			ProcessPacket (client_tcp_fd);
		}
#if 0
		else if(action_handle == ft_fd)
		{
			if ((cft_fd = accept(ft_fd, 
							(struct sockaddr *)&sin, (socklen_t*)&addrlen))<0)  
			{  
				perror("accept");  
				exit(EXIT_FAILURE);  
			}	 	 
			printf ("accepted \r\n");		
		}	
#endif 
	}
	return;
}
void request_message (int fd, char *arr)
{
	int buffer_len = strlen(arr);
	char buffer[10];
	memset(buffer,0,10);
	strcpy(buffer,arr);
	sendall(fd,buffer,&buffer_len);
	return;	
}
void
HandleClientInput ()
{
 	struct sockaddr_in server_addr;
	char arr[300];
	char t_arr[300];
	int start_of_message = 0;
	memset(arr,0,300);
	memset(t_arr,0,300);


	char *token= NULL;
	char *token_arr[5];
	int i = 0;
	
	gets(arr);
	int length = strlen(arr);

	arr[length]='\0';

	strcpy(t_arr,arr);
	
	if(0==strlen(arr))
		return;

   	token = strtok(arr, " ");
   	while( token != NULL ) 
	{
		while(i>2)
		break;

		token_arr[i] = token;
		i++;
   		token = strtok(NULL," ");
  	}

	if ((token_arr[2]!= NULL) && (token_arr[1] != NULL))
	{
		start_of_message = token_arr[2] - token_arr[0];
	}

	if (strcmp(token_arr[0],"LOGIN")==0)
	{
		if (0 != process_user_login(token_arr[0],token_arr[1],token_arr[2],&server_addr))
		{
			cse4589_print_and_log("[%s:ERROR]\n",token_arr[0]);
			cse4589_print_and_log("[%s:END]\n", token_arr[0]);
			return;
		}
		client_tcp_fd = client_tcp_connect(&server_addr);
		cse4589_print_and_log("[%s:SUCCESS]\n",token_arr[0]);
		cse4589_print_and_log("[%s:END]\n", token_arr[0]);
		
		char tee[20];
		memset (tee,0,20);
		strcat(tee,"PORT      ");
		sprintf(tee+10,"%d",port);
		int ttlen = strlen(tee);	
	//	printf ("going to send %s\r\n",tee);
		sendall(client_tcp_fd,tee,&ttlen);
		return;
	}
	else
	{
		if (0 == process_common_commands(token_arr[0]))
			return;
	}

	if (0 == strncmp(token_arr[0],"SENDFILE",8))
	{
	//	printf ("handling sendfile\r\n"); 
		if (token_arr[1] == NULL)
		{
			cse4589_print_and_log("[%s:ERROR]\n",token_arr[0]);
			cse4589_print_and_log("[%s:END]\n", token_arr[0]);
			return;
		}
		cse4589_print_and_log("[%s:SUCCESS]\n",token_arr[0]);
	//	printf ("going to call send_File \r\n");
		send_file(token_arr[1],token_arr[2]);
		cse4589_print_and_log("[%s:END]\n", token_arr[0]);
	}
	else if (is_login_success == 0)
	{
		return;
	}
	else if (0==strncmp(token_arr[0],"LOGOUT",6))
	{
		return;
		char tee[20];
		memset (tee,0,20);
	
		strcat(tee,"LOGOUT    ");

		int ttlen = strlen(tee);	
		sendall(client_tcp_fd,tee,&ttlen);
		is_login_success = 0;
	}
	else if (0==strcmp(token_arr[0],"REFRESH"))
	{
		list_requested = 2;
		request_message(client_tcp_fd, token_arr[0]);
	}
	else if (0==strcmp(token_arr[0],"SEND"))
	{
		if(MAX_CLIENTS == find_client_str(token_arr[1]))
		{
			cse4589_print_and_log("[%s:ERROR]\n",token_arr[0]);
			cse4589_print_and_log("[%s:END]\n", token_arr[0]);
			return;	
		}

		if (is_valid_ip(token_arr[1]))
		{
			cse4589_print_and_log("[%s:SUCCESS]\n",token_arr[0]);
			handle_client_commands( client_tcp_fd, t_arr,token_arr[1],start_of_message);
			cse4589_print_and_log("[%s:END]\n", token_arr[0]);
		}
		else
		{
			cse4589_print_and_log("[%s:ERROR]\n",token_arr[0]);
			cse4589_print_and_log("[%s:END]\n", token_arr[0]);
		}
	}
	else if (0==strncmp(token_arr[0],"BROADCAST",9))
	{
		cse4589_print_and_log("[%s:SUCCESS]\n",token_arr[0]);
		send_broadcast(client_tcp_fd, t_arr);
		cse4589_print_and_log("[%s:END]\n", token_arr[0]);
	}
	return;
}
void send_file (char *ip, char *file)
{
	printf ("send_file\r\n");
	int send_fd =socket(AF_INET,SOCK_STREAM,0);
	struct sockaddr_in s_addr;

	memset(&s_addr,0,sizeof(s_addr));

	s_addr.sin_family=AF_INET;
	s_addr.sin_addr.s_addr=htonl(ip);
	s_addr.sin_port=htons(FILE_TRANSFER_PORT);
	
	printf ("initiating connect \r\n");
	if (connect(send_fd, (struct sockaddr *) &s_addr, sizeof(struct sockaddr)))
	{
		printf ("Connect failed \r\n");
	}

	printf( "connected successfully\r\n going to requres %s",file);
	return;	
}
void send_broadcast (int client_tcp_fd, char *t_arr)
{
	char temp[1024];
	memset(temp,0,1024);
	
	char *token;
	int j =0;
	int i;
	char *token_arr[100];

	token = strtok(t_arr, " ");
   	while( token != NULL ) 
	{
		if(i>99)
		break;
		
		token_arr[i] = token;
		i++;
  		token = strtok(NULL," ");
  	}

	char temp_ip[15];
	memset(temp_ip,0,15);
	
	sprintf(temp_ip,"%15s",ip_addr);

	strcat (temp,"BROADCAST  ");
	strcat (temp,temp_ip);
	strcat (temp," ");
	for (int j=1; j<i; j++)
	{
		strcat (temp,token_arr[j]);
		strcat (temp," ");
	}

//	printf ("going to send:%s:",temp);
	int buffer_len = strlen(temp);	
	sendall(client_tcp_fd,temp,&buffer_len);
	return;	

}
void handle_client_commands(int client_tcp_fd ,char *t_arr,char *ip, int start_of_message)
{
	int buffer_len;
	char buffer[1024];

	memset(buffer,0,1024);
	sprintf(buffer,"SEND      ");
	
	int offset = strlen(buffer);
	sprintf(buffer+offset,"%15s",ip);

	offset = strlen(buffer);
	/* TBD -HARDCORDED*/
	sprintf(buffer+offset," %s",t_arr+start_of_message);
	buffer_len = strlen(buffer);
	
//	printf (" message to send : %s \r\n",buffer);
	sendall(client_tcp_fd,buffer,&buffer_len);
	return;	

}
void 
ProcessPacket (int client_tcp_fd)
{
	int i=0;
	char *token = NULL;
	char *token_arr[5];
//	printf ("Process packet\r\n ");
	
	char buf[1024];
	memset(buf,0,1024);

	recv(client_tcp_fd, buf, sizeof buf, 0);

//	printf ("<ProcessPacket: %s \r\n",buf);	

	if (0 ==strncmp(buf,"LIST-PART ",10))
	{
//		printf ("list part\r\n");
		token = strtok(buf, "\n");
   		while( token != NULL ) 
		{
			token_arr[i] = token;
			i++;
  			token = strtok(NULL,"\n");
  		}
		char temp1[1024];

		for (int j=0; j<i; j++)
		{
		memset (temp1,0,1024);
		strcpy (temp1,token_arr[j]);
//		printf (" msg: %s \r\n",temp1);
		char ip[16];
		char name[25];
		int port=0;
		int slot;
		int status;
		int in_use;	

		memset(ip,0,16);
		memset(name,0,25);
		sscanf(temp1+10,"%d %s %s %d %d %d",&slot,name,ip,&port,&status,&in_use);
//		printf ("slot: %d name: %s ip %s port %d status %d use %d \r\n",
//			slot,name,ip,port,status,in_use);	

		memset(&client_list[slot],0,sizeof(struct client_str));

		strcpy(client_list[slot].host_name, name);
		strcpy(client_list[slot].host_ip, ip);
		client_list[slot].socket = client_tcp_fd;
		client_list[slot].listen_port_num = port;
		client_list[slot].status = status;
		client_list[slot].in_use = in_use;
		}
	}
	else if (0 ==strncmp(buf,"LIST-DELE ",10))
	{
	//	printf ("list del \r\n");
		int slot = atoi(buf+10);
		memset(&client_list[slot],0,sizeof(struct client_str));		
	}
	else if (0 == strncmp(buf,"SEND",4))
	{
//		printf ("msg:rcvd:%s:\r\n",buf);
		char *tbuff[1024];
		memset (tbuff,0,1024);

		char *token ;
		char *token_arr[100];   	

		token = strtok(buf, " ");
   		while( token != NULL ) 
		{
			if(i>99)
			break;
		
			token_arr[i] = token;
			i++;
  			token = strtok(NULL," ");
  		}
//		printf ("total tokens : %d \r\n",i);

		for (int j=2; j<i; j++)
		{
			strcat (tbuff,token_arr[j]);
			strcat (tbuff," ");
		}
//		printf ("after processing: %s \r\n",tbuff);
	//	printf ("%s %s %s %d %s \r\n",token_arr[0],token_arr[1],token_arr[2],(int)(token_arr[2]-token_arr[0]),(tbuff+(token_arr[2]-token_arr[0])));	
		int length = strlen(tbuff);
		tbuff[length] ='\0';	
		cse4589_print_and_log("[RECEIVED:SUCCESS]\n");
	 	cse4589_print_and_log("msg from:%s\n[msg]:%s\r", token_arr[1], tbuff);
		cse4589_print_and_log("[RECEIVED:END]\n");
			
	}
	else if (0 == strncmp(buf,"BRAD",4))
	{
//		printf ("msg:rcvd:%s:\r\n",buf);
		char *tbuff[1024];
		memset (tbuff,0,1024);

		char *token ;
		char *token_arr[100];   	

		token = strtok(buf, " ");
   		while( token != NULL ) 
		{
			if(i>99)
			break;
		
			token_arr[i] = token;
			i++;
  			token = strtok(NULL," ");
  		}
//		printf ("total tokens : %d \r\n",i);

		for (int j=2; j<i; j++)
		{
			strcat (tbuff,token_arr[j]);
			strcat (tbuff," ");
		}
//		printf ("after processing: %s \r\n",tbuff);
	//	printf ("%s %s %s %d %s \r\n",token_arr[0],token_arr[1],token_arr[2],(int)(token_arr[2]-token_arr[0]),(tbuff+(token_arr[2]-token_arr[0])));	 

		int length = strlen(tbuff);
		tbuff[length] ='\0';	
		cse4589_print_and_log("[RECEIVED:SUCCESS]\n");
	 	cse4589_print_and_log("msg from:%s\n[msg]:%s\r", token_arr[1], tbuff);
		cse4589_print_and_log("[RECEIVED:END]\n");
			
	}
	else if (0 == strncmp(buf,"PRINT-OUT",9))
	{
		if (list_requested == 1)
			cse4589_print_and_log("[LIST:SUCCESS]\n");
		else if (list_requested == 2)
			cse4589_print_and_log("[REFRESH:SUCCESS]\n");
		printf ("%s",buf+10);
		if (list_requested == 1)
			cse4589_print_and_log("[LIST:END]\n");
		else if (list_requested == 2)
			cse4589_print_and_log("[REFRESH:END]\n");
		list_requested = 0;
	}
	else
	{
		printf ("recieved message \r\n");
		printf ("%s",buf);
	}

	return;
}

int wait_on_select( fd_set *read_fs)
{
	int max_readfd;
	FD_ZERO(read_fs);
	FD_SET (0, read_fs);
	max_readfd = 1;


#if 0
	FD_SET (ft_fd, read_fs);
	if (ft_fd+1 > max_readfd)
	{
		max_readfd = ft_fd+1;
	}	
#endif
	if (client_tcp_fd > 0)
	{
		FD_SET (client_tcp_fd, read_fs);
		max_readfd = client_tcp_fd +1;
	}

	select (max_readfd, read_fs, NULL, NULL, NULL);

	if (FD_ISSET(0, read_fs))
	{
//		printf (" Something  on keyboard\r\n");
		return 0;
	}
	else if (FD_ISSET(client_tcp_fd, read_fs))
	{
//		printf (" Something on socket \r\n");
		return client_tcp_fd;
	}
#if 0
	else if (ft_fd > 0)
	{
		if (FD_ISSET(ft_fd, read_fs))
		{
			printf ("something on file server \r\n");
			return ft_fd;
		}
	} 
#endif
	return -1;
}

int 
process_user_login(char *command, char *server_ip, char *server_port, struct sockaddr_in *server_addr)
{
//	printf ("Command :%s server ip %s server port %s \r\n",command,server_ip,server_port);

	char port[10];
	memset (port, 0, 10);
	sprintf(port,"%d",atoi(server_port));
	
	if (strcmp(port,server_port)!=0)
	{
		return -1;
	}
	
	struct hostent *host = gethostbyname(server_ip);
	server_addr->sin_family = AF_INET;
	bcopy(host->h_addr, (char *)&server_addr->sin_addr, host->h_length);
	server_addr->sin_port = htons(atoi(server_port));
	return 0;

}
int
login_user (struct sockaddr_in *server_addr)
{
	char command[10], server_ip[15];
	int server_port = 0;
	
	memset(command,0,sizeof(10));
	memset(server_ip,0,sizeof(server_ip));

	scanf("%s %s %d",command,server_ip,&server_port);
//	printf ("Command :%s server ip %s server port %d \r\n",command,server_ip,server_port);

	if (strcmp(command,"LOGIN")!=0)
	{
		return 0;
	}

	struct hostent *host = gethostbyname(server_ip);

	memset(server_addr,0,sizeof(struct sockaddr));
	server_addr->sin_family = AF_INET;
	bcopy(host->h_addr, (char *)&server_addr->sin_addr, host->h_length);
	server_addr->sin_port = htons(server_port);
//	printf ("Login success \r\n");
	return 1;
}
		

int
client_tcp_connect(struct sockaddr_in *server_addr)
{
	
	int client_fd = socket(AF_INET, SOCK_STREAM, 0);
	
	if (client_fd < 0)
	{
//		printf ("Scocket call failed \r\n");
	}

	if (connect(client_fd, (struct sockaddr *) server_addr, sizeof(struct sockaddr)))
	{
//		printf ("Connect failed \r\n");
	}
	is_login_success = 1;
//	printf ("Connection Successfull \r\n");
	return client_fd;
}

/// Taken from beej guide
int sendall(int s, char *buf, int *len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;

    while(total < *len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
}

int get_ip_addr(char *ip_addr)
{
    int n;
    struct ifreq ifr;
    char array[] = "eth0";
 
    n = socket(AF_INET, SOCK_DGRAM, 0);
    ifr.ifr_addr.sa_family = AF_INET;
 
    strncpy(ifr.ifr_name , array , IFNAMSIZ - 1);
    /* Make the ioctl call and access the network information */
    ioctl(n, SIOCGIFADDR, &ifr);
    close(n);
   
    strcpy(ip_addr, inet_ntoa(( (struct sockaddr_in *)&ifr.ifr_addr )->sin_addr));
    return 0;
}

int 
is_valid_ip (char *ip)
{
    struct sockaddr_in str;
    return (inet_pton(AF_INET, ip, &(str.sin_addr)));
} 
