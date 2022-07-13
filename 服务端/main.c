#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <malloc.h>
typedef int ElemType;
typedef struct LNode
{
    ElemType data;
    struct LNode *next; //指向后继结点
} LinkNode;             //声明单链表结点类型

char sendbuf[128] = {0};
int send_flag = 0;
LinkNode client_t = {0, NULL};
LinkNode *client = &client_t;

int connum = 0;

void InitList(LinkNode *L)
{
    L = (LinkNode *)malloc(sizeof(LinkNode)); //创建头结点
    L->next = NULL;
}

int LocateElem(LinkNode *L, ElemType e)
{
    LinkNode *p = L->next;
    int n = 1;
    while (p != NULL && p->data != e)
    {
        p = p->next;
        n++;
    }
    if (p == NULL)
        return (0);
    else
        return (n);
}

int ListInsert(LinkNode *L, int i, ElemType e)
{
    int j = 0;
    LinkNode *p = L, *s;
    if (i <= 0)
        return 0;                  // i错误返回假
    while (j < i - 1 && p != NULL) //查找第i-1个结点p
    {
        j++;
        p = p->next;
    }
    if (p == NULL) //未找到位序为i-1的结点
        return 0;
    else //找到位序为i-1的结点*p
    {
        s = (LinkNode *)malloc(sizeof(LinkNode)); //创建新结点*s
        s->data = e;
        s->next = p->next; //将s结点插入到结点p之后
        p->next = s;
        return 1;
    }
}

int ListDelete(LinkNode *L, int i, ElemType *e)
{
    int j = 0;
    LinkNode *p = L, *q;
    if (i <= 0)
        return 0;                  // i错误返回假
    while (j < i - 1 && p != NULL) //查找第i-1个结点
    {
        j++;
        p = p->next;
    }
    if (p == NULL) //未找到位序为i-1的结点
        return 0;
    else //找到位序为i-1的结点p
    {
        q = p->next; // q指向要删除的结点
        if (q == NULL)
            return 0; //若不存在第i个结点,返回false
        *e = q->data;
        p->next = q->next; //从单链表中删除q结点
        free(q);           //释放q结点
        return 1;
    }
}

//反馈发送线程
void *sendWithClient(void *arg)
{
    while (1)
    {
        if (send_flag == 1)
        {
            LinkNode *p = client->next;
            while (p->next != NULL)
            {
                write(p->data, sendbuf, strlen(sendbuf));
                p = p->next;
            }
            write(p->data, sendbuf, strlen(sendbuf));
            send_flag = 0;
        }
    }
}

//线程函数，用于和客户端进行通信
void *communicateWithClient(void *arg)
{
    int fd_client = *(int *)arg;
    int i = 0;
    int e = 0;
    //循环地和客户端进行通信
    ssize_t ret;
    while (1)
    {
        char buf[128] = {0};
        ret = read(fd_client, buf, sizeof(buf));
        if (ret == -1)
        {
            perror("read data from client error");
            exit(1);
        }
        else if (ret == 0)
        {
            //客户端断开连接
            printf("client disconnect!\n");
            ListDelete(client, LocateElem(client, fd_client), &e); //从链表上取下来
            connum--;
            break;
        }
        //打印收到的数据
        printf("read data:%s\n", buf);
        if (strcmp(buf, "cmd open door") == 0)
        {
            strcpy(sendbuf, "open door");
            send_flag = 1;
        }
        else if (strcmp(buf, "cmd close door") == 0)
        {
            strcpy(sendbuf, "close door");
            send_flag = 1;
        }
        else if (strcmp(buf, "cmd clear all fingerprints") == 0)
        {
            strcpy(sendbuf, "clear all fingerprints");
            send_flag = 1;
        }
        else if (strcmp(buf, "cmd record fingerprint") == 0)
        {
            strcpy(sendbuf, "record fingerprint");
            send_flag = 1;
        }
        else if (strcmp(buf, "door:0 ") == 0 || strcmp(buf, "door:1 ") == 0)
        {
            int fd_state = open("./state.txt", O_RDWR);
            if (fd_state == -1)
            {
                printf("open error...\n");
                continue;
            }
            write(fd_state, buf, strlen(buf));
            close(fd_state);
            strcpy(sendbuf, buf);
            send_flag = 1;
        }
        else if (strcmp(buf, "warn:1\n") == 0)
        {
            printf("unknown person is trying to open your door!!!\n");
            strcpy(sendbuf, "close door");
            send_flag = 1;
            sleep(1);
            strcpy(sendbuf, "unknown person is trying to open your door!!!\n");
            send_flag = 1;
        }
        else if (strcmp(buf, "cmd change password") == 0)
        {
            strcpy(sendbuf, "change password");
            send_flag = 1;
        }
        else if (buf[0] == 'p' && buf[1] == 'a' && buf[2] == 's' && buf[3] == 's' && buf[4] == 'w' && buf[5] == 'o' && buf[6] == 'r' && buf[7] == 'd')
        {
            int fd_psd = open("./password.txt", O_RDWR);
            if (fd_psd == -1)
            {
                printf("open password.txt error...\n");
                continue;
            }
            write(fd_psd, buf, strlen(buf));
            strcpy(sendbuf, buf);
            close(fd_psd);
            send_flag = 1;
        }
    }
}

int main()
{
    //创建套接字
    int fd_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_sock == -1)
    {
        printf("create socket error...\n");
        return -1;
    }

    //绑定ip地址和端口号
    struct sockaddr_in ser_addr;
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_port = htons(45678);
    inet_aton("192.168.199.15", &(ser_addr.sin_addr));
    int ret = bind(fd_sock, (struct sockaddr *)(&ser_addr), sizeof(ser_addr));
    if (ret == -1)
    {
        printf("bind error...\n");
        close(fd_sock);
        return -1;
    }
    printf("bind finish!\n");

    //开启监听
    ret = listen(fd_sock, 10);
    if (ret == -1)
    {
        printf("listen error...\n");
        close(fd_sock);
        return -1;
    }
    printf("start to listen...\n");
    pthread_t send;
    ret = pthread_create(&send, NULL, sendWithClient, NULL);
    //等待与客户端连接
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);
    while (1)
    {
        int fd_client = accept(fd_sock, (struct sockaddr *)(&client_addr), &addrlen);
        if (-1 == fd_client)
        {
            printf("accept error...\n");
            close(fd_sock);
            return -1;
        };
        //连接成功，打印客户端的IP地址
        printf("client ip:%s\n", inet_ntoa(client_addr.sin_addr));
        write(fd_client, "connected", strlen("connected"));
        int fd_state = open("./state.txt", O_RDONLY);
        char b[16] = {0};
        read(fd_state, b, 7);
        close(fd_state);
        write(fd_client, b, strlen(b));
        sleep(3);
        int fd_psd = open("./password.txt", O_RDONLY);
        read(fd_psd, b, 15);
        close(fd_psd);
        write(fd_client, b, strlen(b));
        ListInsert(client, (++connum), fd_client); //连接成功把标识符挂到链表上
        //创建一个线程，用于和客户端进行通信
        pthread_t thread;
        ret = pthread_create(&thread, NULL, communicateWithClient, &fd_client);
        if (ret != 0)
        {
            perror("pthread_create error");
            return -1;
        }
    }

    close(fd_sock);
    return 0;
}