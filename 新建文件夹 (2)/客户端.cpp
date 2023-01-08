#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#include <iostream>
#include <fstream>
#include <winsock2.h>
#include <time.h>
#include <Winsock2.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <windows.h>
#include <ctime>
#include <io.h>
#include <cmath>

#define BUFFER_SIZE 0xf000
#define HEAD_SIZE 0x14
#define DATA_SIZE (BUFFER_SIZE-HEAD_SIZE)

#pragma comment(lib, "ws2_32.lib")
using namespace std;

ofstream ccout;//ccout;

char sendBuffer[BUFFER_SIZE];
char recvBuffer[BUFFER_SIZE];

int sourcePort = 1234;
int destinationPort = 1235;
unsigned int sequenceNumber = 0;

string fileName;   
int fileLength;  

bool isFirstPackage = true;  

long bytesHaveSent = 0;  
int bytesHaveWritten = 0; 
int t_start;    

ifstream fin;   
int bytesHaveRead = 0; 
int leftDataSize;  

int nowTime = 0;    
int sendTimes;  

void setPort();
void setSeqNum(unsigned int num);
void setAckNum(unsigned int num);
void setSize(int num);
void setAckBit(char a);
void setSynBit(char a);
void setFinBit(char a);
unsigned short calCheckSum(unsigned short* buf);// ����У���
void setCheckSum();
void setBufferSize(unsigned int num);

// ���������׽���
void makeSocket();

void setRTO();
void findFile();


class Getter {
public:
    unsigned int getSeqNum(char* recvBuffer) {
        unsigned int a = ((recvBuffer[7] & 0xff) << 24) + ((recvBuffer[6] & 0xff) << 16) + ((recvBuffer[5] & 0xff) << 8) + (recvBuffer[4] & 0xff);
        return a & 0xffff;
    }
    unsigned int getAckNum(char* recvBuffer) {
        unsigned int a = ((recvBuffer[11] & 0xff) << 24) + ((recvBuffer[10] & 0xff) << 16) + ((recvBuffer[9] & 0xff) << 8) + (recvBuffer[8] & 0xff);
        return a & 0xffff;
    }
    int getSize(char* recvBuffer) {
        int a = recvBuffer[12];
        return a;
    }
    int getAckBit(char* recvBuffer) {
        // 0001 0000
        int a = (recvBuffer[13] & 0x10) >> 4;
        return a;
    }
    int getSynBit(char* recvBuffer) {
        // 0000 0010
        int a = (recvBuffer[13] & 0x2) >> 1;
        return a;
    }
    int getFinBit(char* recvBuffer) {
        // 0000 0010
        int a = (recvBuffer[13] & 0x1);
        return a;
    }
    unsigned short getCheckSum(char* recvBuffer) {
        int a = (recvBuffer[15] << 8) + recvBuffer[14];
        return a;
    }
    unsigned int getBufferSize(char* recvBuffer) {
        unsigned int a = ((recvBuffer[19] & 0xff) << 24) + ((recvBuffer[18] & 0xff) << 16) + ((recvBuffer[17] & 0xff) << 8) + (recvBuffer[16] & 0xff);
        return a & 0xffff;
    }
} getter;


void packSynDatagram(int sequenceNumber);
void packFirst();
void packData();

void printLogSendBuffer();
void printLogRecvBuffer();

bool checkSumIsRight();

void getFileName();

void printFileErr();
void printRTOErr();


