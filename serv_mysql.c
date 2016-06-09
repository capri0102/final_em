#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <mysql.h>

#define DB_HOST "127.0.0.1"
#define DB_USER "root"
#define DB_PASS "1234qwer"
#define DB_NAME "em_01"
#define CHOP(x) x[strlen(x) - 1] = ' '


#define BUF_SIZE 100
#define MAX_CLNT 256

void * handle_clnt(void * arg);
void send_msg(char * msg, int len);
void error_handling(char * msg);


int clnt_cnt=0;
int clnt_socks[MAX_CLNT];
pthread_mutex_t mutx;

MYSQL *connection = NULL, conn;
MYSQL_RES *sql_result;
MYSQL_ROW sql_row;
int query_stat;


int main(int argc, char *argv[])
{
	int serv_sock, clnt_sock;
	struct sockaddr_in serv_adr, clnt_adr;
	int clnt_adr_sz;
	pthread_t t_id;

	mysql_init(&conn);
	connection = mysql_real_connect(&conn, DB_HOST,
					 DB_USER, DB_PASS,
					 DB_NAME, 3306, 
					(char *)NULL, 0);
	/* Mysql connection start */
	if(connection == NULL){
		fprintf(stderr, "Mysql connection error : %s", mysql_error(&conn));
		exit(1);
	}
	else
		printf("Mysql connection success\n");

	/* Mysql connection end */

	if(argc!=2) {
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}
  
	pthread_mutex_init(&mutx, NULL);
	serv_sock=socket(PF_INET, SOCK_STREAM, 0);

	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family=AF_INET; 
	serv_adr.sin_addr.s_addr=htonl(INADDR_ANY);
	serv_adr.sin_port=htons(atoi(argv[1]));
	
	if(bind(serv_sock, (struct sockaddr*) &serv_adr, sizeof(serv_adr))==-1)
		error_handling("bind() error");
	if(listen(serv_sock, 5)==-1)
		error_handling("listen() error");
	
	while(1)
	{
		clnt_adr_sz=sizeof(clnt_adr);
		clnt_sock=accept(serv_sock, (struct sockaddr*)&clnt_adr,&clnt_adr_sz);
		
		pthread_mutex_lock(&mutx);
		clnt_socks[clnt_cnt++]=clnt_sock;
		pthread_mutex_unlock(&mutx);
	
		pthread_create(&t_id, NULL, handle_clnt, (void*)&clnt_sock);
		pthread_detach(t_id);
		printf("Connected client IP: %s \n", inet_ntoa(clnt_adr.sin_addr));
	}
	close(serv_sock);
	return 0;
}
	
void * handle_clnt(void * arg)
{
	int clnt_sock=*((int*)arg);
	int i;
	char msg[BUF_SIZE];
	while(1){
		printf("m_mysql_get start!\n");
		/* Mysql query start */
		query_stat = mysql_query(connection, "select * from a ORDER BY idx DESC limit 1");

		if(query_stat != 0){
			fprintf(stderr, "Mysql query error : %s", mysql_error(&conn));
			exit(1);
		}
		else
			printf("Mysql query get success\n");

		sql_result = mysql_store_result(connection);

		sql_row = mysql_fetch_row(sql_result);
		printf("GET QUERY : %s\n", sql_row[1]);
		strcpy(msg, sql_row[1]);

		mysql_free_result(sql_result);
		
		printf("m_mysql_get end : %s\n", msg);
		if((write(clnt_sock, msg, strlen(msg))) == -1)
			break;
		else
			sleep(3);
	}
	pthread_mutex_lock(&mutx);
	for(i=0; i<clnt_cnt; i++)   // remove disconnected client
	{
		if(clnt_sock==clnt_socks[i])
		{
			while(i++<clnt_cnt-1)
				clnt_socks[i]=clnt_socks[i+1];
			break;
		}
	}
	clnt_cnt--;
	pthread_mutex_unlock(&mutx);
	close(clnt_sock);
	return NULL;
}
void send_msg(char * msg, int len)   // send to all
{
	int i;
	printf("mutex_lock start\n");
	pthread_mutex_lock(&mutx);
	for(i=0; i<clnt_cnt; i++)
		write(clnt_socks[i], msg, len);
	pthread_mutex_unlock(&mutx);
}
void error_handling(char * msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}

