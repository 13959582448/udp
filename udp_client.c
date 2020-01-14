#include"LinkList.h"
#define bzero(ptr,n) memset(ptr,0,n)
#define SA struct sockaddr
#define SLEEPTIME 10 //等待重发和发送存活信息的时间间隔
#define BUFFSIZE 10 //最大缓冲等待确认的报文数量

void* report(); //向服务器报告自己还存活的线程函数
void* getMsg(); //接收消息的线程函数
void* sendMsg(void* arg); //发送消息的线程函数
void* reSend(); //超时重发的线程函数
void end();//无意义的函数

static int sockfd;
static socklen_t addrlen=sizeof(struct sockaddr_in);
static char myID[12];
static struct sockaddr_in servaddr; //服务器地址
//static struct sockaddr_in myaddr;
static int found=0; //判断是否已经得到想要查询的地址
static struct sockaddr_in myaddr;
static buffBlock* block_head=NULL;
static int resend=0;
static int test=0;

int main(int argc,char **argv)
{

    if(argc!=6)
    {
        fprintf(stderr,"Usage <servAddr servPort myAddr myPort myID>");
        exit(0);
    }
    strcpy(myID,argv[5]);
    servaddr.sin_addr.s_addr=inet_addr(argv[1]);
    servaddr.sin_family=AF_INET;
    servaddr.sin_port=htons(atoi(argv[2]));
    sockfd=socket(AF_INET,SOCK_DGRAM,0);   

    myaddr.sin_addr.s_addr=inet_addr(argv[3]);
    myaddr.sin_family=AF_INET;
    myaddr.sin_port=htons(atoi(argv[4]));
    bind(sockfd,(SA*)&myaddr,addrlen);    

    pthread_t tid1,tid2,tid3,tid4;
    pthread_create(&tid1,NULL,(void*)&report,NULL);
    //tid1是报告存活的线程
    pthread_create(&tid2,NULL,(void*)&getMsg,NULL);
    //tid2是接收消息的线程
    pthread_create(&tid3,NULL,(void*)&sendMsg,NULL);
    //tid3是发送消息的线程
    pthread_create(&tid4,NULL,(void*)&reSend,NULL);
    //tid4是等待重发的线程

    pthread_exit(NULL);    
}

void *getMsg()
{

    pthread_detach(pthread_self());
    while(1)
    {
        jsonMsg buff;
        struct sockaddr_in cliaddr;
        recvfrom(sockfd,&buff,sizeof(buff),0,(SA*)&(cliaddr),&addrlen);
        if(test)printf("get %s msg\n",buff.method);
        if(strcmp(buff.method,"response")==0)//服务器的响应报文
        {
            sendMsg((void*)&buff); 
        }
        else if(strcmp(buff.method,"comm")==0)//其他客户端的报文
        {
            time_t t;
            time(&t);            
            const char *timeStamp=(const char *)ctime(&t);
            printf("\n\nFrom: %s\t%s",buff.id,timeStamp);
            fputs(buff.params.data,stdout);
            printf("\n\n"); //输出

            jsonMsg sendbuff; //发送确认收到的报文
            sendbuff.params.seq=buff.params.seq;
            strcpy(sendbuff.method,"ack");
            sendto(sockfd,&sendbuff,sizeof(sendbuff),0,(SA*)&cliaddr,addrlen);
            if(test) printf("jsut send ack to %d\n",ntohs(cliaddr.sin_port));
            //测试用的
        }
        else if(strcmp(buff.method,"ack")==0) //处理确认报文
        {
            removeBlock(&block_head,buff.params.seq);
            if(test)printf("get ack from %d now length:%d\n",ntohs(cliaddr.sin_port),getBlocksLength(block_head));
        }
        bzero(&buff,sizeof(buff));
    }
}