// һ�����Ӷ�Ӧ��һ�����кź�һ��״̬
class SendGrid {
public:
    /**
     * state==0���������û��ʹ��
     * state==1��������ڱ�ռ���ˣ��Ѿ������ˣ���û�յ�ack���յ�ack��תΪ2��
     *           ��ʱ��û���յ�ack���ش������ת2���Ǹ������������ģ��ƶ��������ڡ�
     * state==2�����˶����յ�ack�ˣ�ֻҪ���Ķ����˾Ϳ����ύ����������ݡ�
     */
    int state;  //�����жϸ��ӵ�״̬
    int seq;    //���ӵ����к���
    char buffer[BUFFER_SIZE];   //�����кŵ�sendbuffer
    clock_t start;//�����������ȥ��ʱ��
    SendGrid() :state(0), seq(-1) {};
    void setBuffer(char* sendBuffer) {
        for (int i = 0;i < BUFFER_SIZE;i++) {
            buffer[i] = sendBuffer[i];
        }
    }
};
class SendWindow {
public:
    SendGrid sendGrid[16];
    // ���������ƶ�1λ
    void move() {//���õݹ�ķ�ʽ���ƶ�����
        if (sendGrid[0].state == 2) {//���������ǲ����Ѿ�ack��
            //ccout << "��ʱ���ƶ������ˣ�" << endl;
            nowTime = sendGrid[0].seq + 1;
            cout << nowTime << endl;
            for (int i = 1;i < 16;i++) {
                sendGrid[i - 1].state = sendGrid[i].state;
                sendGrid[i - 1].seq = sendGrid[i].seq;
                for (int j = 0;j < BUFFER_SIZE;j++) {
                    sendGrid[i - 1].buffer[j] = sendGrid[i].buffer[j];
                }
            }
            sendGrid[15].state = 0;//���ұߵĸ������¿���
            sendGrid[15].seq = sendGrid[14].seq + 1;
            //ccout << "���ƶ��˴��ڣ�" << endl;
            printWindow();
            cout << "�����Ƿ��͵ĵ�" << nowTime << "/" << sendTimes << "������" << endl;
            if (nowTime == sendTimes||nowTime==sendTimes-1) {
                cout << "������������" << endl;
                int t = clock() - t_start;
                cout << "���͵��ֽ�����" << bytesHaveSent << ".\n";
                cout << "ʹ��ʱ�䣺 " << t << "ms.\n";

                //cout << "�����ʣ�" << bytesHaveSent * 8 / (t - t_start) * CLOCKS_PER_SEC << "bps\n";

                exit(0);
            }
            // printWindow();
            if (sendGrid[0].state == 2)//�������໹���Ѿ�ack�ˣ�����move
                this->move();
        }
    }
    // ����debug
    void printWindow() {
        for (int i = 0;i < 16;i++) {
            //ccout << "number " << i << " state: " << sendGrid[i].state << ", seq: " << sendGrid[i].seq << endl;
        }
        //ccout << endl;
    }
} win;


SOCKET sockSrv;
SOCKADDR_IN  addrServer;
SOCKADDR_IN  addrClient;
int len;

struct timeval timeout; //��ʱ

HANDLE hMutex = NULL;//�����������ڶ��߳�



// ���ͱ��ĵ�ִ��
void sendData(int i);
void resendData(int i);

// ���߳�֮���նԷ���ack
DWORD WINAPI ackReader(LPVOID lpParamter);

// ���ͱ��ĵ��߼�
void sendFileDatagram();
void sendSynDatagram();





int main() {
    //ccout.open("client.txt");

    WSADATA wsaData;
    WSAStartup(MAKEWORD(1, 1), &wsaData);

    makeSocket();
    setRTO();

    sendSynDatagram();
    sendFileDatagram();


    closesocket(sockSrv);
    WSACleanup();
    return 0;
}

