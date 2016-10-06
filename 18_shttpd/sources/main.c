/*
main.c
This file is part of pserv
http://pserv.sourceforge.net

Copyright (c) 2001-2005 Riccardo Mottola. All rights reserved.
mail: rmottola@users.sourceforge.net

This file is free software, distributed under GPL. Please read acclosed license
*/

#include "main.h"
#include "log.h"
#include "mime.h"
#include "handlers.h"

#ifndef HAVE_MEMMOVE
#include "missing.c"
#endif

/* ------ Global Variables ---- */

int 	    	ss, cs;
int 	    	error;
int 	    	newSocketReady;
char	    	homePath[MAX_PATH_LEN+1];
char	    	defaultFileName[MAX_PATH_LEN+1];
char	    	logFileName[MAX_PATH_LEN+1];
char	    	mimeTypesFileName[MAX_PATH_LEN+1];
char	    	cgiroot[MAX_PATH_LEN+1]; /* root for CGI scripts exec */
struct timeval	s_tv;
mimeData    	*mimeArray;  /* here we will hold all MIME data, inited once, never to be changed */
int 	    	mimeEntries; /* the number of known mimetypes present in *mimeArray and loaded at startup */
FILE	    	*lf;         /* log file */
FILE            *sockStream; /* stream version of the new socket */


/* ------ Signal Handlers  ------- */

void sig_int (int signo)
{
    printf("shutting down the server....\n");
    if (newSocketReady)
    {
        if (close(cs))
            printf("error closing new socket\n");
    }
    if (close(ss))
        printf("error closing mother socket\n");
    logFileClose();
    exit(0);
}

void sig_pipe(int signo)
{
    printf("BrokenPipe!\n");
    if (newSocketReady)
    {
        printf ("new socket was ready....\n");
        if (!close(cs))
            printf("closed ns\n");
    } else {
        printf("This makes no sense!!! broken pipe on a closed socket!\n");
        exit(0);
    }
    newSocketReady = NO;
}


/* ------- General Purpose Functions -------*/

int DHTTPD_HandleErrors(sock, err, errorFilePath, req)
/* output HTML code for given error code */
int sock;
int err;
char *errorFilePath;
struct request req;
{
	char outBuff[512+MAX_PATH_LEN]; /* should be enough since we hard-code output and we must contain the path! */

