#define _WIN32_WINNT 0x0A00

#include <windows.h>
#include <winioctl.h>
#include <stdio.h>

// Callback triggered upon oplock break; holds the lock until ENTER is pressed
void HandleOplock(void) {
    printf("OpLock triggered, hit ENTER to close oplock\n");
    getchar();
}

int wmain(int argc, wchar_t* argv[]) {
    if (argc < 2) {
        printf("Usage: SetOpLock target [rwdx]\n");
        printf("Share Mode:\n");
        printf("r - FILE_SHARE_READ\n");
        printf("w - FILE_SHARE_WRITE\n");
        printf("d - FILE_SHARE_DELETE\n");
        printf("x - Exclusive lock (FSCTL_REQUEST_OPLOCK_LEVEL_1)\n");
        return 1;
    }

    const wchar_t* target = argv[1];
    const wchar_t* share_mode_str = argc > 2 ? argv[2] : L"";

    // Parse share mode flags (r, w, d) and exclusive lock option (x)
    DWORD share_mode = 0;
    BOOL exclusive = FALSE;

    for (int i = 0; share_mode_str[i] != L'\0'; i++) {
        switch (share_mode_str[i]) {
            case L'r':
                share_mode |= FILE_SHARE_READ;
                break;
            case L'w':
                share_mode |= FILE_SHARE_WRITE;
                break;
            case L'd':
                share_mode |= FILE_SHARE_DELETE;
                break;
            case L'x':
                exclusive = TRUE;
                break;
        }
    }

    // Open target file asynchronously without following reparse points
    HANDLE hFile = CreateFileW(
        target,
        GENERIC_READ,
        share_mode,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED | FILE_FLAG_OPEN_REPARSE_POINT,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        printf("Error opening file: %lu\n", GetLastError());
        return 1;
    }

    // Initialize OVERLAPPED structure and manual-reset event for async I/O
    OVERLAPPED overlapped = { 0 };
    overlapped.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (overlapped.hEvent == NULL) {
        printf("Error creating event: %lu\n", GetLastError());
        CloseHandle(hFile);
        return 1;
    }

    DWORD bytesReturned = 0;
    BOOL result = FALSE;

    // Request either an exclusive Level 1 oplock or a standard granular oplock
    if (exclusive) {
        result = DeviceIoControl(
            hFile,
            FSCTL_REQUEST_OPLOCK_LEVEL_1,
            NULL, 0,
            NULL, 0,
            &bytesReturned,
            &overlapped
        );
    } else {
        REQUEST_OPLOCK_INPUT_BUFFER inputBuffer = { 0 };
        inputBuffer.StructureVersion = REQUEST_OPLOCK_CURRENT_VERSION;
        inputBuffer.StructureLength = sizeof(inputBuffer);
        inputBuffer.RequestedOplockLevel = OPLOCK_LEVEL_CACHE_READ | OPLOCK_LEVEL_CACHE_HANDLE;
        inputBuffer.Flags = REQUEST_OPLOCK_INPUT_FLAG_REQUEST;

        REQUEST_OPLOCK_OUTPUT_BUFFER outputBuffer = { 0 };
        outputBuffer.StructureVersion = REQUEST_OPLOCK_CURRENT_VERSION;
        outputBuffer.StructureLength = sizeof(outputBuffer);

        result = DeviceIoControl(
            hFile,
            FSCTL_REQUEST_OPLOCK,
            &inputBuffer, sizeof(inputBuffer),
            &outputBuffer, sizeof(outputBuffer),
            &bytesReturned,
            &overlapped
        );
    }

    // ERROR_IO_PENDING confirms the oplock is active and waiting for a break
    if (!result && GetLastError() == ERROR_IO_PENDING) {
        wprintf(L"OpLock set on %s. Waiting for trigger...\n", target);
        WaitForSingleObject(overlapped.hEvent, INFINITE);
        HandleOplock();
    } else {
        printf("Error creating oplock: %lu\n", GetLastError());
        CloseHandle(overlapped.hEvent);
        CloseHandle(hFile);
        return 1;
    }

    // Clean up handles and release the file
    CloseHandle(overlapped.hEvent);
    CloseHandle(hFile);

    return 0;
}
