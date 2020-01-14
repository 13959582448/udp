#include"LinkList.h"
#define bzero(ptr,n) memset(ptr,0,n)
#define SA struct sockaddr
#define SLEEPTIME 10
//每次检查的时间
#define COUNT 60 
//timeout超时标记达到60说明不在线了

void* response(); //服务器响应客户端的请求
void* check(); //服务器检查已经失效的节点
void setValue(jsonMsg*,clientTable*,struct sockaddr_in);
//解析报文并赋值

static node* head;//链表头节点
static int sockfd;
static int test=0;

int main(int argc,char **argv)
{
    struct sockaddr_in servaddr,cliaddr;
    head=NULL;
    socklen_t socklen=sizeof(struct sockaddr_in);

    if(argc!=3)//需要输入端口
    {
        fprintf(stderr,"Usage <IPaddr Port>");
        exit(0);
    }

    servaddr.sin_addr.s_addr=inet_addr(argv[1]);
    servaddr.sin_family=AF_INET;
    servaddr.sin_port=htons(atoi(argv[2]));
    sockfd=socket(AF_INET,SOCK_DGRAM,0);
    bind(sockfd,(SA*)&servaddr,sizeof(servaddr));   //绑定套接字
    
    pthread_t tid1,tid2;
    pthread_create(&tid1,NULL,(void *)&check,NULL); //检测存活的线程
    pthread_create(&tid2,NULL,(void *)&response,NULL);//处理请求的线程
    pthread_exit(NULL);//不自动关闭其他线程
}

void *check()
{
    pthread_detach(pthread_self());
    while(1)
    {
        sleep(SLEEPTIME);
        if(head)
        {
            node* tmp=head;
            for(;tmp!=NULL;tmp=tmp->next)
            {
                tmp->value.timeout++; //超时标记
                if(tmp->value.timeout>=COUNT)
                {
                    tmp->value.state=0; //掉线
                    if(test)printf("%s is out of line\n",tmp->value.id);
                }
            }
        }
    }
}

void setValue(jsonMsg* buff,clientTable* t,struct sockaddr_in cliaddr)
{
    strcpy(t->id,buff->id);
    t->addr=cliaddr;
    t->state=1;
    t->timeout=0;
}

void *response()
{
    pthread_detach(pthread_self());//分离
    jsonMsg buff;
    struct sockaddr_in cliaddr; 
    socklen_t addrlen=sizeof(cliaddr);
    while(1)
    {
        recvfrom(sockfd,&buff,sizeof(buff),0,(SA *)&cliaddr,&addrlen);
        //接受客户端的请求
        if(strcmp(buff.method,"alive")==0) //判断方法 
        {                       //alive是客户端证明自己存活的方法
            if(!head)   //链表为空
            {
                clientTable t;
                setValue(&buff,&t,cliaddr);
                head=createNode(t); //创建一个节点
            }
            else
            {
                node* flag=searchByID(head,buff.id);
                if(flag==NULL)  //在链表中找不到
                {
                    clientTable t;
                    setValue(&buff,&t,cliaddr);
                    insert_back(head,createNode(t));
                }
                else    //在链表中找到了
                {
                    flag->value.state=1;
                    flag->value.timeout=0;
                }
            }

        }
        else if(strcmp(buff.method,"query")==0)//客户端查询
        {
            node* tmp;
            jsonMsg msg;
            tmp=searchByID(head,buff.id);
            int length=getLength(head);
            if(tmp) //找到了对应的节点
            {
                strcpy(msg.method,"response"); //给报文赋值
                strcpy(msg.protocol,"JSON-RPC2.0");
                msg.params.addr=tmp->value.addr;
                strcpy(msg.id,tmp->value.id);
                if(tmp->value.state==1)msg.params.flag='1';
                else msg.params.flag='0'; //flag是查询结果的标志
                
                sendto(sockfd,&msg,sizeof(msg),0,(SA *)&cliaddr,addrlen);
                //查询结果返回给客户端
                if(test)printf("Found %s and length:%d\n",buff.id,length);
                if(test)printf("sent response %s %d\n",inet_ntoa(cliaddr.sin_addr),ntohs(cliaddr.sin_port));
            }
            else
            {
                strcpy(msg.method,"response");
                strcpy(msg.protocol,"JSON-RPC2.0");
                msg.params.flag='N';
                sendto(sockfd,&msg,sizeof(msg),0,(SA*)&cliaddr,addrlen);
                if(test)printf("Not found %s length=%d\n",buff.id,length);
            }
        }
    }
}
