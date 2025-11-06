/*
 * Binary wrapper for Zig compiler with sccache support
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define MAX_CMD_LEN 32768

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Error: First argument must be 'cc' or 'cxx'\n");
        return 1;
    }

    const char *compiler_type = argv[1];
    const char *inner_wrapper;
    
    if (strcmp(compiler_type, "cc") == 0) {
        inner_wrapper = "zig-cc.py";
    } else if (strcmp(compiler_type, "cxx") == 0) {
        inner_wrapper = "zig-cxx.py";
    } else {
        fprintf(stderr, "Error: First argument must be 'cc' or 'cxx'\n");
        return 1;
    }

    char exe_path[MAX_PATH];
    char script_dir[MAX_PATH];
    GetModuleFileNameA(NULL, exe_path, MAX_PATH);
    
    char *last_backslash = strrchr(exe_path, 92); // 92 is backslash
    if (last_backslash) {
        *last_backslash = 0;
        strcpy(script_dir, exe_path);
    } else {
        strcpy(script_dir, ".");
    }

    char python_exe[MAX_PATH];
    char inner_wrapper_path[MAX_PATH];
    sprintf(python_exe, "%s%c..%c.venv%cScripts%cpython.exe", script_dir, 92, 92, 92, 92);
    sprintf(inner_wrapper_path, "%s%c%s", script_dir, 92, inner_wrapper);

    char cmdline[MAX_CMD_LEN];
    int offset = sprintf(cmdline, "\"%s\" \"%s\"", python_exe, inner_wrapper_path);
    
    for (int i = 2; i < argc; i++) {
        if (strchr(argv[i], ' ')) {
            offset += sprintf(cmdline + offset, " \"%s\"", argv[i]);
        } else {
            offset += sprintf(cmdline + offset, " %s", argv[i]);
        }
    }

    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    
    if (!CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        return 1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return (int)exit_code;
}
