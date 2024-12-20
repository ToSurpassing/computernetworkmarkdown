#include <iostream>
#include <WINSOCK2.h>
#include <time.h>
#include <fstream>
#include <iomanip>
#pragma comment(lib, "ws2_32.lib")
#pragma warning(disable : 4996)
using namespace std;

const int MAXSIZE = 1024; // 传输缓冲区最大长度
double MAX_TIME = 0.5 * CLOCKS_PER_SEC;//设置最大重传时间间隔
int CWND = 1; // 窗口初始大小

const unsigned char SYN = 0x1;// 001—— FIN = 0 ACK = 0 SYN = 1
const unsigned char ACK = 0x2;// 010—— FIN = 0 ACK = 1 SYN = 0
const unsigned char ACK_SYN = 0x3;// 011—— FIN = 0 ACK = 1 SYN = 1
const unsigned char FIN = 0x4;// 100—— FIN = 1 ACK = 0 SYN = 0
const unsigned char FIN_ACK = 0x5;// 101—— FIN = 1 ACK = 0 SYN = 1
const unsigned char OVER = 0x7;// 111—— FIN = 1 ACK = 1 SYN = 1代表结束:UDP 无连接发送端\接收端需要知道文件传输是否完成

u_short checkSum(u_short* mes, int size)
{
    int count = (size + 1) / 2;
    // buffer相当于一个元素为u_short类型的数组，每个元素16位，相当于求校验和过程中的一个元素
    u_short* buf = (u_short*)malloc(size + 1);
    memset(buf, 0, size + 1);
    memcpy(buf, mes, size); // 将message读入buf
    u_long sum = 0;
    while (count--)
    {
        sum += *buf++;
        // 如果有进位则将进位加到最低位
        if (sum & 0xffff0000)
        {
            sum &= 0xffff;
            sum++;
        }
    }
    // 取反
    return ~(sum & 0xffff);
}

struct HEADER
{
    u_short sum = 0;//16位校验和
    u_short datasize = 0; //16位 所包含数据长度
    unsigned char flag = 0;// 8位，使用后3位，FIN ACK SYN
    unsigned char SEQ = 0; // 8位 传输序列号0~255


    HEADER()
    {
        sum = 0;      // 校验和
        datasize = 0; // 所包含数据长度
        flag = 0;     // 标志位
        SEQ = 0;      // 传输序列号
    }
};

int Connect(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen) // 三次握手建立连接
{
    HEADER header;
    char* Buffer = new char[sizeof(header)];

    // 第一次握手
    header.flag = SYN;
    header.sum = 0; // 校验和置0
    // 计算校验和
    header.sum = checkSum((u_short*)&header, sizeof(header));
    // 将数据头放入buffer
    memcpy(Buffer, &header, sizeof(header));
    if (sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen) == -1)
    {
        return -1;
    }
    else
    {
        cout << "成功发送第一次握手数据" << endl;
    }
    clock_t start = clock(); // 记录发送第一次握手时间

    // 为了函数能够继续运行，设置socket为非阻塞状态
    u_long mode = 1;
    ioctlsocket(socketClient, FIONBIO, &mode);

    // 第二次握手
    while (recvfrom(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrlen) <= 0)
    {
        // 超时重传
        if (clock() - start > MAX_TIME) // 超时重传第一次握手
        {
            cout << "第一次握手超时" << endl;
            header.flag = SYN;
            header.sum = 0;                                            // 校验和置0
            header.sum = checkSum((u_short*)&header, sizeof(header)); // 计算校验和
            memcpy(Buffer, &header, sizeof(header));                   // 将数据头放入Buffer
            sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
            start = clock();
            cout << "第一次握手重传" << endl;
        }
    }

    // 第二次握手，收到来自接收端的ACK
    // 进行校验和检验
    memcpy(&header, Buffer, sizeof(header));
    if (header.flag == ACK && checkSum((u_short*)&header, sizeof(header)) == 0)
    {
        cout << "接收到第二次握手数据" << endl;
    }
    else
    {
        cout << "第二次握手数据错误，请重试" << endl;
        return -1;
    }

    // 进行第三次握手
    header.flag = ACK_SYN;
    header.sum = 0;
    header.sum = checkSum((u_short*)&header, sizeof(header)); // 计算校验和
    if (sendto(socketClient, (char*)&header, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen) == -1)
    {
        return -1;
    }
    else
    {
        cout << "成功发送第三次握手数据" << endl;
    }
    cout << "--服务器连接成功--" << endl;
    return 1;
}