	switch (err)
	{
		case INPUT_LINE_TOO_LONG:
			DHTTPD_GenMimeHead(sock, 400, "text/html", NULL, req.protocolVersion, FULL_HEADER);
			strcpy(outBuff, "<HTML>\n<HEAD>\n<TITLE>Error</TITLE>\n</HEAD>\n<BODY>\n");
			write (sock, outBuff, strlen(outBuff));
			strcpy(outBuff, "<H1>Error: Browser request line too long.\n");
			write (sock, outBuff, strlen(outBuff));
			strcpy(outBuff, "</BODY>\n</HTML>\n");
			write (sock, outBuff, strlen(outBuff));
			logWriter(LOG_INPUT_LINE_TOO_LONG, NULL, 0, NULL, 0);

			break;
		case POST_BUFFER_OVERFLOW:
			DHTTPD_GenMimeHead(sock, 500, "text/html", NULL, req.protocolVersion, FULL_HEADER);
			strcpy(outBuff, "<HTML>\n<HEAD>\n<TITLE>Error</TITLE>\n</HEAD>\n<BODY>\n");
			write (sock, outBuff, strlen(outBuff));
			strcpy(outBuff, "<H1>Error: Internal Server Error (Content-length in post data too long)\n");
			write (sock, outBuff, strlen(outBuff));
			strcpy(outBuff, "</BODY>\n</HTML>\n");
			write (sock, outBuff, strlen(outBuff));
			logWriter(LOG_POST_BUFFER_OVERFLOW, NULL, 0, NULL, 0);

			break;
		case FORBIDDEN:
			DHTTPD_GenMimeHead(sock, 403, "text/html", NULL, req.protocolVersion, FULL_HEADER);
			strcpy(outBuff, "<HTML>\n<HEAD>\n<TITLE>Error</TITLE>\n</HEAD>\n<BODY>\n");
			write (sock, outBuff, strlen(outBuff));
			strcpy(outBuff, "<H1>Error 403: Forbidden.</H1>\n");
			write (sock, outBuff, strlen(outBuff));
			strcpy(outBuff, "<HR><P>\n");
			write (sock, outBuff, strlen(outBuff));
			strcpy(outBuff, errorFilePath);
			write (sock, outBuff, strlen(outBuff));
			strcpy(outBuff, "</P>\n");
			write (sock, outBuff, strlen(outBuff));
			strcpy(outBuff, "</BODY>\n</HTML>\n");
			write (sock, outBuff, strlen(outBuff));
			logWriter(LOG_FORBIDDEN, errorFilePath, 0, req, 0);

			break;
		case NOT_FOUND:
			DHTTPD_GenMimeHead(sock, 404, "text/html", NULL, req.protocolVersion, FULL_HEADER);
			strcpy(outBuff, "<HTML>\n<HEAD>\n<TITLE>Error</TITLE>\n</HEAD>\n<BODY>\n");
			write (sock, outBuff, strlen(outBuff));
			strcpy(outBuff, "<H1>Error 404: File not found.</H1>\n");
			write (sock, outBuff, strlen(outBuff));
			strcpy(outBuff, "<HR><P>\n");
			write (sock, outBuff, strlen(outBuff));
			strcpy(outBuff, errorFilePath);
			write (sock, outBuff, strlen(outBuff));
			strcpy(outBuff, "</P>\n");
			write (sock, outBuff, strlen(outBuff));
			strcpy(outBuff, "</BODY>\n</HTML>\n");
			write (sock, outBuff, strlen(outBuff));
			logWriter(LOG_FILE_NOT_FOUND, errorFilePath, 0, req, 0);

			break;
		case LENGTH_REQUIRED:
			DHTTPD_GenMimeHead(sock, 411, "text/html", NULL, req.protocolVersion, FULL_HEADER);
			strcpy(outBuff, "<HTML>\n<HEAD>\n<TITLE>Error</TITLE>\n</HEAD>\n<BODY>\n");
			write (sock, outBuff, strlen(outBuff));
			strcpy(outBuff, "<H1>Error 411: Length Required</H1>\n");
			write (sock, outBuff, strlen(outBuff));
			strcpy(outBuff, "<HR><P>\n");
			write (sock, outBuff, strlen(outBuff));
			strcpy(outBuff, errorFilePath);
			write (sock, outBuff, strlen(outBuff));
			strcpy(outBuff, "</P>\n");
			write (sock, outBuff, strlen(outBuff));
			strcpy(outBuff, "</BODY>\n</HTML>\n");
			write (sock, outBuff, strlen(outBuff));
			logWriter(LOG_LENGTH_REQUIRED, 0, req, 0);

			break;
		case UNHANDLED_METHOD:
			DBGPRINTF(("unhandled method case\n"));
			DHTTPD_GenMimeHead(sock, 501, "text/html", NULL, req.protocolVersion, FULL_HEADER);
			strcpy(outBuff, "<HTML>\n<HEAD>\n<TITLE>Error</TITLE>\n</HEAD>\n<BODY>\n");
			write (sock, outBuff, strlen(outBuff));
			strcpy(outBuff, "<H1>Error: Not Implemented: Unhandled Method.\n");
			write (sock, outBuff, strlen(outBuff));
			strcpy(outBuff, "</BODY>\n</HTML>\n");
			write (sock, outBuff, strlen(outBuff));

			break;
		default:
			DBGPRINTF(("generic SayError case\n"));
			DHTTPD_GenMimeHead(sock, 500, "text/html", NULL, req.protocolVersion, FULL_HEADER);
			strcpy(outBuff, "<HTML>\n<HEAD>\n<TITLE>Error</TITLE>\n</HEAD>\n<BODY>\n");
			write (sock, outBuff, strlen(outBuff));
			strcpy(outBuff, "<H1>Error: Unknown error trapped.\n");
			write (sock, outBuff, strlen(outBuff));
			strcpy(outBuff, "</BODY>\n</HTML>\n");
			write (sock, outBuff, strlen(outBuff));
			logWriter(LOG_GENERIC_ERROR, NULL, 0, NULL, 0);
		break;
	} /* of switch */

	return 0;
}


/* substitute escaped % characters with their equivalent */
int DHTTPD_ConvertPercents(s, l)
char s[];
int l;
{
	char tok[4];
	char tokSubst;
	size_t tokLen;
	char *tokPtr;
	size_t tokPos;


	/* we match and subsitute the %20 with a blank space */
	strcpy(tok, "%20");
	tokLen = strlen(tok);
	tokSubst = ' ';
	tokPtr = strstr(s, tok);
	while (tokPtr)
	{
		tokPos = tokPtr - s;  /* calculate the position */
		s[tokPos] = tokSubst;
		/* now we shift all characters left including the terminator */
		/* assumes tokSubst len of 1 (= one char) */
		memmove(tokPtr + 1, tokPtr + tokLen, l - tokPos - tokLen + 1);
		tokLen = strlen(tok);
		tokPtr = strstr(s, tok);
	}

	return 0;
}


