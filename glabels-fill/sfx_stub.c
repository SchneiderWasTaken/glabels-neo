/*
 * glabels-fill SFX launcher stub.
 *
 * This stub is compiled and then a payload (the real glabels-fill.exe +
 * its dependency DLLs) is APPENDED after a magic trailer.  At runtime the
 * stub extracts the payload to %TEMP%\glabels-fill-<pid>\ and runs the
 * inner executable.  When the inner exe exits, the temp dir is cleaned up.
 *
 * A FillConfig trailer ("GLBLFILL") may be appended AFTER the SFX trailer
 * by the Deploy tab.  The stub handles this by scanning backwards for its
 * own "GLBLSFX1" magic rather than assuming it's at the very end.
 *
 * SFX payload format (all little-endian):
 *   repeat:
 *     uint16  name_len
 *     bytes   name (UTF-8, no path separators -- flat extraction)
 *     uint32  data_len
 *     bytes   data
 *   uint32  entry_count        (at the very end of the SFX trailer)
 *   uint32  payload_size       (total bytes of the entries above)
 *   char[8] magic "GLBLSFX1"
 *
 * Build:  gcc -static -O2 -s -o glabels-fill-sfx.exe sfx_stub.c -lshlwapi
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAGIC "GLBLSFX1"
#define MAGIC_LEN 8
#define TRAILER_LEN (4 + 4 + MAGIC_LEN)  /* entry_count + payload_size + magic */
#define SCAN_BACK 65536   /* how far back to look for our magic */

static int read_all(HANDLE f, void *buf, DWORD len) {
    DWORD got = 0, total = 0;
    while (total < len) {
        if (!ReadFile(f, (char*)buf + total, len - total, &got, NULL) || got == 0)
            return 0;
        total += got;
    }
    return 1;
}

int WINAPI WinMain(HINSTANCE hI, HINSTANCE hP, LPSTR cmd, int show) {
    char self_path[MAX_PATH];
    GetModuleFileNameA(NULL, self_path, MAX_PATH);

    HANDLE f = CreateFileA(self_path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return 1;

    LARGE_INTEGER fsize;
    if (!GetFileSizeEx(f, &fsize)) { CloseHandle(f); return 1; }

    /* Scan backwards for our magic.  A FillConfig trailer may have been
     * appended after ours by the Deploy tab, so we can't assume we're at
     * the very end.  Search the last SCAN_BACK bytes. */
    DWORD scan_len = (DWORD)(fsize.QuadPart < SCAN_BACK ? fsize.QuadPart : SCAN_BACK);
    char *scan_buf = (char*)malloc(scan_len);
    if (!scan_buf) { CloseHandle(f); return 1; }

    SetFilePointer(f, (LONG)(fsize.QuadPart - scan_len), NULL, FILE_BEGIN);
    if (!read_all(f, scan_buf, scan_len)) {
        free(scan_buf); CloseHandle(f); return 1;
    }

    /* Find the LAST occurrence of MAGIC in the scan window. */
    LONGLONG magic_off = -1;
    for (long i = scan_len - MAGIC_LEN; i >= 0; i--) {
        if (memcmp(scan_buf + i, MAGIC, MAGIC_LEN) == 0) {
            magic_off = (LONGLONG)(fsize.QuadPart - scan_len + i);
            break;
        }
    }
    free(scan_buf);

    if (magic_off < 0) {
        /* No SFX payload -- nothing to extract. */
        CloseHandle(f); return 1;
    }

    /* Read the SFX trailer (entry_count + payload_size are right before magic). */
    SetFilePointer(f, (LONG)(magic_off - 8), NULL, FILE_BEGIN);
    DWORD entry_count, payload_size;
    if (!read_all(f, &entry_count, 4) ||
        !read_all(f, &payload_size, 4)) {
        CloseHandle(f); return 1;
    }

    /* Seek to payload start. */
    LONGLONG payload_off = magic_off - 8 - payload_size;
    SetFilePointer(f, (LONG)payload_off, NULL, FILE_BEGIN);

    /* Create temp dir. */
    char tmpdir[MAX_PATH];
    GetTempPathA(MAX_PATH, tmpdir);
    char destdir[MAX_PATH];
    snprintf(destdir, MAX_PATH, "%sglabels-fill-%lu", tmpdir, GetCurrentProcessId());
    CreateDirectoryA(destdir, NULL);

    /* Extract entries. */
    char exe_path[MAX_PATH] = {0};
    for (DWORD i = 0; i < entry_count; i++) {
        uint16_t name_len;
        if (!read_all(f, &name_len, 2)) break;
        char name[1024];
        if (!read_all(f, name, name_len)) break;
        name[name_len] = 0;

        uint32_t data_len;
        if (!read_all(f, &data_len, 4)) break;

        char out_path[MAX_PATH];
        snprintf(out_path, MAX_PATH, "%s\\%s", destdir, name);

        HANDLE of = CreateFileA(out_path, GENERIC_WRITE, 0, NULL,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (of == INVALID_HANDLE_VALUE) continue;

        char buf[65536];
        DWORD remaining = data_len;
        while (remaining > 0) {
            DWORD chunk = remaining > sizeof(buf) ? sizeof(buf) : remaining;
            DWORD got = 0;
            if (!ReadFile(f, buf, chunk, &got, NULL) || got != chunk) break;
            DWORD written = 0;
            WriteFile(of, buf, chunk, &written, NULL);
            remaining -= chunk;
        }
        CloseHandle(of);

        /* Remember the inner exe path. */
        if (_stricmp(name, "glabels-fill.exe") == 0) {
            strncpy(exe_path, out_path, MAX_PATH - 1);
        }
    }
    CloseHandle(f);

    if (exe_path[0] == 0) {
        MessageBoxA(NULL, "Inner executable not found in payload.", "glabels-fill SFX", MB_ICONERROR);
        return 1;
    }

    /* Run the inner exe and wait.  Pass our own path so the inner exe can
     * find its config trailer on the SFX (not the extracted temp copy). */
    SetEnvironmentVariableA("GLABELS_FILL_SFX_PATH", self_path);

    char cmdline[32768];
    snprintf(cmdline, sizeof(cmdline), "\"%s\" %s", exe_path, cmd ? cmd : "");

    STARTUPINFOA si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = show;
    PROCESS_INFORMATION pi = {0};

    if (CreateProcessA(exe_path, cmdline, NULL, NULL, FALSE, 0, NULL, destdir, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    /* Clean up temp dir (best effort). */
    char delcmd[MAX_PATH + 32];
    snprintf(delcmd, sizeof(delcmd), "cmd /c rmdir /s /q \"%s\"", destdir);
    WinExec(delcmd, SW_HIDE);

    return 0;
}