void send_package(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, char* message, int len, int order)
{
    HEADER header;
    char* buffer = new char[MAXSIZE + sizeof(header)];
    header.datasize = len;
    header.SEQ = unsigned char(order); // 序列号
    memcpy(buffer, &header, sizeof(header));
    memcpy(buffer + sizeof(header), message, sizeof(header) + len);
    u_short check = checkSum((u_short*)buffer, sizeof(header) + len); // 计算校验和：头部+数据
    header.sum = check;
    memcpy(buffer, &header, sizeof(header));                                                   // 计算完校验和头部需要进行刷新
    sendto(socketClient, buffer, len + sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen); // 发送
    cout << "[\033[1;31mSend\033[0m] 发送了" << len << " 字节，"
        << " flag:" << int(header.flag) << " SEQ:" << int(header.SEQ) << " SUM:" << int(header.sum) << endl;
}

void send(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, char* message, int len) {
    HEADER header;
    char* Buffer = new char[sizeof(header)];
    int packagenum = len / MAXSIZE + (len % MAXSIZE != 0); // 计算总包数
    int head = -1;  // 缓冲区头部:指向最后一个已被接收端确认的序列号
    int tail = 0;   // 缓冲区尾部:指向最后一个未确认的序列号
    clock_t start;

    // RENO的拥塞控制变量
    int state = 0;       // 初始状态：SS (慢启动)
    int cwnd = 1;        // 初始拥塞窗口
    int threshold = 32;  // 慢启动阈值
    int dup_acks = 0;    // 重复ACK计数器

    while (head < packagenum - 1) {
        // 填充并发送数据包，直到滑动窗口满或所有数据包已发送完毕
        while (tail - head <= cwnd && tail != packagenum) {
            send_package(socketClient, servAddr, servAddrlen, message + tail * MAXSIZE,
                (tail == packagenum - 1 ? len - (packagenum - 1) * MAXSIZE : MAXSIZE), tail % 256);
            start = clock();
            tail++;
        }

        // 设置非阻塞模式
        u_long mode = 1;
        ioctlsocket(socketClient, FIONBIO, &mode);

        if (recvfrom(socketClient, Buffer, MAXSIZE, 0, (sockaddr*)&servAddr, &servAddrlen) > 0) {//能收到接收端的反馈
            memcpy(&header, Buffer, sizeof(header));
            u_short check = checkSum((u_short*)&header, sizeof(header));
            if (int(check) != 0) {
                tail = head + 1; // 校验和错误，回退
                cout << "[\033[1;33mInfo\033[0m] 错误的包，等待重传" << endl;
                continue;
            }
            else 
            {
                // 收到ACK的处理逻辑
                if (int(header.SEQ) >= head % 256)
                {
                    // 收到新的ACK
                    if (int(header.SEQ) == head % 256) 
                    {
                        dup_acks++;  // 重复ACK计数
                    }
                    else 
                    {
                        dup_acks = 0;  // 重置重复ACK计数
                    }
                    head = head + int(header.SEQ) - head % 256;  // 更新head

                    cout << "[\033[1;34mReceive\033[0m] 收到了ACK：Flag:" << int(header.flag)
                        << " SEQ:" << int(header.SEQ) << endl;

                    if (state == 0) 
                    {  // SS状态：慢启动
                        cwnd++;  // cwnd每收到一个ACK加1
                        if (cwnd > threshold) {
                            state = 2;  // 如果cwnd > threshold，进入CA状态
                        }
                    }
                    else if (state == 1) 
                    {//快速恢复QR状态：进入CA
                        cwnd = threshold;
                        dup_acks = 0;
                        state = 2;
                    }
                    else if (state == 2)
                    {  // CA状态：拥塞避免
                        cwnd += 1.0 / cwnd;  // cwnd按照1/cwnd线性增长，逐步增大
                        cwnd = floor(cwnd);  // 使用floor函数确保cwnd为整数
                        if (cwnd <= 0) {
                            cwnd = 1;
                        }
                    }

                    // 处理快速重传的逻辑
                    if (dup_acks == 3) 
                    {  // 收到3个重复ACK，认为发生了丢包
                        cout << "[\033[1;33mInfo\033[0m] 从第: " << head << "号数据包开始超时重传"<<endl;
                        send_package(socketClient, servAddr, servAddrlen, message + head * MAXSIZE,
                            MAXSIZE, head % 256);
                        threshold = cwnd / 2;
                        cwnd = threshold + 3;  // 设置cwnd为ssthresh + 3
                        state = 1;  // 进入QR状态
                    }
                }

                // 收到旧ACK：非QR状态等待三次重复ACK
                if (dup_acks >= 3 && state != 1) 
                {
                    state = 1;  // 进入QR状态
                    threshold = cwnd / 2;
                    cwnd = threshold + 3;  // 设置cwnd为ssthresh + 3
                }

                // QR状态处理
                if (state == 1) 
                {
                    cwnd++;  // QR状态下每次收到ACK，cwnd增加1
                }
                // 如果接收到的ACK序列号小于当前窗口大小（cwnd），但跨越了序列号环，需要更新缓冲区头部
                else if (head % 256 > 256 - cwnd - 1 && int(header.SEQ) < cwnd) {
                    // 计算跨越环后更新head的值
                    head = head + 256 - head % 256 + int(header.SEQ);

                    cout << "[\033[1;34mReceive\033[0m] 收到了ACK（跨seq）：Flag:" << int(header.flag)
                        << " SEQ:" << int(header.SEQ) << endl;

                    // 进入正常的状态更新处理
                    if (state == 0) {  // SS状态：慢启动
                        cwnd++;  // cwnd每收到一个ACK加1
                        if (cwnd > threshold) {
                            state = 2;  // 如果cwnd > threshold，进入CA状态
                        }
                    }
                    else if (state == 1) {  // QR状态：进入CA
                        cwnd = threshold;
                        state = 2;
                    }
                    else if (state == 2) {  // CA状态：拥塞避免
                        cwnd += 1.0 / cwnd;  // cwnd按照1/cwnd线性增长，逐步增大
                        cwnd = floor(cwnd);  // 使用floor函数确保cwnd为整数
                        if (cwnd <= 0) {
                            cwnd = 1;
                        }
                    }

                    // 更新tail的位置，如果tail == head，表示滑动窗口已经没有未确认的包
                    if (tail == head) {
                        tail = head + 1;
                    }
                }
                cout << "[\033[1;33mInfo\033[0m] head:" << head % 256 << ' ' << "tail:" << tail % 256;
                cout << " dup_acks:" << dup_acks << "     目前的窗口大小为：" << cwnd << endl;
            }
        }
        else 
        {
            // 超时处理：回退窗口并重新发送
            if (clock() - start > MAX_TIME)
            {
                tail = head + 1;
                threshold = cwnd / 2;  // 设置threshold为cwnd / 2
                cwnd = 1;              // cwnd回退至1
                state = 0;             // 进入慢启动状态（SS）
                cout << "[\033[1;33mInfo\033[0m] 超时，重新进入慢启动状态，准备重传" << endl;
                cout << "[\033[1;33mInfo\033[0m] head:" << head % 256 << ' ' << "tail:" << tail % 256;
                cout << " dup_acks:" << dup_acks << "     目前的窗口大小为：" << cwnd << endl;
            }
        }
        mode = 0;
        ioctlsocket(socketClient, FIONBIO, &mode);  // 恢复阻塞模式
    }

    // 发送结束信号
    header.flag = OVER;
    header.sum = 0;
    u_short temp = checkSum((u_short*)&header, sizeof(header));
    header.sum = temp;
    memcpy(Buffer, &header, sizeof(header));
    sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
    cout << "[\033[1;31mSend\033[0m] 发送OVER信号" << endl;
    start = clock();
    while (1) {
        u_long mode = 1;
        ioctlsocket(socketClient, FIONBIO, &mode);
        while (recvfrom(socketClient, Buffer, MAXSIZE, 0, (sockaddr*)&servAddr, &servAddrlen) <= 0) {
            if (clock() - start > MAX_TIME) {
                char* Buffer = new char[sizeof(header)];
                header.flag = OVER;
                header.sum = 0;
                u_short temp = checkSum((u_short*)&header, sizeof(header));
                header.sum = temp;
                memcpy(Buffer, &header, sizeof(header));
                sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
                cout << "[\033[1;33mInfo\033[0m] OVER消息发送超时，已经重传" << endl;
                start = clock();
            }
        }
        memcpy(&header, Buffer, sizeof(header));
        u_short check = checkSum((u_short*)&header, sizeof(header));
        if (header.flag == OVER) {
            cout << "[\033[1;33mInfo\033[0m] 对方已成功接收文件" << endl;
            break;
        }
    }
    u_long mode = 0;
    ioctlsocket(socketClient, FIONBIO, &mode); // 恢复为阻塞模式
}


