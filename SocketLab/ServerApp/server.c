#pragma region Includes
#include <winsock2.h> 
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <tchar.h>
#pragma endregion Includes

#pragma region Definitions
#define PROTOPORT         5193	/* default protocol port number */
#define QUEUESIZE         100	/* size of request queue        */
#define SimultaneousUsers 100	/* how many threads can be run simultaneously */
#pragma endregion Definitions

#pragma region Structures
typedef struct transaction
{
	char c_uid[51];				//User id
	char c_sts[4];				//Stock ticker symbol
	enum { BUY, SELL } e_type;	//Buy or sell
	int i_amount;				//The amount
	char c_response[51];		//Used for the communication between the service and the DBM/DBS and transmitting messages to the client
} TR, *pTR; //sizeof=164		//Aliases

typedef struct company
{
	char c_user[51];			//Holds the username
	char c_tickerSymbol[4];		//Holds the 3 tickerSymbols + escape character
	int i_amount;				//How many shares a user owns
} CO, *pCO;						//Aliases

typedef struct ThreadData {
	int sd;				//Socket descriptor
	HANDLE sm;			//Shard memory handle
	TCHAR smName[30];	//Shared memory name
	HANDLE trMutex;		//Mutex to the transaction data
} THREADDATA, *PMYDATA; //aliases

typedef struct DBSData {
	pTR tr;				//Pointer to the transction data from the client
	HANDLE sm;			//Shard memory handle
	TCHAR smName[30];	//Shared memory name
} DBSDATA, *PDBSDATA; //aliases

#pragma endregion Structures

#pragma region Global Variables
CO companies[10];	//The storage of the combined transactions
HANDLE hMutex;
#pragma endregion


static DWORD WINAPI DBS(LPVOID lpParam)
{
	#pragma region Variables
	PDBSDATA data = (PDBSDATA)lpParam;
	#pragma endregion

	#pragma region Data processing
	WaitForSingleObject(hMutex, INFINITE); //Request ownership of the mutex
	// Checks if the ticker and the user is found in the companies array if not, adds them to the array
	for (int i = 0; i < (sizeof(companies) / sizeof(companies[0])); i++)
	{
		//Ticker found
		if (strcmp(companies[i].c_tickerSymbol, data->tr->c_sts) == 0) //Identical match
		{
			//User found
			if (strcmp(companies[i].c_user, data->tr->c_uid) == 0) //Identical match
			{
				if (data->tr->e_type == BUY)
				{
					companies[i].i_amount = companies[i].i_amount + data->tr->i_amount;
					//Reply message
					sprintf(data->tr->c_response, "Success. %s now owns %d share(s) in %s", companies[i].c_user, companies[i].i_amount, companies[i].c_tickerSymbol);
					//sprintf(data->tr->c_response, "Success. You now own %d share(s)", companies[i].i_amount);
					break;
				}
				else if (data->tr->e_type == SELL)
				{
					companies[i].i_amount = companies[i].i_amount - data->tr->i_amount;
					//Reply message
					sprintf(data->tr->c_response, "Success. %s now owns %d share(s) in %s", companies[i].c_user, companies[i].i_amount, companies[i].c_tickerSymbol);
					//sprintf(data->tr->c_response, "Success. You now own %d share(s)", companies[i].i_amount);
					break;
				}
			}
		}
		//Ticker nor User found
		else if (i == (sizeof(companies) / sizeof(companies[0])) - 1)
		{
			//Searches for the first unused spot in the companies array
			for (int ii = 0; ii < (sizeof(companies) / sizeof(companies[0])); ii++)
			{
				//Spot found
				if (companies[ii].c_tickerSymbol[0] == '\0')
				{
					//Copies the data over to the new spot
					strcpy(companies[ii].c_tickerSymbol, data->tr->c_sts);
					strcpy(companies[ii].c_user, data->tr->c_uid);
					if (data->tr->e_type == BUY)
					{
						companies[ii].i_amount = companies[ii].i_amount + data->tr->i_amount;
					}
					else if (data->tr->e_type == SELL)
					{
						companies[ii].i_amount = companies[ii].i_amount - data->tr->i_amount;
					}
					//Reply message
					sprintf(data->tr->c_response, "Success. %s now owns %d share(s) in %s", companies[ii].c_user, companies[ii].i_amount, companies[ii].c_tickerSymbol);
					//sprintf(data->tr->c_response, "Success. You now own %d share(s)", companies[ii].i_amount);
					break;
				}
				//No spots available
				else if (ii == (sizeof(companies) / sizeof(companies[0])) - 1)
				{
					//507 -> not enough storage
					sprintf(data->tr->c_response, "Error 507");
					break;
				}
			}
		}
	}
	#pragma endregion

	//CopyMemory((PVOID)data->tr, data->tr, (_tcslen(data->tr) * sizeof(TR))); //Useless?

	ReleaseMutex(hMutex); //Release ownership

	return 0;
}

