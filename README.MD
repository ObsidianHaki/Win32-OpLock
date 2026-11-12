# SetOpLock (Pure C Port)

A lightweight, standalone C implementation of James Forshaw's `SetOpLock` utility (originally from Google Project Zero's `symboliclink-testing-tools`). This tool sets an **Opportunistic Lock (OpLock)** on a target file via the Win32 API to pause execution of other processes attempting to access the file.

## How It Works (Sequence Flow)

```text
+---------------+                   +------------------+                   +----------------+

| SetOpLock.exe |                   |  Windows Kernel  |                   | Target Process |
| (User Mode)   |                   | (NTFS / SMB I/O) |                   | (e.g. Notepad) |
+-------+-------+                   +--------+---------+                   +-------+--------+

        |                                    |                                     |
        | 1. CreateFileW(OVERLAPPED)         |                                     |
        |----------------------------------->|                                     |
        |    Returns File Handle (hFile)     |                                     |
        |<-----------------------------------|                                     |
        |                                    |                                     |
        | 2. DeviceIoControl(FSCTL_REQUEST)  |                                     |
        |----------------------------------->|                                     |
        |    ERROR_IO_PENDING (OpLock Active)|                                     |
        |<-----------------------------------|                                     |
        |                                    |                                     |
        | 3. WaitForSingleObject(hEvent)     |                                     |
        |    [Process sleeps / 0% CPU]       |                                     |
        |                                    |       4. Attempts to open file      |
        |                                    |<------------------------------------|
        |                                    |                                     |
        |    5. Signals hEvent (OpLock Break)|       [FREEZES / BLOCKS ACCESS]     |
        |<-----------------------------------|       (Waits for OpLock release)    |
        |                                    |                                     |
        | 6. HandleOplock() -> getchar()     |                                     |
        |    [Holds lock until user input]   |                                     |
        |                                    |                                     |
        | 7. User presses ENTER              |                                     |
        |    CloseHandle(hFile)              |                                     |
        |----------------------------------->|                                     |
        |                                    |       8. Access granted / Resumes   |
        |                                    |------------------------------------>|
+-------+-------+                   +--------+---------+                   +-------+--------+
```

## Compilation

### Cross-Compiling on Linux (MinGW-w64)
```bash
x86_64-w64-mingw32-gcc SetOpLock.c -o SetOpLock.exe -municode
```

### Compiling on Windows (MSVC)
```cmd
cl SetOpLock.c
```

## Usage

```cmd
SetOpLock.exe <target_file> [rwdx]
```

### Share Mode Flags
* `r` — `FILE_SHARE_READ` (allow other processes to request read access)
* `w` — `FILE_SHARE_WRITE` (allow other processes to request write access)
* `d` — `FILE_SHARE_DELETE` (allow other processes to request delete access)
* `x` — Exclusive lock (`FSCTL_REQUEST_OPLOCK_LEVEL_1`, breaks on any access attempt)

### Example
Set an exclusive lock while permitting other processes to initiate a read/write/delete request (which triggers the freeze):
```cmd
SetOpLock.exe C:\Users\User\Desktop\test.txt rwdx
```

## Disclaimer & Credits

* **Educational & Research Purpose Only:** Intended for understanding Windows Internals, Win32 asynchronous I/O, and file system behavior in isolated test environments.
* **Credits:** Based on the original `SetOpLock` by James Forshaw / Google Inc., licensed under the **Apache License 2.0**.