int DHTTPD_ParseRequest (sock, req, reqStruct)
/* extracts meaningful tokens from the client request and stores them in the request struct */
int sock;
char req[];
struct request *reqStruct;
{
	char token[BUFFER_SIZE + 1];   /* we add one to be able the trailing new line we append for security */
	char reqArray[MAX_REQUEST_LINES][BUFFER_SIZE]; /* no need to add one here since we trim newline */
	int i, j, k;
	int reqSize;
	int readLines;
	int tokenEnd;

	/* we copy the header lines to an array for easier parsing */ 
	/* but first we make sure that our string has a newline and an end */ 
	req[BUFFER_SIZE] = '\0';
	reqSize = strlen(req);
	req[reqSize] = '\n';
	reqSize++;
	req[reqSize] = '\0';
	i = 0; j = 0;
	while (i < MAX_REQUEST_LINES && j < reqSize)
	{
		k = 0;
		while (req[j] != '\n')
		{
			token[k++] = req[j++];
		}
		token[k-1] = '\0';   /* the line read ends with an \n, we skip it and count it as read */
		j++;
		strcpy(reqArray[i], token);
		i++;
	}
	readLines = i - 1;
	for (k = 0; k < readLines; k++)
	{
		printf("%d - |%s|\n", k, reqArray[k]);
	}

	/* first line: method, path and protocol version */
	/* we copy to a temporary buffer to be more secure against overflows */
	i = j = 0;
	while (reqArray[0][i] != ' ' && reqArray[0][i] != '\0' && j < METHOD_LEN)
	{
		token[j++] = reqArray[0][i++];
	}
	token[j] = '\0';      /* to make sure we have a terminated string */
	strcpy(reqStruct->method, token);  /* copy back */
	if (reqArray[0][i] == '\0'){
		tokenEnd = YES;
	}   else{
		tokenEnd = NO;
	}
	i++;
	/* we look for the document address */
	j = 0;
	reqStruct->documentAddress[0] = '\0';
	if (!tokenEnd)   {
		while (reqArray[0][i] != ' ' && reqArray[0][i] != '\0' && reqArray[0][i] != '?' && j < MAX_PATH_LEN)
		{
			token[j++] = reqArray[0][i++];
		}
		token[j] = '\0';      /* to make sure we have a terminated string */
		/* now we need to convert some escapings from the path like %20 */
		DHTTPD_ConvertPercents(token, j);
		strcpy(reqStruct->documentAddress, token);  /* copy back */
		if (reqArray[0][i] == '\0'){
			tokenEnd = YES;
		} else{
			tokenEnd = NO;
		}
		i++;

		/* we need now to separate path from query string ("?" separated) */
		if (reqArray[0][i-1] == '?')    {
			k = 0;
			token[0] = '\0';
			while (reqArray[0][i] != ' ' && reqArray[0][i] != '?' && reqArray[0][i] != '\0' && k < MAX_QUERY_STRING_LEN)
			{
				token[k++] = reqArray[0][i++];
			}
			token[k] = '\0';      /* to make sure we have a terminated string */
			strcpy(reqStruct->queryString, token);  /* copy back */
			i++;
		}
	}
	/* we analyze the HTTP protocol version */
	/* default is 0.9 since that version didn't report itself */
	strcpy(reqStruct->protocolVersion, "HTTP/0.9");
	j = 0;
	if (!tokenEnd)    {
		while (reqArray[0][i] != ' ' && reqArray[0][i] != '\0' && j < PROTOCOL_LEN)
		{
			token[j++] = reqArray[0][i++];
		}
		token[j] = '\0';      /* to make sure we have a terminated string */
		if (j){
			strcpy(reqStruct->protocolVersion, token);  /* copy back */
		}
	}

	/* Connection type */
	reqStruct->keepAlive = NO;
	if (!strncmp(reqArray[1], "Connection: Keep-Alive", strlen("Connection: Keep-Alive"))){
		reqStruct->keepAlive = YES;
	} else if (!strncmp(reqArray[1], "Connection: keep-alive", strlen("Connection: keep-alive"))){
		reqStruct->keepAlive = YES;
	}