void sendData(int i) {
    if (sequenceNumber == sendTimes) {
        return;
    }
    //ccout << "��Ҫ����win" << i << "��seqΪ" << sequenceNumber << endl;
    cout << "\n��Ҫ����win" << i << "��seqΪ" << sequenceNumber << endl;

    win.sendGrid[i].seq = sequenceNumber;
    if (sequenceNumber >= sendTimes) {
        return;
    }
    win.sendGrid[i].state = 1;

    // win.printWindow();
    leftDataSize = DATA_SIZE;
    if (isFirstPackage) {
        // �ǵ�һ����
        packFirst();//����Size
        sequenceNumber++;
        // ���ݶε�ǰlength��λ�÷��ļ���
        for (int j = 0;j < fileName.length();j++) {
            sendBuffer[HEAD_SIZE + j] = fileName[j];
        }
        leftDataSize -= fileName.length();      //��ΪҪ���ļ�������ʣ���ܴ����ݵĿռ��С�ˡ�
        bytesHaveWritten += fileName.length();  //���ļ���Ҳ�Ǵ�
        isFirstPackage = false;
        // �������к�packData.
    }
    else {
        packData();//û��Size����ͨͷ
        sequenceNumber++;

    }
    cout << "bytesHaveRead: " << bytesHaveRead << endl;
    cout << "���İ��������: " << win.sendGrid[i].seq << endl;
    fin.seekg(bytesHaveRead, fin.beg);
    int sendSize = min(leftDataSize, fileLength - bytesHaveRead); //��������һ�����Ļ����ܻ᲻����
    cout << "leftDataSize: " << leftDataSize << endl;
    cout << "fileLength-bytesHaveRead: " << fileLength - bytesHaveRead << endl;
    cout << "sendSize: " << sendSize << endl;

    fin.read(&sendBuffer[HEAD_SIZE + (DATA_SIZE - leftDataSize)], sendSize);// sendBuffer��ʲô�ط���ʼ���𣬶�����
    bytesHaveRead += sendSize;
    bytesHaveWritten += sendSize;

    setBufferSize(sendSize);
    setCheckSum();
    cout << "setBufferSize" << getter.getBufferSize(sendBuffer) << endl;
    if (win.sendGrid[i].seq == sendTimes - 1) {
        setFinBit(1);
        setCheckSum();
    }
    sendto(sockSrv, sendBuffer, sizeof(sendBuffer), 0, (sockaddr*)&addrServer, len);
    //ccout << "������" << sequenceNumber - 1 << endl;

    //���浽�����Դ���Buffer���档
    for (int j = 0;j < BUFFER_SIZE;j++) {
        win.sendGrid[i].buffer[j] = sendBuffer[j];
    }

    bytesHaveSent += getter.getBufferSize(win.sendGrid[i].buffer);
    cout << "bytesHaveSent: " << bytesHaveSent << endl;
    // printLogSendBuffer();
    //ccout << "sent." << endl;
    memset(sendBuffer, 0, sizeof(sendBuffer));
    bytesHaveWritten = 0;
}
void resendData(int i) {

    for (int j = 0;j < BUFFER_SIZE;j++) {
        sendBuffer[j] = win.sendGrid[i].buffer[j];
    }
    sendto(sockSrv, sendBuffer, sizeof(sendBuffer), 0, (sockaddr*)&addrServer, len);
    cout << "���·���" << i << "�� seq: " << win.sendGrid[i].seq << endl;
    printLogSendBuffer();
    bytesHaveSent += getter.getBufferSize(win.sendGrid[i].buffer);
    cout << "bytesHaveSent:" << bytesHaveSent << endl;
    //ccout << sequenceNumber << " resent." << endl;
}

DWORD WINAPI ackReader(LPVOID lpParamter) {

    while (1) {
        // �յ���Ϣ
        int it = recvfrom(sockSrv, recvBuffer, sizeof(recvBuffer), 0, (SOCKADDR*)&addrServer, &len);

        if (!checkSumIsRight()) {
            //ccout << "CheckSum is wrong!" << endl;
            continue;
        }
        if (getter.getAckBit(recvBuffer) == false) {
            //ccout << "not an ack datagram!" << endl;
            continue;
        }

        WaitForSingleObject(hMutex, INFINITE);
        //ccout << "===============" << endl;
        //ccout << "ackreader�õ�����" << endl;
        //ccout << "===============" << endl;
        printLogRecvBuffer();

        int i = 0;
        for (;i < 16;i++) {
            // ���ƥ������
            if (getter.getAckNum(recvBuffer) == win.sendGrid[i].seq) {
                //ccout << "matched! window: " << i;
                //ccout << "seq: " << getter.getAckNum(recvBuffer) << endl;
                break;
            }
        }
        // ˵��һ����ûƥ����
        if (i == 16) {
            //ccout << "һ����ûƥ����" << endl;
            ReleaseMutex(hMutex);
            continue;
        }

        for(int k=0;k<=i;k++)
            win.sendGrid[k].state = 2;//��������ӵ�״̬λ��2
        win.printWindow();

        // ÿ���յ�һ�������ܲ���Move��
        win.move();

        if (nowTime == sendTimes) {
            ReleaseMutex(hMutex);
            cout << "�����ǵ�" << nowTime << "�Ρ�\n";
            cout << "���˳���" << endl;
            return 0L;//��ʾ���ص���long�͵�0

        }
        //ccout << "===============" << endl;
        //ccout << "ackreader�ͷ�����" << endl;
        //ccout << "===============" << endl;
        ReleaseMutex(hMutex);

    }
}