int disConnect(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen)
{
    HEADER header;
    char* Buffer = new char[sizeof(header)];

    // 进行第一次挥手
    header.flag = FIN;
    header.sum = 0;                                            // 校验和置0
    header.sum = checkSum((u_short*)&header, sizeof(header)); // 计算校验和
    memcpy(Buffer, &header, sizeof(header));                   // 将首部放入缓冲区
    if (sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen) == -1)
    {
        return -1;
    }
    else
    {
        cout << "成功发送第一次挥手数据" << endl;
    }
    clock_t start = clock(); // 记录发送第一次挥手时间

    u_long mode = 1;
    ioctlsocket(socketClient, FIONBIO, &mode); // FIONBIO为命令，允许1/禁止0套接口s的非阻塞1/阻塞0模式。

    // 接收第二次挥手
    while (recvfrom(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrlen) <= 0)
    {
        // 超时，重新传输第一次挥手
        if (clock() - start > MAX_TIME)
        {
            cout << "第一次挥手超时" << endl;
            header.flag = FIN;
            header.sum = 0;                                            // 校验和置0
            header.sum = checkSum((u_short*)&header, sizeof(header)); // 计算校验和
            memcpy(Buffer, &header, sizeof(header));                   // 将首部放入缓冲区
            sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
            start = clock();
            cout << "已重传第一次挥手数据" << endl;
        }
    }

    // 进行校验和检验
    memcpy(&header, Buffer, sizeof(header));
    if (header.flag == ACK && checkSum((u_short*)&header, sizeof(header) == 0))
    {
        cout << "成功接收到第二次挥手数据" << endl;
    }
    else
    {
        cout << "第二次握手数据错误，请重试" << endl;
        return -1;
    }

    // 进行第三次挥手
    header.flag = FIN_ACK;
    header.sum = 0;
    header.sum = checkSum((u_short*)&header, sizeof(header)); // 计算校验和
    if (sendto(socketClient, (char*)&header, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen) == -1)
    {
        return -1;
    }
    else
    {
        cout << "成功发送第三次挥手数据" << endl;
    }
    start = clock();
    // 接收第四次挥手
    while (recvfrom(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrlen) <= 0)
    {
        if (clock() - start > MAX_TIME) // 超时，重新传输第三次挥手
        {
            cout << "第三次挥手超时" << endl;
            header.flag = FIN;
            header.sum = 0;                                            // 校验和置0
            header.sum = checkSum((u_short*)&header, sizeof(header)); // 计算校验和
            memcpy(Buffer, &header, sizeof(header));                   // 将首部放入缓冲区
            sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
            start = clock();
            cout << "已重传第三次挥手数据" << endl;
        }
    }
    cout << "成功接收到第四次挥手数据" << endl << "--已经成功断开发送端和接收端的连接--" << endl;
    return 1;
}