	/* user-agent, content-length and else */
	i = 1;
	j = NO;
	reqStruct->userAgent[0] = '\0';
	while (i < readLines)
	{
		if (!strncmp(reqArray[i], "User-Agent:", strlen("User-Agent:"))) {
			strncpy(reqStruct->userAgent, &reqArray[i][strlen("User-Agent: ")], USER_AGENT_LEN - 1);
			reqStruct->userAgent[USER_AGENT_LEN] = '\0';
		}else if (!strncmp(reqArray[i], "Content-Length:", strlen("Content-length:")) || \
			!strncmp(reqArray[i], "Content-length:", strlen("Content-length:")))	{
			strcpy(token, &reqArray[i][strlen("Content-length: ")]);
			sscanf(token, "%ld", &(reqStruct->contentLength));
			printf("content length %ld\n", reqStruct->contentLength);
		}
		i++;
	}
	/* if we didn't find a User-Agent we fill in a (N)ot(R)ecognized */
	if (reqStruct->userAgent[0] == '\0'){
		strcpy(reqStruct->userAgent, "NR");
	}

	return 0;
}


int DHTTPD_HandleMethod (port, sock, req)
/* analyze method of Request and take necessary actions */
int port;
int sock;
struct request req;
{
	char completeFilePath[MAX_PATH_LEN+MAX_PATH_LEN+MAX_INDEX_NAME_LEN+1]; /* to be sure root+path+indexname stay inside */
	char mimeType[MAX_MIMETYPE_LEN+1];
	struct stat fileStats;

	 /* now we check if the given path tries to get out of the root
	 * POSIX file name have / as a separator, so // is still a separator */
	 {
	 	register int i,j;
		register int sL;
		char         dirName[MAX_PATH_LEN+1];
		register int depthCount;

		depthCount = 0;
		sL = strlen(req.documentAddress);
		if (sL > 3)    {
			if (req.documentAddress[1] == '.' && req.documentAddress[2] == '.'   \
				&& req.documentAddress[3] == '/') {
				DHTTPD_HandleErrors(sock, FORBIDDEN, req.documentAddress, req);
				return -1;
			}
		}

		dirName[0] = '\0';
		j = 0;
		for (i = 1; i < sL; i++)
		{
			if (req.documentAddress[i] == '/'){
				dirName[j] = '\0';
				if (strcmp(dirName, "..")) {
					/* ignore ./ */
					if (strcmp(dirName, ".")){
						/* count only the first of multiple / spearators */
						if (req.documentAddress[i-1] != '/'){
							depthCount++;
						}
					}
				} else{
					depthCount--;
				}
				j = 0;
			} else{
				dirName[j++] = req.documentAddress[i];
			}
		}
		if (depthCount < 0) {
			DHTTPD_HandleErrors(sock, FORBIDDEN, req.documentAddress, req);
			return -1;
		}
	}

