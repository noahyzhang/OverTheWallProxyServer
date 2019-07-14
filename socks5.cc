#include "socks5.h"


void Sock5Server::ConnectEventHandle(int connectfd)
{
    TraceLog("new connect event:%d",connectfd);
    //添加connect 事件到epoll 中,监听读事件
    SetNonblocking(connectfd);
    OPEvent(connectfd,EPOLLIN,EPOLL_CTL_ADD);
    
    Connect* con = new Connect;
    con->_state = AUTH;   //身份认证状态
    con->_clientChannel._fd = connectfd;
    _fdConnectMap[connectfd] = con;
    con->_ref++;

}

// 0 表示数据没有到，继续等待
// 1 成功
// -1 失败
int Sock5Server::AuthHandle(int fd) //这块身份认证一般会给我发送3个字节
{
    char buf[260];
    int rlen = recv(fd,buf,260,MSG_PEEK); // MSG_PEEK 窥探一下并不是真的读走

    if(rlen <= 0)
    {
        return -1;
    }
    else if(rlen < 3)
    {
        return 0;
    }
    else 
    {
        recv(fd,buf,rlen,0);
        Decrypt(buf,rlen);
        if(buf[0] != 0X05)  //版本号
        {
            ErrorLog("not socks5");
            return -1;
        }
        return 1;
    }
}

// 失败 -1
// 数据没到返回 -2
// 连接成功 返回 serverfd 
int Sock5Server::EstablishmentHandle(int fd)
{
    // 当传过来是域名时，他DST.ADDR 这个字段的第一个字节保存的是域名的长度。
    // 也就是说这个域名的长度不会超过256位。
    char buf[256];
    int rlen = recv(fd,buf,256,MSG_PEEK);
    TraceLog("Establishment recv:%d",rlen);
    if(rlen <= 0)
    {
        return -1;
    }
    else if(rlen < 10)
    {
        return -2;
    }
    else 
    {
        char ip[4];
        char port[2];

        recv(fd,buf,4,0);
        Decrypt(buf,4);
        char addresstype = buf[3];
        if(addresstype == 0x01)  //ipv4
        {
            TraceLog("use ipv4");
            recv(fd,ip,4,0);
            Decrypt(ip,4);
            recv(fd,port,2,0);
            Decrypt(port,2);
        }
        else if(addresstype == 0x03) //domainname
        {
            //TraceLog("use domainname");
            char len = 0;
            //recv domainname
            recv(fd,&len,1,0);
            Decrypt(&len,1);
            recv(fd,buf,len,0);  // len 是char 还是int，在这里有问题******************************

            buf[len] = '\0'; 
            TraceLog("encry domainname:%s",buf);  //拿到域名之后，打印出来

            Decrypt(buf,len);
            // recv port 
            recv(fd,port,2,0);
            Decrypt(port,2);

            TraceLog("decrypt domainname:%s",buf);  //拿到域名之后，打印出来
           //因此我们拿到域名后，需要ip才能connect，因此我们使用gethostbyname 这个函数
           //这个函数就是DNS服务器(底层是UDP)来请求域名服务器，得到IP
            struct hostent* hostptr = gethostbyname(buf);
            memcpy(ip,hostptr->h_addr,hostptr->h_length); // 将DNS 服务器上的第一个对应的IP地址赋值给我的IP
            TraceLog("domainname, use DNS success get ip");
        }
        else if(addresstype == 0x04) //ipv6 暂不支持ipv6
        {
            ErrorLog("not support ipv6");
            return -1;
        }
        else 
        {
            ErrorLog("invalid address type");
            return -1;
        }
        struct sockaddr_in addr;
        memset(&addr,0,sizeof(struct sockaddr_in));
        addr.sin_family = AF_INET;
        memcpy(&addr.sin_addr.s_addr,ip,4);
        addr.sin_port = *((uint16_t*)port);

        int serverfd = socket(AF_INET,SOCK_STREAM,0);
        if(serverfd < 0)
        {
            ErrorLog("server socket");
            return -1;
        }
        if(connect(serverfd,(struct sockaddr*)&addr,sizeof(addr)) < 0)
        {
            ErrorLog("connect error");
            close(serverfd);
            return -1;
        }
        TraceLog("Establishment success");
        return serverfd;
    }
}



void Sock5Server::ReadEventHandle(int connectfd)
{
    TraceLog("read event:%d",connectfd);
    map<int,Connect*>::iterator it = _fdConnectMap.find(connectfd);
    if(it != _fdConnectMap.end())
    {
        Connect* con = it->second;
        //TraceLog("event state:%d",con->_state);
        if(con->_state == AUTH)  //身份认证
        {
            char replay[2];
            replay[0] = 0x05; //版本号
            int ret = AuthHandle(connectfd);
            if(ret == 0)  //数据没有到
            {
                return;
            }
            else if(ret == 1)
            {
                replay[1] = 0x00;
                con->_state = ESTABLISHMENT;
                //TraceLog("state mod");
            }
            else if(ret == -1)
            {
                replay[1] = 0xFF;
                RemoveConnect(connectfd);
            }

            Encry(replay,2);
            if(send(connectfd,replay,2,0) != 2)
            {
                ErrorLog("auth replay");
            }
        }
        else if(con->_state == ESTABLISHMENT)  // 已经建立连接
        {
            //TraceLog("Establishment");
            //回复
            char replay[10] = {0};
            replay[0] = 0x05;
            
            int serverfd = EstablishmentHandle(connectfd);
            if(serverfd == -1)
            {
                // ErrorLog("EstablishmentHandle");
                replay[1] = 0x01;
                RemoveConnect(connectfd);
                // return;
            }
            else if(serverfd == -2)
            {
                return;
            }
            else 
            {
                replay[1] = 0x00;
                replay[3] = 0x01;
            }

            Encry(replay,10);
            if(send(connectfd,replay,10,0) != 10)
            {
                ErrorLog("Establishment replay");
            }
            
            if(serverfd >= 0)
            {
                SetNonblocking(serverfd);
                OPEvent(serverfd,EPOLLIN,EPOLL_CTL_ADD);
            
                con->_serverChannel._fd = serverfd;
                _fdConnectMap[serverfd] = con;
                con->_ref++;
                con->_state = FORWARDING;
            }
        }
        else if(con->_state == FORWARDING) // 转发
        {
            //TraceLog("Forwarding");
            Channel* clientChanne = &con->_clientChannel;
            Channel* serverChannel = &con->_serverChannel;

            bool sendencry = false,recvdecrypt = true; 

            if(connectfd == serverChannel->_fd)
            {
                swap(clientChanne,serverChannel);
                swap(sendencry,recvdecrypt);
            }
            // client -> server 
            Forwarding(clientChanne,serverChannel,sendencry,recvdecrypt);

        }
        else 
        {
            assert(false);
        }
    }
    else 
    {
        assert(false);
    }
}



int main()
{
    Sock5Server server(8001);
    server.Start(); 
    return 0;
}