int main()
{
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);

    SOCKADDR_IN serverAddr;
    SOCKET server;

    serverAddr.sin_family = AF_INET; // 使用IPV4
    serverAddr.sin_port = htons(8080);
    serverAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

    server = socket(AF_INET, SOCK_DGRAM, 0);
    int len = sizeof(serverAddr);

    // 建立连接
    if (Connect(server, serverAddr, len) == -1)
    {
        return 0;
    }

   // cout << endl
    //    << "输入设置的滑动窗口最大值：";
    //cin >> cwnd;
    //cout << "当前MAX（滑动窗口大小）为 " << cwnd << endl;

    bool flag = true;
    while (true)
    {
        cout << endl
            << "请选择接下来要进行的操作：" << endl
            << "输入“0”以退出程序" << endl;
        if (flag)
        {
            cout << "输入“1”以进行文件传输" << endl;
            flag = !flag;
        }
        else
        {
            cout << "输入“1”以进行文件传输" << endl;
        }

        int choice = {};

        cin >> choice;
        cout << endl;
        if (choice == 0)
            break;
        else
        {

            // 读取文件内容到buffer
            string inputFile; // 希望传输的文件名称
            cout << "请输入要传输的文件名称：" << endl;
            cin >> inputFile;
            ifstream fileIN(inputFile.c_str(), ifstream::binary); // 以二进制方式打开文件
            char* buffer = new char[100000000];                   // 文件内容
            int i = 0;
            unsigned char temp = fileIN.get();
            while (fileIN)
            {
                buffer[i++] = temp;
                temp = fileIN.get();
            }
            fileIN.close();

            // 发送文件名
            send(server, serverAddr, len, (char*)(inputFile.c_str()), inputFile.length());
            clock_t start1 = clock();
            // 发送文件内容（在buffer里）
            send(server, serverAddr, len, buffer, i);
            clock_t end1 = clock();
            cout << "------------------------------------------------" << endl;
            cout << std::fixed << std::setprecision(2);
            cout << "传输总时间为:" << static_cast<double>(end1 - start1) / CLOCKS_PER_SEC << "s" << endl;
            cout << "吞吐率为:" << std::fixed << std::setprecision(2)
                << (((double)i) / ((end1 - start1) / CLOCKS_PER_SEC)) << "byte/s" << endl;
        }
    }

    disConnect(server, serverAddr, len);
    system("pause");
    return 0;
}