	 if (req.method[0]=='G' && req.method[1]=='E' \
	 	&& req.method[2]=='T' && req.method[3]=='\0')   {
	 	/* GET method */
		printf ("handling get of %s\n", req.documentAddress);
		/* first we check if the path contains the directory selected for cgi's and in case handle it */
		if (!strncmp(req.documentAddress, CGI_MATCH_STRING, strlen(CGI_MATCH_STRING))){
			cgiHandler(port, sock, req, NULL);
		} else { /* GET for standard files */
			/* we check that the path doesn't contain the cgi match string
			* we don't want to serve scripts as files */
			if (strstr(req.documentAddress, CGI_MATCH_STRING)) {
				DHTTPD_HandleErrors(sock, FORBIDDEN, req.documentAddress, req);
				return -1;
			}

			strcpy(completeFilePath, homePath);
			strcat(completeFilePath, req.documentAddress);

			/* now we check if the given file is a directory or a plain file */
			stat(completeFilePath, & fileStats);
			if ((fileStats.st_mode & S_IFDIR) == S_IFDIR) {
				/* if does not end with a slash, we get an error */
				if(completeFilePath[strlen(completeFilePath)-1] != '/') {
					DHTTPD_HandleErrors(sock, NOT_FOUND, req.documentAddress, req);
					return -1;
				}
				if (generateIndex(sock, completeFilePath, mimeType, req)) {
					/* we got an error, generateIndex was not able to handle the request itself
					* this means that there already exists and index
					* we append the default file name */
					strcat(completeFilePath, defaultFileName);
					analyzeExtension(mimeType, completeFilePath);
					dumpFile(sock, completeFilePath, mimeType, req);
				}
			} else { /* it is a plain file */
				analyzeExtension(mimeType, completeFilePath);
				dumpFile(sock, completeFilePath, mimeType, req);
			}
		}
	} else if (req.method[0]=='H' && req.method[1]=='E' && \
			req.method[2]=='A' && req.method[3]=='D' && req.method[4]=='\0')  {
			/* HEAD method */
			DBGPRINTF(("handling head of %s\n", req.documentAddress));
			/* first we check if the path contains the directory selected for cgi's and in case handle it */
			if (!strncmp(req.documentAddress, CGI_MATCH_STRING, strlen(CGI_MATCH_STRING)))   {
				cgiHandler(port, sock, req, NULL);
			} else    {
				strcpy(completeFilePath, homePath);
				strcat(completeFilePath, req.documentAddress);
				/* now we check if the given file is a directory or a plain file */
				stat(completeFilePath, &fileStats);
				if ((fileStats.st_mode & S_IFDIR) == S_IFDIR)  {
					/* if does not end with a slash, we get an error */
					if(completeFilePath[strlen(completeFilePath)-1] != '/')  {
						DHTTPD_HandleErrors(sock, NOT_FOUND, req.documentAddress, req);
						return -1;
					}
					/* we append the default file name */
					strcat(completeFilePath, defaultFileName);
				}
				analyzeExtension(mimeType, completeFilePath);
				dumpHeader(sock, completeFilePath, mimeType, req);
			}
		} else if (req.method[0]=='P' && req.method[1]=='O' \
		&& req.method[2]=='S' && req.method[3]=='T' && req.method[4]=='\0')   {
			/* POST method */
			/* we add 5 characters to be able to hold a \r\n\r\n\0 sequence at the end */
			char buff[POST_BUFFER_SIZE+5];
			int totalRead;
			int stuckCounter; /* if we receive too many errors */
			int readFinished;
			int ch;

			DBGPRINTF(("Handling of POST method\n"));
			/* first we check if the path contains the directory selected for cgi's and in case handle it */
			if (strncmp(req.documentAddress, CGI_MATCH_STRING, strlen(CGI_MATCH_STRING)))       {
				/* non cgi POST is not supported */
				DHTTPD_HandleErrors(sock, UNHANDLED_METHOD, "", req);
				return -1;
			}
			DBGPRINTF(("begin of post handling\n"));
			buff[0] = '\0';
			readFinished = NO;
			totalRead = 0;
			stuckCounter = 0;
			if (req.contentLength < 0)        {
				DHTTPD_HandleErrors(sock, LENGTH_REQUIRED, "", req);
				return -1;
			} else if (req.contentLength >= POST_BUFFER_SIZE)        {
				DHTTPD_HandleErrors(sock, BUFFER_OVERFLOW, "", req);
				return -1;
			}
			while (!readFinished)
			{
				ch = (char)fgetc(sockStream);
				if(ch == EOF)	    {
					perror("fgetc");
					if (errno == EAGAIN)	{
						clearerr(sockStream);
						printf("resource not available on POST data read\n");
						stuckCounter++;
						if (stuckCounter >= MAX_STUCK_COUNTER)    {
							printf("stuck in post data read\n");
							close(cs);
							newSocketReady = NO;
							readFinished = YES;
						}
					} else{
						perror("fgetc in post");
						printf("unrecoverable error\n");
						close(cs);
						newSocketReady = NO;
						readFinished = YES;
					}
				} else	    { 
					stuckCounter = 0;
					buff[totalRead] = ch; totalRead++;
					if (totalRead == req.contentLength){
						readFinished = YES;
					}
				}
			}
			clearerr(sockStream);
			DBGPRINTF(("total read %d\n", totalRead));
			if (totalRead == 0)      {
				printf("Request read error\n");
			} else   {
			if (buff[totalRead - 1] != '\n') /* we need a trailing \n or the script will wait forever */       {
				buff[totalRead++] = '\n';
				buff[totalRead] = '\0';
			}
			DBGPRINTF(("buff: |%s|\n", buff));
			cgiHandler(port, sock, req, buff);
		}
		/* end of POST */
	} else    {
		DHTTPD_HandleErrors(sock, UNHANDLED_METHOD, "", req);
		return -1;
	}

	return 0;
}



