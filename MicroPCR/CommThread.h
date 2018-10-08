#include <vector>
using namespace std;

#define WM_COMM_READ (WM_USER + 200)
#define BUFF_SIZE			64
//#define BUFF_SIZE			4192
//#define BUFF_SIZE			8384

//	��� Ŭ����	CCommThread 

// ��Ʈ���� �б� :
//   ��Ʈ�� �� �Ŀ� ��Ʈ�� �ڷᰡ �����ϸ� WM_COMM_READ �޽����� ���� 
//   �����쿡 ���޵ȴ�. ON_MESSAGE ��ũ�θ� �̿�, �Լ��� �����ϰ�
//   m_ReadData String�� ����� �����͸� �̿� �б�
// ��Ʈ�� ���� : WriteComm(buff, 30)�� ���� ���ۿ� �� ũ�⸦ �ǳ׸� �ȴ�.
class CQueue
{
public:
	BYTE buff[BUFF_SIZE];//ť ���� ������� 4192�� ���� 
	int m_iHead, m_iTail;//ť ���ۿ� �����͸� �ְ� �� �ö� ����� ���� 
	CQueue();
	void Clear();//���۸� �ʱ�ȭ ��Ŵ
	int GetSize();//���� ���ۿ� ����ִ� �������� size�� ����
	BOOL PutByte(BYTE b);//ť���ۿ� 1����Ʈ�� ����
	BOOL GetByte(BYTE *pb);//ť���ۿ��� 1����Ʈ�� �� ��
};

class	CCommThread
{
public:

	CCommThread();
	~CCommThread();
	//--------- ȯ�� ���� -----------------------------------------//
	BOOL        check;
	HANDLE		m_hComm;				// ��� ��Ʈ ���� �ڵ�
	CString		m_sPortName;			// ��Ʈ �̸� (COM1 ..)
	BOOL		m_bConnected;			// ��Ʈ�� ���ȴ��� ������ ��Ÿ��.
	OVERLAPPED	m_osRead, m_osWrite;	// ��Ʈ ���� Overlapped structure
	HANDLE		m_hThreadWatchComm;		// Watch�Լ� Thread �ڵ�.
	WORD		m_wPortID;				// WM_COMM_READ�� �Բ� ������ �μ�.
	CQueue      m_QueueRead;			// ť����
	BOOL		m_SendStatus;			// ������ ���� ���� 
	//--------- �ܺ� ��� �Լ� ------------------------------------//
	BOOL	OpenPort(CString strPortName, 
					   DWORD dwBaud, BYTE byData, BYTE byStop, BYTE byParity, int flow );//��Ʈ ���� 
	void	ClosePort();				//��Ʈ �ݱ�
	DWORD	WriteComm(BYTE *pBuff, DWORD nToWrite);//��Ʈ�� ������ ����

	//--------- ���� ��� �Լ� ------------------------------------//
	DWORD	ReadComm(BYTE *pBuff, DWORD nToRead);//��Ʈ���� ������ �о����

	// Adding by YJ(141111)
	void searchPort(vector<CString> &portList);
};

// Thread�� ����� �Լ� 
DWORD	ThreadWatchComm(CCommThread* pComm);