void sendFileDatagram() {
    getFileName();

    // �����ļ��������ļ��������ļ����Ⱥ�sendTimes
    findFile();

    // ����ͳ��
    t_start = clock();

    // ���ڶ��߳�
    HANDLE hThread = CreateThread(NULL, 0, ackReader, NULL, 0, NULL);
    hMutex = CreateMutex(NULL, FALSE, L"screen");

    CloseHandle(hThread);//�ر��̵߳ľ���������̻߳�������ܡ�

    while (1) {

        // �õ�����������һ��ѭ����

        WaitForSingleObject(hMutex, INFINITE);
        //ccout << "===============" << endl;
        //ccout << "���ļ��õ�����" << endl;
        //ccout << "===============" << endl;

        if (win.sendGrid[0].seq == sendTimes - 1) {
            //ccout << "===============" << endl;
            //ccout << "�������һ����" << endl;
            //ccout << "===============" << endl;
            ReleaseMutex(hMutex);
            break;
        }
        for (int i = 0;i < 16;i++) {

            if (win.sendGrid[i].state == 2) {//����Ѿ���ack�˲�������
                //ccout << "����" << i << "�Ѿ���ack��" << endl;
                continue;
            }
            // ���timerû�����ϣ���ô��ʼ���ļ�
            if (win.sendGrid[i].state == 0) {
                //ccout << "����" << i << "��û����" << endl;

                win.sendGrid[i].start = clock();//��ȡ��ǰʱ��
                //ccout << "��Ҫ�ڴ���" << i << "������" << endl;
                sendData(i);//�ѷ������ݷ���win.sendGrid[i].buffer[]��
                // ͬʱ������printLog
            }
            // timer�Ѿ�����
            else {
                //ccout << "����" << i << "�Ѿ�������" << endl;

                int time = clock() - win.sendGrid[i].start;
                // ��û�г�ʱ�ز�����
                // clock�ǰ���CPU��ģ���������ĳ�ʱ��һ��
                if (time < 1 * CLOCKS_PER_SEC) {
                    //ccout << "û�г�ʱ\n";
                    continue;
                }
                // ��ʱ�ˣ��ش�
                else {
                    printRTOErr();
                    win.sendGrid[i].start = clock();//�������ö�ʱ��
                    resendData(i);//���°�grid[i].buffer��������ٷ�һ�飬����printLog
                }
            }

        }
        //ccout << "===============" << endl;
        //ccout << "���ļ��ͷ�����" << endl;
        //ccout << "===============" << endl;
        ReleaseMutex(hMutex);


    }
}

void sendSynDatagram() {
    // ����Ӧ���ò��Ż�������
    while (1) {
        packSynDatagram(0);
        sendto(sockSrv, sendBuffer, sizeof(sendBuffer), 0, (sockaddr*)&addrServer, len);
        printLogSendBuffer();
        //ccout << "sent." << endl;

        int it = recvfrom(sockSrv, recvBuffer, sizeof(recvBuffer), 0, (SOCKADDR*)&addrServer, &len);

        if (it <= 0) {
            if (WSAGetLastError() == 10060) {// ��ʱ����
                printRTOErr();
                continue;
            }
        }
        else {
            printLogRecvBuffer();
            // ���Ϸ���SYN��ֱ���յ�SYN+ACKΪֹ��
            if (getter.getAckBit(recvBuffer) && getter.getSynBit(recvBuffer) && checkSumIsRight()) {
                //ccout << "Got a SYN ACK!" << endl;
                break;
            }
        }
    }
}





//##################################### Utils #####################################//

void findFile() {
    fin.open(fileName, ifstream::in | ios::binary);
    fin.seekg(0, fin.end);              //��ָ�붨λ��ĩβ
    fileLength = fin.tellg();           //��ȡָ��
    fin.seekg(0, fin.beg);              //���ļ���ָ�����¶�λ�����Ŀ�ʼ

    if (fileLength <= 0) {
        printFileErr();
        return;
    }
    //ccout << "The size of this file is " << fileLength << " bytes.\n";
    cout << "The size of this file is " << fileLength << " bytes.\n";

    // //ccout<<"fileLength: "<<fileLength<<endl;
    // //ccout<<"fileName.length(): "<<fileName.length()<<endl;
    // //ccout<<"DATA_SIZE: "<<DATA_SIZE<<endl;
    sendTimes = ceil(((double)fileLength + (double)fileName.length()) / (double)DATA_SIZE);     //��Ҫ������ô���
    //ccout << "We will split this file to " << sendTimes << " packages and send it." << endl;
    cout << "We will split this file to " << sendTimes << " packages and send it." << endl;
}

