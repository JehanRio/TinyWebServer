#include "lst_timer.h"
#include <libgen.h>
#include <cstdio>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <stdlib.h>

const int FD_LIMIT = 65535;
const int MAX_EVENT_NUMBER = 1024;
const int TIMESLOT = 5;

static int pipefd[2];
static sort_timer_lst timer_lst;
static int epollfd = 0;

// ���÷�����
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;   // ���ش���
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void sig_handler(int sig)
{
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char*)&msg, 1, 0); // �����ź�
    errno = save_errno;
}

void addsig(int sig)
{
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;  // SA_RESTART�ź���ʵ���������ڵ��ҳ���������е�ʱ�� �����ǰ�ĳ���������״̬
    sigfillset(&sa.sa_mask);    // ����������źţ�������
    assert(sigaction(sig, &sa, nullptr) != -1);
}

void timer_handler()
{
    // ��ʱ����������ʵ���Ͼ��ǵ���tick()����
    timer_lst.tick();   // �����������
    // ��Ϊһ�� alarm ����ֻ������һ��SIGALARM �źţ���������Ҫ���¶�ʱ���Բ��ϴ��� SIGALARM�źš�
    alarm(TIMESLOT);    // ��ʱ5s
}

// ��ʱ���ص���������ɾ���ǻ����socket�ϵ�ע���¼������ر�֮��
void cb_func(client_data* user_data)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);  // ������û��Ͽ�����ᱨ��
    close(user_data->sockfd);
    printf("close fd %d\n",user_data->sockfd);
}


int main(int argc, char** argv)
{
    if(argc <= 1)
    {
        printf("usage: %s port_number\n", basename(argv[0]));   //basename�����ǵõ��ض���·���е����һ��'/',���������
        return 1;
    }
    int port = atoi(argv[1]);   // ��ȡ�˿ں�

    int ret = 0;
    sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = INADDR_ANY;

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenfd>=0);
    
    ret = bind(listenfd, (sockaddr*)&address, sizeof(address));
    assert(ret!=-1);

    ret = listen(listenfd, 5);
    assert(ret!=-1);

    epoll_event events[MAX_EVENT_NUMBER];   // �¼�����
    int epollfd = epoll_create(7);
    assert(epollfd);
    addfd(epollfd, listenfd);

    // �����ܵ�
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);  // �����׽���ͨ��
    assert(ret!=-1);
    setnonblocking(pipefd[1]);  // ���÷�����
    addfd(epollfd, pipefd[0]);  // ������ӵ�epoll������

    // �����źŴ����� 
    addsig(SIGALRM);    // ��ʱ
    addsig(SIGTERM);    // kill�ж�
    bool stop_server = false;   

    client_data* users = new client_data[FD_LIMIT]; // ���������
    bool timeout = false;
    alarm(TIMESLOT);    // ��ʱ��ʼ

    while(!stop_server)
    {
        int num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1); // �����ȴ�
        if(num < 0 && (errno != EINTR)) // EINTR���ж�
        {
            printf("epoll failure!\n");
            break;
        }

        // ѭ�������¼�����
        for(int i=0;i<num;i++)
        {
            int sockfd = events[i].data.fd;
            if(sockfd == listenfd)
            {
                sockaddr_in cli_address;
                socklen_t len = sizeof(cli_address);
                int cfd = accept(listenfd, (sockaddr*)&cli_address, &len);
                addfd(epollfd, cfd);
                users[cfd].address = cli_address;
                users[cfd].sockfd = cfd;

                // ������ʱ����������ص������볬ʱʱ�䣬Ȼ��󶨶�ʱ�����û����ݣ���󽫶�ʱ����ӵ�����timer_lst��
                util_timer* timer = new util_timer;
                timer->user_data = &users[cfd];
                timer->cb_func = cb_func;   // �ص�����
                time_t cur = time(0);   // ��ǰʱ���¼
                timer->expire = cur + 3*TIMESLOT;
                users[cfd].timer = timer;
                timer_lst.add_timer(timer);
            }
            else if(sockfd == pipefd[0] && events[i].events & EPOLLIN)  // ��⵽pipefd[0]�Ķ��¼�
            {
                // �����ź�
                int sig;
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if(ret==-1 || ret==0)   // �쳣��Ͽ�
                    continue;
                else
                {
                    for(int i=0;i<ret;i++)
                    {
                        switch(signals[i])
                        {
                            case SIGALRM:
                                // ��timeout��������ж�ʱ������Ҫ����������������ʱ����
                                // ������Ϊ��ʱ��������ȼ����Ǻܸߣ��������ȴ�����������Ҫ������(I/O���ȼ�����)
                                timeout = true;
                                break;
                            case SIGTERM:
                                stop_server = true; // kill
                        }
                    }
                }
            }
            else if(events[i].events & EPOLLIN) // �����Ķ��¼�������д�ӡ����
            {
                memset(users[i].buf, '\0', BUFFER_SIZE);    // ��ն�������
                ret = recv(sockfd, users[i].buf, BUFFER_SIZE-1, 0);
                printf( "get %d bytes of client data %s from %d\n", ret, users[sockfd].buf, sockfd );
                util_timer* timer = users[sockfd].timer;
                if(ret < 0)
                {
                    // ���������������ر����ӣ����Ƴ����Ӧ�Ķ�ʱ��
                    if(errno != EAGAIN)
                    {
                        cb_func(&users[sockfd]);
                        if(timer)
                            timer_lst.del_timer(timer);
                    }
                }
                else if(ret == 0)
                {
                    // ����Է��Ѿ��ر����ӣ�������Ҳ�ر����ӣ����Ƴ���Ӧ�Ķ�ʱ����
                    cb_func(&users[sockfd]);
                    if(timer)
                        timer_lst.del_timer(timer);
                }
                else
                {
                    // ���ĳ���ͻ����������ݿɶ���������Ҫ���������Ӷ�Ӧ�Ķ�ʱ�������ӳٸ����ӱ��رյ�ʱ�䡣
                    if(timer)
                    {
                        time_t cur = time(0);
                        timer->expire = cur + 3*TIMESLOT;
                        printf( "adjust timer once\n" );
                        timer_lst.adjust_timer( timer );
                    }
                }
            }
        }
        // �����ʱ�¼�����ΪI/O�¼��и��ߵ����ȼ�����Ȼ�������������¶�ʱ�����ܾ�׼�İ���Ԥ����ʱ��ִ�С�
        if(timeout)
        {
            timer_handler();
            timeout = false;
        }
    }
    close(listenfd);
    close(pipefd[0]);
    close(pipefd[1]);
    delete[] users;
    return 0;
}