static DWORD WINAPI DBM(LPVOID lpParam)
{
	#pragma region Variables
	pTR recieved[SimultaneousUsers];	 //Pointer to the shared memory spaces
	HANDLE smArray[SimultaneousUsers];	 //The handles for shared memory space, NOT the same variable as in the main()!
	TCHAR szName[SimultaneousUsers][30]; //2D Array!! //Contains the names of the shared memory spaces, NOT the same variable as in the main()!
	int iii = 0;						 //Index variable
	int stop = FALSE;					 //Checks if a service has delivered data for processing
	DWORD dwThreadIdArray[SimultaneousUsers];	//An array containing the IDs of the DBS threads
	HANDLE hThreadArray[SimultaneousUsers];		//An array containing the handles for the DBS threads
	DBSDATA pDataArray[SimultaneousUsers];	//An array containing the data that will be passed to the DBS thread
	int RunningThreads = 0;						//How many threads are running
	DWORD dwWaitResult;
	#pragma endregion

	#pragma region Setup
	hMutex = CreateMutex(
		NULL,              //Default security attributes
		FALSE,             //Initially not owned
		NULL);             //Unnamed mutex

	WaitForSingleObject(hMutex, INFINITE); //Request ownership of the mutex
	//Prepares the companies array and its content
	for (int i = 0; i < (sizeof(companies) / sizeof(companies[0])); i++)
	{
		for (int ii = 0; ii < 4; ii++)
		{
			companies[i].c_tickerSymbol[ii] = '\0';
		}
		for (int ii = 0; ii < 51; ii++)
		{
			companies[i].c_user[ii] = '\0';
		}
		companies[i].i_amount = 0;
	}
	ReleaseMutex(hMutex); //Releases ownership

	for (int i = 0; i < SimultaneousUsers; i++)
	{
		//Empties the szName (2D)array
		for (int ii = 0; ii < 30; ii++)
		{
			szName[i][ii] = '\0';
		}
	}

	strcpy(szName[0], "Global\\OsFileMappingObject");
	szName[0][strlen(szName[0])] = 65;
	for (int i = 1; i < SimultaneousUsers; i++)
	{
		//sprintf(szName[i], "Global\\OsFileMappingObject%d", i);

		strcpy(szName[i], "Global\\OsFileMappingObject");

		if (strlen(szName[i]) + 1 != strlen(szName[i - 1]))
		{
			szName[i][strlen(szName[i])] = szName[i - 1][strlen(szName[i])];
		}

		szName[i][strlen(szName[i])] = szName[i - 1][strlen(szName[i])] + 1;

		//Shared memory names are not case sensitive?
		/*if (szName[i][strlen(szName[i]) - 1] == 91 )
		{
			//The ascii values 91-96 are special characters
			szName[i][strlen(szName[i]) - 1] += 6;
		}*/
		if (szName[i][strlen(szName[i]) - 1] == 91/*123*/)
		{
			if (strlen(szName[i - 1]) == 27)
			{
				szName[i][strlen(szName[i]) - 1] = 65;
				szName[i][strlen(szName[i])] = 65;
			}
			else
			{
				szName[i][strlen(szName[i]) - 2] = szName[i - 1][strlen(szName[i]) - 2] + 1;
				szName[i][strlen(szName[i]) - 1] = 65;
			}
		}
	}

	for (int i = 0; i < SimultaneousUsers; i++)
	{
		smArray[i] = OpenFileMapping(
			FILE_MAP_ALL_ACCESS,   //read/write access
			FALSE,                 //do not inherit the name
			szName[i]);            //name of mapping object

		recieved[i] = (pTR)MapViewOfFile(
			smArray[i],			  //handle to map object
			FILE_MAP_ALL_ACCESS,  //read/write permission
			0,					  //high-order DWORD of the file offset where the view begins
			0,					  //low-order DWORD of the file offset where the view is to begin
			sizeof(TR));		  //number of bytes of a file mapping to map to the view
	}
	#pragma endregion Setup

	while (TRUE)
	{
		#pragma region Waits for a package to be delivered from a service
		stop = FALSE;
		for (; !stop; iii++)
		{
			__try
			{
				dwWaitResult = WaitForSingleObject(hMutex, INFINITE); //WAIT?
				if (recieved[iii]->c_response[0] == '1' && dwWaitResult == WAIT_OBJECT_0)
				{
					//Package delivered
					recieved[iii]->c_response[0] = '2';
					pDataArray[RunningThreads] = *(PDBSDATA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(DBSDATA));
					pDataArray[RunningThreads].tr = recieved[iii];
					pDataArray[RunningThreads].sm = smArray[iii];				//Shared memory handle
					strcpy(pDataArray[RunningThreads].smName, szName[iii]);	//Name of the shared memory space 

					ReleaseMutex(hMutex);
					//Creates a service
					hThreadArray[RunningThreads] = CreateThread(NULL, 0, DBS, (LPVOID)&pDataArray[RunningThreads], 0, &dwThreadIdArray[RunningThreads]);
					RunningThreads++;

					/*
						If the dbs thread array is full, the DBM will wait for all to finish
						To do:
							-Sense when a DBS is done and can be reused
					*/
					if (RunningThreads == SimultaneousUsers)
					{
						WaitForMultipleObjects(RunningThreads, hThreadArray, TRUE, INFINITE);
						//RunningThreads = 0;
						//Implement auto-restart feature
					}

					iii--;
				}
			}
			__except (EXCEPTION_CONTINUE_EXECUTION)
			{

			}
			ReleaseMutex(hMutex);

			if (iii == SimultaneousUsers - 1)
			{
				//Repeat the loop
				//Could use a true loop and a break instead
				iii = -1; //On continue iii==0;
			}			
		}
	#pragma endregion
	}

	#pragma region Cleanup
	WaitForMultipleObjects(RunningThreads, hThreadArray, TRUE, INFINITE);
	for (int i = 0; i < RunningThreads; i++)
	{
		CloseHandle(hThreadArray[i]);
	}
	for (int i = 0; i < SimultaneousUsers; i++)
	{
		CloseHandle(smArray[i]);
	}
	#pragma endregion

	return 0;
}

