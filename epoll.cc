#include "epoll.h"

void EpollServer::Start()
{
    _listenfd = socket(PF_INET,SOCK_STREAM,0);
    if(_listenfd == -1)
    {
        ErrorLog("create soket");
        return;
    }

    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(_listenfd,(struct sockaddr*)&addr,sizeof(addr)) < 0)
    {
        ErrorLog("bind error");
        return;
    }
    
    if(listen(_listenfd,100000) < 0)
    {
        ErrorLog("listen");
        return;
    }

    TraceLog("epoll server listen on %d",_port);
    
    _eventfd = epoll_create(100000);
    if(_eventfd == -1)
    {
        ErrorLog("epoll_create");
        return;
    }

    //添加listenfd 到epoll，监听连接事件
    SetNonblocking(_listenfd);
    OPEvent(_listenfd, EPOLLIN, EPOLL_CTL_ADD);

    //进入事件循环
    EventLoop();
}

void EpollServer::EventLoop()
{
    struct epoll_event events[100000];
    while(1)
    {
        //epoll_wait 最后一个参数timeout，-1代表永远阻塞，0代表调用立即返回
        int n = epoll_wait(_eventfd, events, 100000,0);
        for(int i = 0;i < n; ++i)
        {
            if(events[i].data.fd == _listenfd)
            {
                struct sockaddr clientaddr;
                socklen_t len;
                int connectfd = accept(_listenfd, &clientaddr, &len);
                if(connectfd < 0)
                    ErrorLog("accept");

                TraceLog("new cnonnect...");
                ConnectEventHandle(connectfd);
            }
            else if(events[i].events & EPOLLIN)  //读事件
                ReadEventHandle(events[i].data.fd);
            else if(events[i].events & EPOLLOUT) //写事件
                WriteEventHandle(events[i].data.fd);
            else 
                ErrorLog("event %d",events[i].data.fd);

        }
    }
}


void EpollServer::Forwarding(Channel* clientChanne,Channel* serverChannel,bool sendencry,bool recvdecrypt)
{
    char buf[4096]; // 我一次就recv 4k 的字节
    int rlen = recv(clientChanne->_fd,buf,4096,0);
    signal(SIGPIPE,SIG_IGN);
    if(rlen < 0)
    {
        ErrorLog("recv : %d",clientChanne->_fd);
    }
    else if(rlen == 0) // 对端进入四次挥手
    {
        //client channel 发起关闭
        // client 发送recv一个0,就说明client 不会给我的server发数据了
        // socks 服务器的读是不会关闭的,它要是关闭了如何读到两端的数据
        shutdown(serverChannel->_fd,SHUT_WR);  // 关的是我的socks 服务器的write,意思是我不会再向server端写
        RemoveConnect(clientChanne->_fd);
    }
    else 
    {
        // 万一我这边一次性发不完就会出现问题
        /*
        int slen = send(serverChannel->_fd,buf,rlen,0);
        TraceLog("recv:%d->send:%d",rlen,slen);
        */
        // 因此我们通过事件循环来发
        if(recvdecrypt)
        {
            Decrypt(buf,rlen);
        }
        if(sendencry)
        {
            Encry(buf,rlen);
        }
        buf[rlen] = '\0';
        SendInLoop(serverChannel->_fd,buf,rlen);
    }
}


void EpollServer::SendInLoop(int fd, const char* buf,int len)
{
    int slen = send(fd,buf,len,0);
    if(slen < 0)
    {
        ErrorLog("send to %d",fd);
    }
    else if(slen < len)
    {
        TraceLog("recv %d bytes,send %d bytes,left %d send in loop",len,slen,len-slen);
        map<int,Connect*>::iterator it = _fdConnectMap.find(fd);
        if(it != _fdConnectMap.end())
        {
            Connect* con = it->second;
            Channel* channel = &con->_clientChannel;
            if(fd == con->_serverChannel._fd)
            {
                channel = &con->_serverChannel;
            }
            // EPOLLONESHOT 只会通知我一次，万一还是没有写完，那么我就继续调用SendInLoop
            int events = EPOLLOUT | EPOLLIN | EPOLLONESHOT;
            OPEvent(fd,events,EPOLL_CTL_MOD);

            channel->_buff.append(buf+slen);
        }
        else 
        {
            assert(false);
        }

    }
}



void EpollServer::RemoveConnect(int fd)
{
    OPEvent(fd,0,EPOLL_CTL_DEL);
    map<int,Connect*>::iterator it = _fdConnectMap.find(fd);
    if(it != _fdConnectMap.end())
    {
        Connect* con = it->second;
        if(--con->_ref == 0)
        {
            delete con;
            _fdConnectMap.erase(it);
        }
    }
    else 
    {
        assert(false);
    }
}


void EpollServer::WriteEventHandle(int fd)
{
    map<int,Connect*>::iterator it = _fdConnectMap.find(fd);
    if(it != _fdConnectMap.end())
    {
        Connect* con = it->second;
        Channel* channel = &con->_clientChannel;
        if(fd == con->_serverChannel._fd)
        {
            channel = &con->_serverChannel;
        }
        string buff;
        /*
        buff += channel->_buff;
        channel->_buff.clear();
        */
        buff.swap(channel->_buff);
        SendInLoop(fd,buff.c_str(),buff.size());
    }
    else 
    {
        assert(fd);
    }
}
