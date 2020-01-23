#pragma region Includes
#include <winsock.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <io.h>
#include <tchar.h>
#pragma endregion

#pragma region Defines
#define PROTOPORT			5193            /* default protocol port number */
#define SimultaneousUsers	50
#define TestRuns			2
#pragma endregion

#pragma region Structures
typedef struct transaction
{
	char c_uid[51];				//User id
	char c_sts[4];				//Stock ticker symbol
	enum { BUY, SELL } e_type;	//Buy or sell
	int i_amount;				//The amount
	char c_response[51];
} TR, *pTR;
#pragma endregion

//Simulated client with static inputs
static DWORD WINAPI Client(LPVOID lpParam)
{
	#pragma region Variables
    struct  hostent  *pHostTableEntry;		/* pointer to a szHost table entry     */
    struct  protoent *pProtocolTableEntry;	/* pointer to a protocol table entry   */
    struct  sockaddr_in sadServerAddres;	/* structure to hold an IP address     */
    int     sd;								/* socket descriptor                   */
    int     iPortNumber;					/* protocol port number                */
    char    *szHost;						/* pointer to host name                */
    int     n;								/* number of characters read           */
    char    szBuffer[sizeof(TR)];			/* buffer for data from the server     */
	char	szLocalHost[] = "localhost";	/* default szHost name				   */

	#ifdef WIN32
	WSADATA wsaData;
	WORD wVersionRequested;
	int err;

 	wVersionRequested = MAKEWORD( 2, 2 );
 	err = WSAStartup( wVersionRequested, &wsaData);
	if ( err != 0 ) 
	{
		/* Tell the user that we could not find a usable */
		/* WinSock DLL.                                  */
		return -1;
	}
	#endif

	TR tr;			//Transaction data
	char buf[255];	//Temporary variable used as the user input buffer
	#pragma endregion Variables

	#pragma region Setup
	memset((char *)&sadServerAddres,0,sizeof(sadServerAddres)); /* clear sockaddr structure */
    sadServerAddres.sin_family = AF_INET;         /* set family to Internet     */

	iPortNumber = PROTOPORT;       /* use default port number   */

	sadServerAddres.sin_port = htons((u_short)iPortNumber);
    
	szHost = szLocalHost;

    /* Convert szHost name to equivalent IP address and copy to sadServerAddres. */
    pHostTableEntry = gethostbyname(szHost);
    if( pHostTableEntry == 0 ) 
	{
		fprintf(stderr,"invalid szHost: %s\n", szHost);
        return(-1);
    }

    memcpy(&sadServerAddres.sin_addr, pHostTableEntry->h_addr, pHostTableEntry->h_length);

    /* Map TCP transport protocol name to protocol number. */
	if ((pProtocolTableEntry = getprotobyname("tcp")) == 0)
	{
		fprintf(stderr, "cannot map \"tcp\" to protocol number");
        return(1);
    }

    /* Create a socket. */
	//sd = socket(PF_INET, SOCK_STREAM, pProtocolTableEntry);
	sd = socket(2, 1, 6);
	if (sd < 0)
	{	
		fprintf(stderr, "SocketError");
		return(10);
	}

	for (int i = 0; i < 10; i++) //10 connection attempts
	{
		/* Connect the socket to the specified server. */
		if (connect(sd, (struct sockaddr *) &sadServerAddres, sizeof(sadServerAddres)) < 0)
		{
			if (i == 9)
			{
				fprintf(stderr, "ConnectError");
				return(11);
			}
			else
			{
				fprintf(stderr, "ConnectError, Retrying");
				Sleep(100);
			}
		}	
		else
		{
			break;
		}
	}

	//Empties the arrays
	for (int i = 0; i < 255; i++)
	{
		buf[i] = '\0';
	}
	for (int i = 0; i < 51; i++)
	{
		tr.c_uid[i] = '\0';
	}
	for (int i = 0; i < 4; i++)
	{
		tr.c_sts[i] = '\0';
	}
	#pragma endregion
		
	#pragma region Datahandling
	//Recieves and writes the ACK message from the service
	n = recv(sd, szBuffer, sizeof(szBuffer), 0);
	//write(1, szBuffer, n);
		
	//User inputs
	//printf("\nEnter your username (max 50 characters): ");
	strcpy(tr.c_uid, "TestUser");
	tr.c_uid[strlen(tr.c_uid)] = '\0';
	//printf("Enter the ticker (max 3 characters): ");
	strcpy(tr.c_sts, "tst");
	tr.c_sts[3] = '\0';
	//printf("Buy or Sell: ");
	tr.e_type = BUY;
	//printf("Enter the amount: ");
	tr.i_amount = 1;

	//Sends the transaction data over to the service
	send(sd, (char*)&tr, sizeof(TR), 0);
        
	//Recieves the response from the service
	n = recv(sd, (char*)&tr, sizeof(TR), 0);
	tr.c_response[strlen(tr.c_response)] = '\n';
	write(1, tr.c_response, strlen(tr.c_response));
	#pragma endregion

	#pragma region Cleanup
	/* Close the socket. */
	//closesocket(sd);
	#ifdef WIN32
	WSACleanup();
	#endif
	#pragma endregion
    return 0;
}

int main(int argc, char *argv[])
{
	DWORD dwThreadIdArray[SimultaneousUsers];	
	HANDLE hThreadArray[SimultaneousUsers];		
	int RunningThreads = 0;
	int Passes = 0, Abandons = 0, Timeouts = 0, Failes = 0, Unknowns = 0, ConnectErrors = 0;
	DWORD waitResult;
	DWORD resultCode;

	for (int i = 0; i < TestRuns; i++)
	{
		printf("Loading test %d/%d: \n", i + 1, TestRuns);
		Sleep(1000);
		for (int i = 0; i < SimultaneousUsers; i++)
		{
			hThreadArray[i] = CreateThread(NULL, 0, Client, NULL, 0, &dwThreadIdArray[i]);
			RunningThreads++;
		}

		waitResult = WaitForMultipleObjects(SimultaneousUsers, hThreadArray, TRUE, INFINITE);
		
		for (int i = 0; i < 1; i++)
		{
			//waitResult = WaitForSingleObject(hThreadArray[i], 100);
			if (waitResult == WAIT_OBJECT_0)
			{
				printf("\nPassed\n");
				Passes++;
			}
			else if (waitResult == WAIT_ABANDONED_0)
			{
				printf("\nAbandoned\n");
				Abandons++;
			}
			else if (waitResult == WAIT_TIMEOUT)
			{
				printf("\nTimeout\n");
				Timeouts++;
			}
			else if (waitResult == WAIT_FAILED)
			{
				printf("\nFailed\n");
				Failes++;
			}
			else
			{
				if (GetExitCodeThread(hThreadArray[i], &resultCode) != 0)
				{
					if (resultCode == 11)
					{
						printf("\nConnect Error\n");
						ConnectErrors++;
					}
					else
					{
						printf("\nUnknown Error\n");
						Unknowns++;
					}
				}
				else
				{
					printf("\nUnknown Error\n");
					Unknowns++;
				}				
			}
		}

		for (int i = 0; i < RunningThreads; i++)
		{
			CloseHandle(hThreadArray[i]);
		}
		RunningThreads = 0;

		Sleep(1000);
	}

	printf("\n\tPasses: %d\n\tAbandoned: %d\n\tTimeouts: %d\n\tFailes: %d\n\tUnknowns: %d\n\tConnects: ", Passes, Abandons, Timeouts, Failes, Unknowns, ConnectErrors);
	printf("\n\nAll tests passed, press any key to exit...");
	_gettch();
	
	return 0;
}