#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <pthread.h>
#include <wiringPi.h>
#include <pcf8574.h>
#include <lcd.h>


#define LOW     0
#define HIGH    1
#define OUTPUT  1

#define ROWS    2
#define COLS    16
#define BITS    4

#define PIN_BASE 128
#define I2C_ADDRESS 0x27

#define RS  PIN_BASE
#define RW  PIN_BASE + 1
#define E   PIN_BASE + 2
#define N   PIN_BASE + 3
#define D4  PIN_BASE + 4
#define D5  PIN_BASE + 5
#define D6  PIN_BASE + 6
#define D7  PIN_BASE + 7


#define BUF_SIZE 100

void * recv_msg(void * arg);
void error_handling(char * msg);

char msg[BUF_SIZE];

int m_lcd;
int m_timeout = 0;
int main(int argc, char *argv[])
{



    /*  lcd Client  */
    wiringPiSetup();
    pcf8574Setup(PIN_BASE, I2C_ADDRESS);

    pinMode(RW, OUTPUT);
    digitalWrite(RW, LOW);

    pinMode(N, OUTPUT);
    digitalWrite(N, HIGH);

    m_lcd = lcdInit(ROWS, COLS, BITS, RS, E, D4, D5, D6, D7, 0, 0, 0, 0);
    lcdClear(m_lcd);



    /* Socket Client */
    int sock;
    struct sockaddr_in serv_addr;
    pthread_t rcv_thread;
    void * thread_return;
    if(argc!=3) {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    lcdPosition(m_lcd, 0, 0);
    lcdPuts(m_lcd, "LCD CONTROLE");
    lcdPosition(m_lcd, 0, 1);
    lcdPuts(m_lcd, "Ready LCD!");

    sleep(1);

reconnect:
    sock=socket(PF_INET, SOCK_STREAM, 0);

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_addr.s_addr=inet_addr(argv[1]);
    serv_addr.sin_port=htons(atoi(argv[2]));

    if(m_timeout == 0){
        lcdClear(m_lcd);
        lcdPosition(m_lcd, 0, 0);
        lcdPuts(m_lcd, "SOCKET CONTROLE");
        lcdPosition(m_lcd, 0, 1);
        lcdPuts(m_lcd, "CONNECT....");
    }
    else{
        lcdPosition(m_lcd, 0, 0);
        lcdPuts(m_lcd, "RECONNECT..ing");
    }
    sleep(1);
    if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))==-1){
        if(m_timeout == 0){
            lcdClear(m_lcd);
            lcdPosition(m_lcd, 0, 0);
            lcdPuts(m_lcd, "SOCKET CONTROLE");
            lcdPosition(m_lcd, 0, 1);
            lcdPuts(m_lcd, "CONNECT ERROR!!");
            error_handling("connect() error");
        }
        else
            goto reconnect;
    }
    m_timeout = 0;
    pthread_create(&rcv_thread, NULL, recv_msg, (void*)&sock);
    pthread_join(rcv_thread, &thread_return);
    if(m_timeout == 1)
        goto reconnect;
    close(sock);
    return 0;
}

void * recv_msg(void * arg)   // read thread main
{
    int sock=*((int*)arg);
    char msg[BUF_SIZE];
    char temp_msg[BUF_SIZE];
    int str_len;
    int state;
    struct timeval tv;
    fd_set readfds;


    lcdClear(m_lcd);
    lcdPosition(m_lcd, 0, 0);
    lcdPuts(m_lcd, "SOCKET CONTROLE");
    lcdPosition(m_lcd, 0, 1);
    lcdPuts(m_lcd, "CONNECT OK!");
    sleep(1);

    while(1)
    {
        str_len=read(sock, msg, BUF_SIZE-1);
        FD_ZERO(&readfds);
        FD_SET(str_len, &readfds);
        tv.tv_sec = 5;
        tv.tv_usec = 10;
        state = select(str_len+1, &readfds, (fd_set *)0, (fd_set *)0, &tv);
        switch(state){
            case -1:
                perror("select error :");
                exit(0);
                //TIME OUT
            case 0:
                m_timeout = 1;
                printf("Time out error\n");
                lcdClear(m_lcd);
                lcdPosition(m_lcd, 0, 0);
                lcdPuts(m_lcd, "SERVER MESSAGE");
                lcdPosition(m_lcd, 0, 1);
                lcdPuts(m_lcd, msg);
                return (void*)-1;

            default:
                if(str_len != -1){
                    msg[str_len]=0;
                    printf("Server : %s\n", msg);
                    lcdClear(m_lcd);
                    lcdPosition(m_lcd, 0, 0);
                    lcdPuts(m_lcd, "SERVER MESSAGE");
                    lcdPosition(m_lcd, 0, 1);
                    lcdPuts(m_lcd, msg);
                }
                break;
        }       
    }
    return NULL;
}

void error_handling(char *msg)
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}