void makeSocket() {
    //�������ڼ������׽���
    sockSrv = socket(AF_INET, SOCK_DGRAM, 0);
    // SOCKADDR_IN  addrServer;
    addrServer.sin_addr.s_addr = inet_addr("127.0.0.1");
    addrServer.sin_family = AF_INET;
    addrServer.sin_port = htons(1366);

    // SOCKADDR_IN  addrClient;
    len = sizeof(SOCKADDR);
}

void getFileName() {
    std::cout << "Tell me which file you want to send.\n";
    cin >> fileName;
}

void setRTO() {
    timeout.tv_sec = 2000;
    timeout.tv_usec = 0;

    setsockopt(sockSrv, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    //ccout << "We successfully set the RTO." << endl;
}
//##################################### Utils #####################################//


//##################################### Setters #####################################//

void setPort() {
    // ��0xff��Ϊ�˰Ѹ߹�8λ�Ķ���ա�
    sendBuffer[1] = (sourcePort >> 8) & 0xff;
    sendBuffer[0] = sourcePort & 0xff;
    sendBuffer[3] = (destinationPort >> 8) & 0xff;
    sendBuffer[2] = destinationPort & 0xff;
}

void setSeqNum(unsigned int num) {
    // ������кų�����Χ��ȡģ��
    if (num > 0xffffffff) {
        num %= 0xffffffff;
    }
    sendBuffer[7] = (num >> 24) & 0xff;
    sendBuffer[6] = (num >> 16) & 0xff;
    sendBuffer[5] = (num >> 8) & 0xff;
    sendBuffer[4] = num & 0xff;
}

void setAckNum(unsigned int num) {
    if (num > 0xffffffff) {
        num %= 0xffffffff;
    }
    sendBuffer[11] = (num >> 24) & 0xff;
    sendBuffer[10] = (num >> 16) & 0xff;
    sendBuffer[9] = (num >> 8) & 0xff;
    sendBuffer[8] = num & 0xff;
}

void setSize(int num) {
    // ����������ļ������ȣ����15������Ӧ���Ƿ�option���ȵġ�
    if (num >> 8) {
        //ccout << "+-------------------------------------------------+\n";
        //ccout << "| This fileName is too long! We cannot handle it! |\n";
        //ccout << "+-------------------------------------------------+\n";
    }
    sendBuffer[12] = (char)(num & 0xff);
}

void setAckBit(char a) {
    if (a == 0) {
        // 1110 1111
        sendBuffer[13] &= 0xef;
    }
    else {
        // 0001 0000
        sendBuffer[13] |= 0x10;
    }
}

void setSynBit(char a) {
    if (a == 0) {
        // 1111 1101
        sendBuffer[13] &= 0xfd;
    }
    else {
        // 0000 0010
        sendBuffer[13] |= 0x2;
    }
}

void setFinBit(char a) {
    if (a == 0) {
        // 1111 1110
        sendBuffer[13] &= 0xfe;
    }
    else {
        // 0000 0001
        sendBuffer[13] |= 0x01;
    }
}

void setBufferSize(unsigned int num) {
    cout << num << endl;
    if (num > 0xffffffff) {
        num %= 0xffffffff;
    }
    sendBuffer[19] = (num >> 24) & 0xff;
    sendBuffer[18] = (num >> 16) & 0xff;
    sendBuffer[17] = (num >> 8) & 0xff;
    sendBuffer[16] = num & 0xff;
    // getter.getBufferSize(sendBuffer);
}

//##################################### Setters #####################################//


//##################################### CheckSum #####################################//

unsigned short calCheckSum(unsigned short* buf) {
    int count = HEAD_SIZE / 2;
    register unsigned long sum = 0;

    while (count--) {
        sum += *buf;
        buf++;
        // ���У��Ͳ�ֹ16bit���Ѹ�λ��0�����λ��һ��
        if (sum & 0xffff0000) {
            sum &= 0xffff;
            sum++;
        }
    }
    return ~(sum & 0xffff); //ȡ��
}

void setCheckSum() {
    sendBuffer[14] = sendBuffer[15] = 0; // У�������
    unsigned short x = calCheckSum((unsigned short*)&sendBuffer[0]); //����У���
    sendBuffer[15] = (char)((x >> 8) % 0x100);
    sendBuffer[14] = (char)(x % 0x100);
}

bool checkSumIsRight() {
    unsigned short* buf = (unsigned short*)&recvBuffer[0];
    register unsigned long sum = 0;
    // ��ÿ���ֽڼ�һ��
    for (int i = 0;i < HEAD_SIZE / 2;i++) {
        sum += *buf;
        // //ccout<<*buf<<" ";
        buf++;
        // ���У��Ͳ�ֹ16bit���Ѹ�λ��0�����λ��һ��
        if (sum & 0xffff0000) {
            // //ccout<<"�Ѹ�λ����\n";
            sum &= 0xffff;
            sum++;
        }
    }

    // ȫ������Ӧ����0xffff

    // if(sum==0xffff){
    //     //ccout<<"CheckSum: "<<sum<<", ";
    //     //ccout<<"CheckSum right!"<<endl;
    // }
    // else{
    //     //ccout<<"CheckSum: "<<sum<<", ";
    //     //ccout<<"CheckSum wrong!"<<endl;
    // }
    return sum == 0xffff;
}

//##################################### CheckSum #####################################//

//##################################### DataPack #####################################//

void packSynDatagram(int sequenceNumber) {
    setPort();
    setSeqNum(sequenceNumber);
    setSynBit(1);
    setAckBit(0);
    setFinBit(0);
    setCheckSum();
}


void packFirst() {
    setPort();
    setSeqNum(sequenceNumber);
    setSize(fileName.length());
    setSynBit(0);
    setAckBit(0);
    setFinBit(0);
    setCheckSum();
    // ���ݶε�ǰlength��λ�÷��ļ���
    for (int j = 0;j < fileName.length();j++) {
        sendBuffer[HEAD_SIZE + j] = fileName[j];
    }
}
// ����ͷ
void packData() {
    setPort();
    setSeqNum(sequenceNumber);
    setSize(0);
    setSynBit(0);
    setAckBit(0);
    setFinBit(0);
    setCheckSum();
}

//##################################### DataPack #####################################//

//##################################### LogPrint #####################################//

void printLogSendBuffer() {
    //ccout << "sendBuffer: ";
    //ccout << "SeqNum: " << getter.getSeqNum(sendBuffer) << ", ";
    //ccout << "AckNum: " << getter.getAckNum(sendBuffer) << ", ";
    //ccout << "Size: " << getter.getSize(sendBuffer) << ", ";
    //ccout << "SYN: " << getter.getSynBit(sendBuffer) << ", ";
    //ccout << "ACK: " << getter.getAckBit(sendBuffer) << ", ";
    //ccout << "FIN: " << getter.getFinBit(sendBuffer) << ", ";
    //ccout << "CheckSum: " << getter.getCheckSum(sendBuffer) << endl;
}

void printLogRecvBuffer() {
    //ccout << "recvBuffer: ";
    //ccout << "SeqNum: " << getter.getSeqNum(recvBuffer) << ", ";
    //ccout << "AckNum: " << getter.getAckNum(recvBuffer) << ", ";
    //ccout << "Size: " << getter.getSize(recvBuffer) << ", ";
    //ccout << "SYN: " << getter.getSynBit(recvBuffer) << ", ";
    //ccout << "ACK: " << getter.getAckBit(recvBuffer) << ", ";
    //ccout << "FIN: " << getter.getFinBit(recvBuffer) << ", ";
    //ccout << "CheckSum: " << getter.getCheckSum(recvBuffer) << endl;
}
//##################################### LogPrint #####################################//

//##################################### ErrorPrint #####################################//



void printFileErr() {
    //ccout << "+--------------------------------+\n";
    //ccout << "| Sorry we cannot open the file. |\n";
    //ccout << "+--------------------------------+\n";
}

void printRTOErr() {
    //ccout << "+-----------------------------------------------------------+\n";
    //ccout << "| Over RTO. The server did not respond us, will send again. |\n";
    //ccout << "+-----------------------------------------------------------+\n";
}

//##################################### ErrorPrint #####################################//