void *sendMsg(void* arg) 
{
    srand((unsigned)time(NULL));
    struct sockaddr_in cliaddr; //通信的另一个客户端
    jsonMsg buff; //buff是读取的信息,sendmsg是准备发送的

    if(arg!=NULL) //从另一个线程中得到的
    {
        if(found==1)goto END; //判断是否已经收到服务器回复
        //直接结束函数
        buff=*((jsonMsg *)arg);
        cliaddr=buff.params.addr; //获取想要查询的地址
        if(buff.params.flag=='1')printf("Found %s's address!\n",buff.id);
        if(buff.params.flag=='0')printf("%s is out of line!\n",buff.id);
        if(buff.params.flag=='N')printf("%s not found\n",buff.id);
        if(buff.params.flag!='1')goto Here;
        else found=1;
    }
    else //从本线程得到的
    {
        if(found==1)goto END;//结束函数
        jsonMsg msg;
    Here:

        printf("Enter id:");
        char id[12];//想要查询的ID信息
        scanf("%s",id);
        getchar();
        strcpy(msg.id,id);
        strcpy(msg.method,"query");
        strcpy(msg.protocol,"JSON-RPC2.0");
        sendto(sockfd,&msg,sizeof(msg),0,(SA*)&servaddr,addrlen);
        //等待服务器回应
        //printf("waiting");
        recvfrom(sockfd,&buff,sizeof(buff),0,(SA*)&cliaddr,&addrlen);
        cliaddr=buff.params.addr; //获取想要查询的地址
        if(buff.params.flag=='1')printf("%s's address found!\n",id);
        if(buff.params.flag=='0')printf("%s is out of line!\n",id);
        if(buff.params.flag=='N')printf("%s not found\n",id);

        if(buff.params.flag!='1')goto Here;
        else found=1;        
    }

    //开始发送信息
    printf("Now you can talk !\n");
    while(1)
    {
        jsonMsg sendmsg;
        bzero(&sendmsg,sizeof(sendmsg)); //清空
        printf("enter message:");
        fgets(sendmsg.params.data,sizeof(sendmsg.params.data),stdin);//读取具体内容
        
        if(strcmp(sendmsg.params.data,"EOF\n")==0)
        {
            destroyBlocks(block_head);
            printf("Goodbye...\n");
            exit(0);
        }
        strcpy(sendmsg.id,myID);
        strcpy(sendmsg.method,"comm");
        strcpy(sendmsg.protocol,"JSON-RPC2.0"); //发送method="comm"的报文给另一个客户端
        sendmsg.params.seq=rand()%1000000; //设置足够大的序列号范围
        sendto(sockfd,&sendmsg,sizeof(sendmsg),0,(SA*)&cliaddr,addrlen);
        resend=1; //可以开始重发
        if(!block_head)
        {
            block_head=createBlock(cliaddr,sendmsg); //创建一个缓冲区头部
        }
        else
        {
            insert_block_back(block_head,createBlock(cliaddr,sendmsg));
            //插入一个新的值
        }
        
        int length;
        while((length=getBlocksLength(block_head))>=BUFFSIZE)
        {
            printf("\nno more empty buffBlock!Wait a moment!\n");
            sleep(3);
        }
    }
    END:
        end();
}

void* reSend()
{
    pthread_detach(pthread_self());
    jsonMsg msg;
    struct sockaddr_in cliaddr;
    while(1)
    {
        while(!resend); //控制开关 说明已经开始发送数据可以准备重发
        sleep(SLEEPTIME); //定时重发
        for(buffBlock* tmp=block_head;tmp;tmp=tmp->next)
        {
            msg=tmp->msg;
            cliaddr=tmp->cliaddr; //从缓冲区中取出未确认的报文
            sendto(sockfd,&msg,sizeof(msg),0,(SA*)&cliaddr,addrlen);
        }
        if(test)printf("resend length:%d\n",getBlocksLength(block_head));
    }
}

void *report() //存活报文
{
    pthread_detach(pthread_self()); //分离
    while(1)
    {
        sleep(SLEEPTIME); //一定间隔发送
        jsonMsg msg;
        strcpy(msg.id,myID);
        strcpy(msg.method,"alive"); //method="alive"的报文
        sendto(sockfd,&msg,sizeof(msg),0,(SA*)&servaddr,addrlen);
    }
}
void end()
{
    //只是为了用goto到函数结尾且无warning才定义的无意义的函数
}