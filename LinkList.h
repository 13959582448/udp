#include<stdio.h>
#include<stdlib.h>
#include<time.h>
#include<stdio.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<unistd.h>
#include<netinet/in.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<string.h>
#include<arpa/inet.h>
#include<pthread.h>
#include<time.h>
#define valtype clientTable

typedef struct Table
{
    char id[12];
    struct sockaddr_in addr;
    int state;
    unsigned short timeout;
}clientTable;

typedef struct Parameter
{
    char flag;   //查询的响应报文中判断状态的
    int seq;            //序列号
    struct sockaddr_in addr;    //地址信息 
    //可能是自己的也可能是请求的响应信息
    char data[64];
}parameter;

typedef struct JSONmsg
{
    char protocol[12];  //JSON-RPC2.0
    char method[12];    //方法名
    char id[12];        //标识 
    parameter params;   //具体的信息
}jsonMsg;

typedef struct Node
{
    valtype value;
    struct Node* next;
}node;

node* createNode(valtype value)//创建一个节点
{
    node* head=(node*)malloc(sizeof(node));
    head->value=value;
    head->next=NULL;
    return head;
}

int insert_back(node* head,node *newNode)
//在链表的最后添加一个节点
{
    node* tmp=head;
    while(tmp->next)tmp=tmp->next;
    tmp->next=newNode;
    return 1;
}

int destroy(node *head)
{
    node *tmp;
    while(head->next)
    {
        tmp=head;
        if(1)
        {
            free(head); //扩充
        }
        head=tmp->next;
    }
    free(head);
}

node* searchByID(node* head,char searchID[12])
//以ID为关键字寻找链表中的节点
{
    node* tmp=head;
    while(tmp)
    {
        if(strcmp(searchID,tmp->value.id)==0)return tmp;
        tmp=tmp->next;
    }
    return NULL;
}
int getLength(node* head)
{
    node* tmp=head;
    int count=0;
    while(tmp)
    {
        count++;
        tmp=tmp->next;
    }
    return count;
}

typedef struct n
{
    struct sockaddr_in cliaddr;
    jsonMsg msg;
    struct n *next;
}buffBlock; //存储未确认的报文 可能要重发

buffBlock* createBlock(struct sockaddr_in cliaddr,jsonMsg msg)
    //创建链表
{
    buffBlock* head=(buffBlock*)malloc(sizeof(buffBlock));
    head->msg=msg;
    head->cliaddr=cliaddr;
    head->next=NULL;
    return head;
}

int insert_block_back(buffBlock* head,buffBlock* newblock)
 //插入节点
{
    buffBlock* tmp=head;
    while(tmp->next)tmp=tmp->next;
    tmp->next=newblock;
    return 1;
}

int getBlocksLength(buffBlock* head)
//获取长度
{
    int count=0;
    buffBlock* tmp=head;
    while(tmp)
    {
        count++;
        tmp=tmp->next;
    }
    return count;
}

void destroyBlocks(buffBlock* head)
//销毁
{
    buffBlock* tmp=head;
    if(!tmp)return;
    while(head->next)
    {
        tmp=head;
        free(head);
        head=tmp->next;
    }
    free(head);
}

void removeBlock(buffBlock** head,int seq) //头节点指针的地址
//删除缓存块
{
    buffBlock* tmp=*head;
    buffBlock* pre=(buffBlock*)malloc(sizeof(buffBlock));
    pre->next=tmp;
    
    if(!(*head))
    {
        free(pre);
        return;
    }
    
    while(tmp->msg.params.seq!=seq&&tmp)
    {
        pre=pre->next;
        tmp=tmp->next;
    }
    if(tmp==(*head)) 
    //节点为空或者要删除的就是头节点
    {
        free((*head));
        (*head)=NULL;
    }
    else if(tmp)
    //非头部
    {
        pre->next=tmp->next;
        free(tmp);
    }
    
}