int DHTTPD_InitPara (serverPort, max_child, argc, argv)
/* initializes the operation parameters by reading them from the config file specified in the config file */
int *serverPort;
int *max_child;
int argc;
char *argv[];
{
	char configFile[MAX_PATH_LEN+1];
	char str1[BUFFER_SIZE+1];
	char str2[BUFFER_SIZE+1];
	FILE *f;

	strcpy(configFile, DEFAULT_CONFIG_LOCATION);
	strcat(configFile, CONFIG_FILE_NAME);
	if (argc > 0){
		printf("%s\n", *argv);  /* we shall insert here command-line arguments processing */
	}

	f = fopen(configFile, "r");
	if (f == NULL){
		printf("Error opening config file. Setting defaults.\n");
		*serverPort = DEFAULT_PORT;
		*max_child = DEFAULT_MAX_CHILDREN;
		strcpy(homePath, DEFAULT_DOCS_LOCATION);
		strcpy(defaultFileName, DEFAULT_FILE_NAME);
		s_tv.tv_sec = DEFAULT_SEC_TO;
		s_tv.tv_usec = DEFAULT_USEC_TO;
		strcpy(logFileName, DEFAULT_LOG_FILE);
		strcpy(mimeTypesFileName, DEFAULT_MIME_FILE);
		strcpy(cgiroot, DEFAULT_CGI_ROOT);
		return -1;
	}

	if (!feof(f)){
		fscanf(f, "%s %s", str1, str2);
	}

	*serverPort = 0;
	if (str1 != NULL && str2 != NULL && !strcmp(str1, "port")){
		sscanf(str2, "%d", serverPort);
	}

	if (*serverPort <= 0) {
		*serverPort = DEFAULT_PORT;
		printf("Error reading port from file, setting default, %d\n", *serverPort);
	}

	printf("port: %d\n", *serverPort);
	if (!feof(f)){
		fscanf(f, "%s %s", str1, str2);
	}

	*max_child = 0;
	if (str1 != NULL && str2 != NULL && !strcmp(str1, "max_child"))	{
		sscanf(str2, "%d", max_child);
	}

	if (*max_child <= 0) {
		*max_child = DEFAULT_MAX_CHILDREN;
		printf("Error reading max_child from file, setting default, %d\n", *max_child);
	}

	printf("max_child: %d", *max_child);
	printf("\n");
	if (!feof(f)){
		fscanf(f, "%s %s", str1, str2);
	}

	if (str1 != NULL && str2 != NULL && !strcmp(str1, "documentsPath")){
		sscanf(str2, "%s", homePath);
		if (homePath == NULL) {
			strcpy(homePath, DEFAULT_DOCS_LOCATION);
			printf("Error reading documentPath from file, setting default, %s\n", homePath);
		}
	} else  {
		strcpy(homePath, DEFAULT_DOCS_LOCATION);
		printf("Error reading documentPath from file, setting default, %s\n", homePath);
	}

	if (!feof(f)) {
		fscanf(f, "%s %s", str1, str2);
	}

	if (str1 != NULL && str2 != NULL && !strcmp(str1, "defaultFile"))   {
		sscanf(str2, "%s", defaultFileName);
		if (defaultFileName == NULL) {
			strcpy(defaultFileName, DEFAULT_FILE_NAME);
			printf("Error reading defaultFile from file, setting default, %s\n", defaultFileName);
		}
	} else    {
		strcpy(defaultFileName, DEFAULT_FILE_NAME);
		printf("Error reading defaultFile from file, setting default, %s\n", defaultFileName);
	}

	if (strlen(defaultFileName) > MAX_INDEX_NAME_LEN)    {
		printf("Error: the default file name is too long, exiting.\n");
		return -1;
	}

	if (!feof(f)){
		fscanf(f, "%s %s", str1, str2);
	}

	s_tv.tv_sec = -1;
	if (str1 != NULL && str2 != NULL && !strcmp(str1, "secTimeout")){
		sscanf(str2, "%ld", (long int *)&(s_tv.tv_sec));
	}

	if (s_tv.tv_sec < 0)    {
		s_tv.tv_sec = DEFAULT_SEC_TO;
		printf("Error reading secTimeout from file, setting default, %ld\n", (long int)s_tv.tv_sec);
	}

	printf("timeout sec: %ld, ", (long int)s_tv.tv_sec);
	if (!feof(f)) {
		fscanf(f, "%s %s", str1, str2);
	}

	s_tv.tv_usec = -1;
	if (str1 != NULL && str2 != NULL && !strcmp(str1, "uSecTimeout")){
		sscanf(str2, "%ld", (long int *)&(s_tv.tv_usec));
	}

	if (s_tv.tv_usec < 0){
		s_tv.tv_usec = DEFAULT_USEC_TO;
		printf("Error reading usecTimeout from file, setting default, %ld\n", (long int)s_tv.tv_usec);
	}

	printf("usec: %ld\n", (long int)s_tv.tv_usec);
	if (!feof(f)){
		fscanf(f, "%s %s", str1, str2);
	}

	if (str1 != NULL && str2 != NULL && !strcmp(str1, "logFile"))	{
		sscanf(str2, "%s", logFileName);
		if (logFileName == NULL)  {
			strcpy(logFileName, DEFAULT_LOG_FILE);
			printf("Error reading logFile from file, setting default, %s\n", logFileName);
		}
	} else   {
		strcpy(logFileName, DEFAULT_LOG_FILE);
		printf("Error reading logFile from file, setting default, %s\n", logFileName);
	}

	if (!feof(f)){
		fscanf(f, "%s %s", str1, str2);
	}

	if (str1 != NULL && str2 != NULL && !strcmp(str1, "mimeTypesFile"))    {
		sscanf(str2, "%s", mimeTypesFileName);
		if (mimeTypesFileName == NULL)       {
			strcpy(mimeTypesFileName, DEFAULT_MIME_FILE);
			printf("Error reading mimeTypesFileName from file, setting default, %s\n", mimeTypesFileName);
		}
	} else {
		strcpy(mimeTypesFileName, DEFAULT_MIME_FILE);
		printf("Error reading mimeTypesFileName from file, setting default, %s\n", mimeTypesFileName);
	}

	if (!feof(f)){
		fscanf(f, "%s %s", str1, str2);
	}

	if (str1 != NULL && str2 != NULL && !strcmp(str1, "cgiroot"))    {
		sscanf(str2, "%s", cgiroot);
		if (cgiroot == NULL)  {
			strcpy(cgiroot, DEFAULT_CGI_ROOT);
			printf("Error reading cgiroot from file, setting default, %s\n", cgiroot);
		}
	} else    {
		strcpy(cgiroot, DEFAULT_CGI_ROOT);
		printf("Error reading cgiroot from file, setting default, %s\n", cgiroot);
	}

	fclose(f);
	initMimeTypes();

	return 0;
}