static DWORD WINAPI Service(LPVOID lpParam)
{
	#pragma region Variables
	PMYDATA data = (PMYDATA)lpParam;	//Holds the data passed from the parent (See "struct ThreadData")
	char szBuffer[sizeof(TR)];			//Buffer that sends and retrives the data from/to the client
	pTR recieved;						//Pointer to the data recieved and passed to the client
	pTR pBuf;							//Pointer to the data in the shared memory space
	DWORD dwWaitResult;					//Request mutex result
	#pragma endregion

	#pragma region Setup
	data->sm = OpenFileMapping(
		FILE_MAP_ALL_ACCESS,		//read/write access
		FALSE,						//do not inherit the name
		data->smName);				//name of mapping object

	pBuf = (pTR)MapViewOfFile(data->sm, //handle to map object
		FILE_MAP_ALL_ACCESS,			//read/write permission
		0,								//high-order DWORD of the file offset where the view begins
		0,								//low-order DWORD of the file offset where the view is to begin
		sizeof(TR));					//number of bytes of a file mapping to map to the view

	//Empties the szBuffer
	for (int i = 0; i < sizeof(TR); i++)
	{
		szBuffer[i] = '\0';
	}
	#pragma endregion

	#pragma region Communication with the client
	//Sends ACK message to the client
	sprintf(szBuffer, "Now connected");
	send(data->sd, szBuffer, strlen(szBuffer), 0);

	//Empties the szBuffer
	for (int i = 0; i < sizeof(TR); i++)
	{
		szBuffer[i] = '\0';
	}

	//Recieves the data package
	recv(data->sd, szBuffer, sizeof(szBuffer), 0);
	recieved = (pTR)&szBuffer;
	#pragma endregion

	#pragma region Communication with the DBM process
	//Empties the response variable
	for (int i = 0; i < sizeof(recieved->c_response); i++)
	{
		recieved->c_response[i] = '\0';
	}

	//Used to let the DBM know the package from this thread is ready
	recieved->c_response[0] = '1';

	//Sends the data over to the shared memory
	dwWaitResult = WaitForSingleObject(hMutex, INFINITE);
	strcpy(pBuf->c_uid, recieved->c_uid);
	strcpy(pBuf->c_sts, recieved->c_sts);
	pBuf->e_type = recieved->e_type;
	pBuf->i_amount = recieved->i_amount;
	strcpy(pBuf->c_response, recieved->c_response);
	ReleaseMutex(hMutex);
	

	//Waits while the DBM processes the data
	/*while (((pTR)pBuf)->c_response[0] == '1' || ((pTR)pBuf)->c_response[0] == '2')
	{
		Sleep(200);
	}*/

	/*recieved->c_response[0] == '1' || recieved->c_response[0] == '2'*/
	while (1)
	{
		dwWaitResult = WaitForSingleObject(hMutex, INFINITE); //Request ownership of the mutex //WAIT?
		if (dwWaitResult == WAIT_OBJECT_0)
		{
			recieved = pBuf;
			if (recieved->c_response[0] == '1' || recieved->c_response[0] == '2')
			{
				ReleaseMutex(hMutex);
			}
			else
			{
				break;
			}
		}
		ReleaseMutex(hMutex);
		Sleep(200);
	}
	ReleaseMutex(hMutex);
	

	recieved = pBuf; //Updates the recieved variable
	#pragma endregion

	#pragma region Communication with the client
	//Sends the reply
	send(data->sd, recieved, sizeof(TR), 0);
	#pragma endregion

	#pragma region Cleanup
	closesocket(data->sd);
	CloseHandle(data->sm);
	#pragma endregion

	return 0;
}

