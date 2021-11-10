#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
class http_conn
{
public:
    //文件名的最大长度
    static const int FILENAME_LEN = 200;
    //读缓冲区大小 
    static const int READ_BUFFER_SIZE = 2048;
    //写缓冲区大小
    static const int WRITE_BUFFER_SIZE = 1024;
    //HTTP请求方法，目前仅支持GET和POST
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    //解析客户请求时，主状态机所处的状态
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };

    //服务器处理HTTP请求的可能结果
    enum HTTP_CODE
    {
        //请求不完整，需要继续读取请求报文数据
        NO_REQUEST,
        //获得了完整的ＨＴＴＰ请求
        GET_REQUEST,
        //HTTP请求报文有语法错误
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        //服务器内部错误
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };

    //行的读取状态
    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

public:
    http_conn() {}
    ~http_conn() {}

public:
    //初始化新接受的连接
    void init(int sockfd, const sockaddr_in &addr);
    //关闭连接
    void close_conn(bool real_close = true);
    //处理客户请求
    void process();
    //读取浏览器端发来的全部数据
    bool read_once();
    //相应报文写入函数
    bool write();
    //获取socket地址
    sockaddr_in *get_address()
    {
        return &m_address;
    }
    //同步线程初始化数据库读取表
    void initmysql_result(connection_pool *connPool);

private:
    //初始化连接
    void init();
    //从m_write_buf读取，并处理请求报文
    HTTP_CODE process_read();
    //向m_write_buf写入响应报文数据
    bool process_write(HTTP_CODE ret);

    //主状态机解析报文中的请求行数据
    HTTP_CODE parse_request_line(char *text);
    //主状态机解析报文中的请求头数据
    HTTP_CODE parse_headers(char *text);
    //主状态机解析报文中的请求内容
    HTTP_CODE parse_content(char *text);
    //生成相应报文
    HTTP_CODE do_request();

    //m_start_line是已经解析的字符
    //get_line用于将指针向后偏移，指向未处理的字符
    char *get_line() { return m_read_buf + m_start_line; };

    //从状态机读取一行，分析是请求报文的哪一部分
    LINE_STATUS parse_line();

    void unmap();
    //根据响应报文格式，生成对应８个部分，以下函数均由do_request调用
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    //所有socket事件都被注册到同一个epoll内核事件表中，所以将epoll文件描述符设置为静态的
    static int m_epollfd;
    //统计用户数量
    static int m_user_count;
    MYSQL *mysql;

private:
    //该HTTP连接的socket
    int m_sockfd;
    //对方的socket地址
    sockaddr_in m_address;
    
    //读缓冲区
    char m_read_buf[READ_BUFFER_SIZE];
    //标记读缓冲区m_read_buf中已经读入的客户数据最后一个字节的下一个位置
    int m_read_idx;
    //当前正在分析的字符在读缓冲区中的位置
    int m_checked_idx;
    //m_read_buf中已经解析的字符个数
    int m_start_line;

    //写缓冲区
    char m_write_buf[WRITE_BUFFER_SIZE];
    //写缓冲区中待发送的字节数
    int m_write_idx;

    //主状态机当前所处的状态
    CHECK_STATE m_check_state;
    //请求方法
    METHOD m_method;

    //客户请求的目标文件的完整路径
    char m_real_file[FILENAME_LEN];
    //客户请求的目标文件的文件名
    char *m_url;
    //HTTP协议版本号，仅支持1.1
    char *m_version;
    //主机名
    char *m_host;
    //HTTP请求的消息体长度
    int m_content_length;
    //HTTP请求是否要求保持连接
    bool m_linger;

    //客户请求的目标文件被mmap映射到内存中的起始位置
    char *m_file_address;
    //目标文件的状态
    struct stat m_file_stat;
    //采用writev执行写操作
    struct iovec m_iv[2];
    //写内存块的数量
    int m_iv_count;

    //是否启用的POST
    int cgi;   
    //存储请求头数据     
    char *m_string; 
    //剩余发送字节数
    int bytes_to_send;
    //已经发送字节数
    int bytes_have_send;
};

#endif