int main (int argc, char *argv[])
/* contains the main connection accept loop which calls analyzers and handlers */
{
	int                port;                 /* 服务器侦听的端口 */
	int                max_child;          /* 可并行处理的客户端数量*/
	char               buff[BUFFER_SIZE+1];
	int                totalRead;
	int                stuckCounter;         /* to behold read progress and catch nasty loops */
	int                readFinished;
	struct request     gottenReq;
	int                isKeepAlive;
	struct sockaddr_in serv_addr;           /* 服务器地址 */
	struct sockaddr_in clie_addr;   /* 客户端连接地址 */
	size_t        addr_len;    /* 地址结构长度 */
	int                sockReuse;
	int                c_pid;                  /* 子进程PID */
	int                runningChildren;

	DHTTPD_InitPara(&port, &max_child, argc, argv); /* 从配置文件读取设置 */

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr=INADDR_ANY;
	serv_addr.sin_port = htons(port);
	addr_len = sizeof(clie_addr);

	/* 挂接信号处理函数*/
	signal(SIGINT,  sig_int);
	signal(SIGPIPE, sig_pipe);
	if (logFileOpen()) {
		printf("log file creation error or other misconfiguration\n");
		exit(0);
	}

	/* socket初始化 */
	ss = socket (AF_INET, SOCK_STREAM, 0);
	if (ss == -1)    {
		printf("socket creation error occoured\n");
		return -1;
	}

	/* 地址复用 */
	sockReuse = YES;
	error = setsockopt (ss, SOL_SOCKET, SO_REUSEADDR, &sockReuse, sizeof(sockReuse));
	if (error == -1)    {
		printf("socket reuse option setting failed\n");
	}

	/* 绑定 */
	error = bind (ss, (struct sockaddr*)  &serv_addr, sizeof(serv_addr));
	if (error == -1)   {
		printf("socket binding error occoured\n");
		return -2;
	}

	/* 侦听 */
	error = listen(ss, BACK_LOG);
	if (error)    {
		printf ("listen error\n");
		return -1;
	}

	/* 以下为主处理过程 */
	isKeepAlive = NO;
	runningChildren = 0;
	while (1)
	{
		DBGPRINTF(("listen\n"));
		cs = accept (ss, (struct sockaddr *) &clie_addr, &addr_len);
		c_pid = fork();
		if (c_pid)	{
			/* 父进程 */
			int exitResult;
			int i;

			if (c_pid < 0)   {
				/* fork出错*/
				printf ("A forking error occoured!\n");
			}
			runningChildren++;
			newSocketReady = NO;
			close(cs);
			/* 检查子进程的数量和状态*/
			/* 如果子进程过多，阻塞并等待进程结束*/
			if (runningChildren >= max_child)	{
				wait(&exitResult);
				DBGPRINTF(("Child exited with: %d\n", exitResult));
				runningChildren--;
			}
			/* 非阻塞快速退出 */
			for (i = 0; i < runningChildren; i++);
			{
				if (waitpid(-1, &exitResult, WNOHANG) > 0)  {
					DBGPRINTF(("Child exited with: %d\n", exitResult));
					runningChildren--;
				}
			}
		} else	{
			/* 子进程 */
			if (cs == -1) {
				newSocketReady = NO;
				printf("error accepting\n");
			} else {
				strcpy(gottenReq.address, inet_ntoa(clie_addr.sin_addr));
				DBGPRINTF(("accepted from %s\n", gottenReq.address));
				newSocketReady = YES;
				/* 设置超时时间 */
				error = setsockopt (cs, SOL_SOCKET, SO_RCVTIMEO, &s_tv, sizeof(s_tv));
				if(error){
					perror("setsockopt: ");
				}
				/* 复制套接字描述符 */
				sockStream = fdopen(cs, "r+");

				if (sockStream != NULL){
					int ch;
					buff[0] = '\0';
					readFinished = NO;
					stuckCounter = 0;
					totalRead = 0;

					while (!readFinished)
					{
						ch = fgetc(sockStream);
						if (ch == EOF){
							if (errno == EAGAIN){
								DBGPRINTF(("resource not available on header read\n"));
								clearerr(sockStream);
								stuckCounter++;
								if (stuckCounter >= MAX_STUCK_COUNTER){
									DBGPRINTF(("Loop in read catched! closing connection.\n"));
									if (newSocketReady) {
										DBGPRINTF(("new socket was ready....\n"));
										close(cs);
									}
									newSocketReady = NO;
									readFinished = YES;
								}
							} else    {
								DBGPRINTF(("read error: %d\n", errno));
								newSocketReady = NO;
								readFinished = YES;
								close(cs);
							}
						} else	{
							/* 最少读取一个字符 */	
							buff[totalRead] = (char)ch; totalRead++;
							/* 头部字符串以两个回车换行符结束 */
							if (totalRead > 2)   {
								if (totalRead >= BUFFER_SIZE) /* 检查缓冲区溢出 */ 	{
									DBGPRINTF(("Buffer overflow on request read\n"));
									DHTTPD_HandleErrors(cs, INPUT_LINE_TOO_LONG, "", gottenReq);
									readFinished = YES;
								} else if (buff[totalRead-1] == '\n' && buff[totalRead-3] == '\n')	{
									buff[totalRead] = '\0';
									readFinished = YES;
								}
							}
						}
					} /* 读取头部数据结束 */
					if (totalRead <= 0)  {
						DBGPRINTF(("Request read error\n"));
					} else if (buff[totalRead-1] != '\n' && buff[0]=='P' && buff[1]=='O' \
						&& buff[2]=='S' &&  buff[3]=='T' && buff[4]=='\0'){
						/* POST method */
						/* POST不像GET一样以换行符结束*/
						DBGPRINTF(("Unterminated request header\n"));
						DHTTPD_HandleErrors(cs, INPUT_LINE_TOO_LONG, "", gottenReq);
					} else {
						DHTTPD_ParseRequest(cs, buff, &gottenReq);
						error = setsockopt (cs, SOL_SOCKET, SO_SNDTIMEO, &s_tv, sizeof(s_tv));
						DHTTPD_HandleMethod(port, cs, gottenReq);
						
					}
				} /* sockStream != NULL */

				if (close(cs)){
					DBGPRINTF(("error closing socket after connection\n"));
				} else{
					newSocketReady = NO;
				}
			}
			exit(0);
		} /* end of fork if */
	} /* end of listen error if */

	close(ss); /* if we shall ever exit the infinite loop... */
}