int main(int argc, char *argv[])
{
	#pragma region Variables
	struct  protoent *pProtocolTableEntry;	/* pointer to a protocol table entry   */
	struct  sockaddr_in sadServerAddres;	/* structure to hold an IP address     */
	struct  sockaddr_in sadClientAddress;	/* structure to hold client's address  */
	int     sd;								/* socket descriptors                  */
	int		sd2[SimultaneousUsers];			/* socket descriptor for the client    */
	int     iPortNumber;					/* protocol port number                */
	int     iAddressLength;					/* length of address                   */
	int		iVisits = 0;					/* counts client connections    */

	#ifdef WIN32 //Also Windows x64
	WSADATA wsaData;
	WORD wVersionRequested;
	int err;
	wVersionRequested = MAKEWORD(2, 2);
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0)
	{
		/* Tell the user that we could not find a usable */
		/* WinSock DLL.                                  */
		return;
	}
	#endif

	HANDLE smArray[SimultaneousUsers];			//An array containing the handles to shared memory spaces
	LPTSTR pBuf[SimultaneousUsers];				//An array containing the values of the shared memory (?Variable can be removed?)
	TCHAR szName[SimultaneousUsers][30];		//An array containing the names of the shared memory spaces
	HANDLE dbmThread;							//The handle to the dbm thread
	DWORD dwThreadIdArray[SimultaneousUsers];	//An array containing the IDs of the service threads
	HANDLE hThreadArray[SimultaneousUsers];		//An array containing the handles for the service threads
	THREADDATA pDataArray[SimultaneousUsers];	//An array containing the data that will be passed to the sevice thread
	int RunningThreads = 0;						//How many threads are running
	DWORD Result;								//Result from the service thread
	#pragma endregion

	#pragma region Setup 
	//Allocates "SimultaneousUsers" count of TCHAR arrays for shared memory spaces
	for (int i = 0; i < SimultaneousUsers; i++)
	{
		for (int ii = 0; ii < 30; ii++)
		{
			szName[i][ii] = '\0';
		}
	}

	strcpy(szName[0], "Global\\OsFileMappingObject");
	szName[0][strlen(szName[0])] = 65;
	for (int i = 1; i < SimultaneousUsers; i++)
	{
		//sprintf(szName[i], "Global\\OsFileMappingObject%d", i);

		strcpy(szName[i], "Global\\OsFileMappingObject");

		if (strlen(szName[i]) + 1 != strlen(szName[i - 1]))
		{
			szName[i][strlen(szName[i])] = szName[i - 1][strlen(szName[i])];
		}

		szName[i][strlen(szName[i])] = szName[i - 1][strlen(szName[i])] + 1;

		/*if (szName[i][strlen(szName[i]) - 1] == 91 )
		{
			//The ascii values 91-96 are special characters
			szName[i][strlen(szName[i]) - 1] += 6;
		}*/
		if (szName[i][strlen(szName[i]) - 1] == 91/*123*/)
		{
			if (strlen(szName[i - 1]) == 27)
			{
				szName[i][strlen(szName[i]) - 1] = 65;
				szName[i][strlen(szName[i])] = 65;
			}
			else
			{
				szName[i][strlen(szName[i]) - 2] = szName[i - 1][strlen(szName[i]) - 2] + 1;
				szName[i][strlen(szName[i]) - 1] = 65;
			}
		}
	}

	//Creates "SimultaneousUsers" count of shared memory spaces
	for (int i = 0; i < SimultaneousUsers; i++)
	{
		//Can not be higher than 26*26 = 676
		smArray[i] = CreateFileMapping(
			INVALID_HANDLE_VALUE,   // use paging file 
			NULL,                   // default security
			PAGE_READWRITE,         // read/write access
			0,                      // maximum object size (high-order DWORD)
			sizeof(TR),				// maximum object size (low-order DWORD)
			szName[i]);				// name of mapping object
		if (smArray[i] == NULL)
		{
			printf(TEXT("Could not open file mapping object (%d).\n"), GetLastError());
			return 1;
		}

		pBuf[i] = (LPTSTR)MapViewOfFile(
			smArray[i],				//handle to map object
			FILE_MAP_ALL_ACCESS,	//read/write permission
			0,						//high-order DWORD of the file offset where the view begins
			0,						//low-order DWORD of the file offset where the view is to begin
			sizeof(TR));			//number of bytes of a file mapping to map to the view
		if (pBuf == NULL)
		{
			printf(TEXT("Could not map view of file (%d).\n"), GetLastError());
			if (CloseHandle(smArray[i]) == 0)
			{
				printf(TEXT("Could not close the handle (%d).\n"), GetLastError());
				_gettch();
			}
			return 1;
		}
	}

	//DBM(); //Init the dbm
	dbmThread = CreateThread(NULL, 0, DBM, (void*)szName, 0, NULL);

	memset((char *)&sadServerAddres, 0, sizeof(sadServerAddres)); /* clear sockaddr structure */
	sadServerAddres.sin_family = AF_INET;         /* set family to Internet     */
	sadServerAddres.sin_addr.s_addr = INADDR_ANY; /* set the local IP address   */

	/* Check command-line argument for protocol port and extract    */
	/* port number if one is specified.  Otherwise, use the default */
	/* port value given by constant PROTOPORT                       */
	if (argc > 1)
	{
		/* if argument specified */
		iPortNumber = atoi(argv[1]);   /* convert argument to binary */
	}
	else
	{
		iPortNumber = PROTOPORT;       /* use default port number */
	}

	if (iPortNumber > 0)
		/* test for illegal value */
		sadServerAddres.sin_port = htons((u_short)iPortNumber); // Host to Network short
	else
	{
		/* print error message and exit */
		fprintf(stderr, "bad port number %s\n", argv[1]);
		WSACleanup();
		exit(1);
	}

	/* Map TCP transport protocol name to protocol number */
	if ((pProtocolTableEntry = getprotobyname("tcp")) == 0)
	{
		fprintf(stderr, "cannot map \"tcp\" to protocol number");
		WSACleanup();
		exit(1);
	}

	/* Create a socket */
	sd = socket(AF_INET, SOCK_STREAM, pProtocolTableEntry->p_proto);
	//sd = socket(2, 1, 6);
	if (sd < 0)
	{
		fprintf(stderr, "SocketError: %i", WSAGetLastError());
		WSACleanup();
		exit(10);
	}

	/* Bind a local address to the socket */
	if (bind(sd, (struct sockaddr *) &sadServerAddres, sizeof(sadServerAddres)))
	{
		fprintf(stderr, "BindError");
		WSACleanup();
		exit(11);
	}

	/* Specify size of request queue */
	if (listen(sd, QUEUESIZE) < 0)
	{
		fprintf(stderr, "ListenError");
		WSACleanup();
		exit(12);
	}

	iAddressLength = sizeof(sadClientAddress);
	printf("Ready\n");
	#pragma endregion End of setup

	#pragma region Accepts and handles requests from clients
	while (TRUE)
	{
		//Waits for a client to connect
		if ((sd2[RunningThreads] = accept(sd, (struct sockaddr *) &sadClientAddress, &iAddressLength)) < 0)
		{
			fprintf(stderr, "AcceptFailed");
		}

		pDataArray[RunningThreads] = *(PMYDATA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(THREADDATA));
		pDataArray[RunningThreads].sd = sd2[RunningThreads];					//Clients socket descriptor
		pDataArray[RunningThreads].sm = smArray[RunningThreads];				//Shared memory handle
		strcpy(pDataArray[RunningThreads].smName, szName[RunningThreads]);		//Name of the shared memory space 

		//Creates a service
		hThreadArray[RunningThreads] = CreateThread(NULL, 0, Service, (LPVOID)&pDataArray[RunningThreads], 0, &dwThreadIdArray[RunningThreads]);
		RunningThreads++;
		iVisits++;

		/*
			If the main thread array is full, the main will wait for all to finish
			To do:
				-Sense when a service is done and can be reused
		*/

		/* possible solution?
		result = WaitForSingleObject( hThread, 0);

		if (result == WAIT_OBJECT_0) {
			// the thread handle is signaled - the thread has terminated
		}
		else {
			// the thread handle is not signaled - the thread is still alive
		}
		*/
		printf("threads running: %d\n", RunningThreads);
		printf("iVisits count: %d\n", iVisits);

		if (RunningThreads == SimultaneousUsers - 1)
		{
			Result = WaitForMultipleObjects(RunningThreads, hThreadArray, TRUE, INFINITE);
			RunningThreads = 0;
			//Implement auto-restart feature
		}
	}
	#pragma endregion Accepts and handles requests from clients

	#pragma region Cleanup
	for (int i = 0; i < RunningThreads; i++)
	{
		CloseHandle(hThreadArray[i]);
	}
	for (int i = 0; i < SimultaneousUsers; i++)
	{
		CloseHandle(smArray[i]);
	}
	ExitThread(dbmThread);
	WSACleanup();
	#pragma endregion Cleanup

	exit(0);
}
