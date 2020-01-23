#pragma region Includes
#ifndef __unix__
#include <winsock.h>
#include <windows.h>
#else
#define closesocket close
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef __unix__
#include <io.h>
#else
#include <sys/io.h>
#endif
#pragma endregion

#pragma region Defines
#define PROTOPORT       5193            /* default protocol port number */
#pragma endregion

#pragma region Structures
typedef struct transaction
{
	char c_uid[51]; //User id
	char c_sts[4];  //Stock ticker symbol
	enum { BUY, SELL } e_type; //Buy or sell
	int i_amount;   //The amount
	char c_response[51];
} TR, *pTR;
#pragma endregion

int main(int argc, char* argv[])
{
	#pragma region Variables
    struct  hostent  *pHostTableEntry;		/* pointer to a szHost table entry     */
    struct  protoent *pProtocolTableEntry;	/* pointer to a protocol table entry   */
    struct  sockaddr_in sadServerAddres;	/* structure to hold an IP address     */
    int     sd;								/* socket descriptor                   */
    int     iPortNumber;					/* protocol port number                */
    char    *szHost;						/* pointer to host name                */
    int     n;								/* number of characters read           */
    char    szBuffer[sizeof(TR)];			/* size = 116, buffer for data from the server     */
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
		return;
	}
	#endif

	TR tr;			//Transaction data
	char buf[255];	//Temporary variable used as the user input buffer
	char *end;		//Temporary variable used in the converison between string and int
	#pragma endregion Variables

	#pragma region Setup
	memset((char *)&sadServerAddres,0,sizeof(sadServerAddres)); /* clear sockaddr structure */
    sadServerAddres.sin_family = AF_INET;         /* set family to Internet     */

    /* Check command-line argument for protocol port and extract    */
    /* port number if one is specified.  Otherwise, use the default */
    /* port value given by constant PROTOPORT                       */
    if (argc > 2) 
	{
		/* if protocol port specified  */
        iPortNumber = atoi(argv[2]);   /* convert to binary         */
    }
	else 
	{
		iPortNumber = PROTOPORT;       /* use default port number   */
    }

    if (iPortNumber > 0)                   
		/* test for legal value */
        sadServerAddres.sin_port = htons((u_short)iPortNumber);
    else 
	{
		/* print error message and exit */
        fprintf(stderr,"bad port number %s\n",argv[2]);
        exit(1);
    }

    /* Check szHost argument and assign szHost name. */
    if (argc > 1) 
	{
		/* if szHost argument specified   */
        szHost = argv[1];         
    } 
	else 
	{
		szHost = szLocalHost;
    }

    /* Convert szHost name to equivalent IP address and copy to sadServerAddres. */
    pHostTableEntry = gethostbyname(szHost);
    if( pHostTableEntry == 0 ) 
	{
		fprintf(stderr,"invalid szHost: %s\n", szHost);
        exit(1);
    }

    memcpy(&sadServerAddres.sin_addr, pHostTableEntry->h_addr, pHostTableEntry->h_length);

    /* Map TCP transport protocol name to protocol number. */
	if ((pProtocolTableEntry = getprotobyname("tcp")) == 0)
	{
		fprintf(stderr, "cannot map \"tcp\" to protocol number");
        exit(1);
    }

    /* Create a socket. */
	//sd = socket(PF_INET, SOCK_STREAM, pProtocolTableEntry);
	sd = socket(2, 1, 6);
	if (sd < 0)
	{	
		fprintf(stderr, "SocketError");
		exit(10);
	}

    /* Connect the socket to the specified server. */
	if (connect(sd,(struct sockaddr *) &sadServerAddres,sizeof(sadServerAddres)) < 0)
	{
		fprintf(stderr, "ConnectError");
		exit(11);
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
	write(1, szBuffer, n);
		
	//User inputs
	printf("\nEnter your username (max 50 characters): ");
	if (fgets(tr.c_uid, sizeof(tr.c_uid), stdin))
	{
		//Empties the buffer to prevent overflows
		char *p;
		//strchr locates the first occurance of a character in a string
		//Returns a pointer to the first occurance if found
		//Returns null if not found
		if (p = strchr(tr.c_uid, '\n')) 
		{	
			*p = 0;
		}
		else 
		{
			//Clears upto newline
			scanf("%*[^\n]"); scanf("%*c");
		}
	}
	//tr.c_uid[strlen(tr.c_uid) - 1] = '\0';
	printf("Enter the ticker (max 3 characters): ");
	if (fgets(tr.c_sts, sizeof(tr.c_sts), stdin))
	{
		char *p;
		if (p = strchr(tr.c_sts, '\n')) 
		{
			*p = 0;
		}
		else
		{
			scanf("%*[^\n]"); scanf("%*c");
		}
	}
	//tr.c_sts[3] = '\0';
	printf("Buy or Sell: ");
	if (fgets(buf, sizeof(buf), stdin))
	{
		char *p;
		if (p = strchr(buf, '\n'))
		{
			*p = 0;
		}
		else
		{
			scanf("%*[^\n]"); scanf("%*c");
		}
	}
	if (toupper(buf[0]) == 'B')
	{
		tr.e_type = BUY;
	}
	else
	{
		tr.e_type = SELL;
	}
	for (int i = 0; i < 255; i++)
	{
		buf[i] = '\0';
	}
	printf("Enter the amount: ");
	do 
	{
		if (!fgets(buf, sizeof(buf), stdin))
		{
			break;
		}
		//Remove '\n'
		buf[strlen(buf) - 1] = 0;
		tr.i_amount = strtol(buf, &end, 10);
	} while (end != buf + strlen(buf));

	//Sends the transaction data over to the service
	send(sd, (char*)&tr, sizeof(TR), 0);
        
	//Recieves the response from the service
	n = recv(sd, (char*)&tr, sizeof(TR), 0);
	write(1, tr.c_response, strlen(tr.c_response));
	#pragma endregion

	#pragma region Cleanup
	/* Close the socket. */
	closesocket(sd);
	#ifdef WIN32
	WSACleanup();
	#endif
	#pragma endregion
	_getch();
    exit(0);
}


