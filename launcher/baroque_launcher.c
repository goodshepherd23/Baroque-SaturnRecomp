/* Baroque.exe -- launcher for the Baroque (SaturnRecomp) kit.
 *
 * Player data (saves, settings, the BIOS copy) and the build cache live in
 * %LOCALAPPDATA%\Baroque-SaturnRecomp, outside the app folder, so a launcher
 * update that replaces the app folder cannot wipe them. A file named
 * portable.txt beside Baroque.exe keeps everything in the app folder instead.
 *
 * 0. Moves what v0.1.1 and older kept in the app folder to the data folder,
 *    never replacing anything already there.
 * 1. Runs kit\setup.ps1 when the runtime has not been built for this runtime
 *    version yet, or the BIOS/disc are not set up (it asks for them).
 * 2. Starts runtime\saturnwin.exe on game\game.toml with this kit's settings,
 *    on the fastest CPU cores, at high priority, with the window focused.
 *    [Audio] AudioBuffer in settings.ini sets the sound lead (default 2048).
 * 3. While the game runs, switches Windows' power mode to "Best performance"
 *    for the power source in use, and puts the original back afterwards.
 *
 * With SATURN_VK_HIDDEN set (automated tests) the game window stays hidden,
 * setup runs without a console, errors go only to launcher.log, and the power
 * mode is left alone.
 *
 * Usage: Baroque.exe            play (sets up first if needed)
 *        Baroque.exe --setup    re-run setup (choose another disc/BIOS)
 *        Baroque.exe --rebuild  rebuild the runtime from source
 * MIT licensed, see LICENSE. */
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

/* -DKIT_VER=0.1.0 -DRUNTIME_VER=0.1.0 (bare tokens, so no shell quoting is
 * involved). RUNTIME_VER only changes when the patched runtime does, so a
 * launcher-only update does not trigger a rebuild. */
#define KIT_STR_(x) #x
#define KIT_STR(x) KIT_STR_(x)
#define KIT_WIDE_(s) L##s
#define KIT_WIDE(s) KIT_WIDE_(s)
#ifdef KIT_VER
#define KIT_VERSION KIT_WIDE(KIT_STR(KIT_VER))
#else
#define KIT_VERSION L"dev"
#endif
#ifdef RUNTIME_VER
#define RUNTIME_VERSION KIT_WIDE(KIT_STR(RUNTIME_VER))
#else
#define RUNTIME_VERSION KIT_VERSION
#endif
#define PATHMAX 32768

static wchar_t root[PATHMAX];   /* app folder: Baroque.exe, kit\ */
static wchar_t data[PATHMAX];   /* data folder: game\, bios\, settings.ini, runtime\, tools\, build\ */
static int quiet;

static void path(wchar_t *out, const wchar_t *rel) { swprintf(out, PATHMAX, L"%ls\\%ls", data, rel); }
static int is_file(const wchar_t *p) { return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES; }
static int exists(const wchar_t *rel) { wchar_t p[PATHMAX]; path(p, rel); return is_file(p); }

static void note(const wchar_t *msg) {   /* appends to launcher.log in the data folder */
    wchar_t p[PATHMAX]; path(p, L"launcher.log");
    FILE *f = _wfopen(p, L"a, ccs=UTF-8"); if (!f) return;
    SYSTEMTIME t; GetLocalTime(&t);
    fwprintf(f, L"%04u-%02u-%02u %02u:%02u:%02u %ls\n", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, msg);
    fclose(f);
}
static void fail(const wchar_t *msg) { note(msg); if (!quiet) MessageBoxW(NULL, msg, L"Baroque", MB_ICONERROR); }

/* ---------------------------------------------------------- data folder */
static void find_data(void) {
    wchar_t p[PATHMAX], local[PATHMAX];
    swprintf(p, PATHMAX, L"%ls\\portable.txt", root);
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", local, PATHMAX);
    if (is_file(p) || !n || n >= PATHMAX) { wcscpy(data, root); return; }
    swprintf(data, PATHMAX, L"%ls\\Baroque-SaturnRecomp", local);
    if (!CreateDirectoryW(data, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) wcscpy(data, root);
}

/* v0.1.1 and older kept everything in the app folder. Move it over; anything
 * already in the data folder wins and the old copy is left where it is. The
 * build cache only moves within one drive (it is downloaded again otherwise). */
static void migrate(void) {
    static const wchar_t *items[] = {
        L"game\\backup.bin", L"game\\cheats.txt", L"game\\console.bin", L"game\\game.toml",
        L"settings.ini", L"bios\\saturn.bin", L"power-restore.txt", L"tools", L"build", L"runtime" };
    static wchar_t from[PATHMAX], to[PATHMAX], parent[PATHMAX], msg[2 * PATHMAX + 128];
    if (!_wcsicmp(root, data)) return;
    for (size_t i = 0; i < sizeof items / sizeof *items; i++) {
        swprintf(from, PATHMAX, L"%ls\\%ls", root, items[i]); path(to, items[i]);
        DWORD a = GetFileAttributesW(from);
        if (a == INVALID_FILE_ATTRIBUTES) continue;
        if (is_file(to)) {
            swprintf(msg, sizeof msg / sizeof *msg, L"migrate: kept %ls (%ls already exists)", from, to);
            note(msg); continue;
        }
        wcscpy(parent, to); *wcsrchr(parent, L'\\') = 0;
        CreateDirectoryW(parent, NULL);
        BOOL ok = MoveFileExW(from, to, (a & FILE_ATTRIBUTE_DIRECTORY) ? 0 : MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH);
        swprintf(msg, sizeof msg / sizeof *msg, ok ? L"migrate: moved %ls to %ls" : L"migrate: could not move %ls to %ls (error %lu)",
                 from, to, GetLastError());
        note(msg);
    }
}

static int runtime_current(void) {
    wchar_t p[PATHMAX], v[64] = L"";
    path(p, L"runtime\\.kit-version");
    FILE *f = _wfopen(p, L"r");
    if (!f) return 0;
    int ok = fwscanf(f, L"%63ls", v) == 1 && !wcscmp(v, RUNTIME_VERSION);
    fclose(f);
    return ok && exists(L"runtime\\saturnwin.exe");
}

static int run_setup(const wchar_t *extra) {
    static wchar_t cmd[3 * PATHMAX];
    STARTUPINFOW si = {0}; PROCESS_INFORMATION pi = {0}; si.cb = sizeof si;
    swprintf(cmd, sizeof cmd / sizeof *cmd,
        L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"%ls\\kit\\setup.ps1\" -Root \"%ls\" -Data \"%ls\" %ls",
        root, root, data, extra);
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, quiet ? CREATE_NO_WINDOW : CREATE_NEW_CONSOLE, NULL, root, &si, &pi)) {
        fail(L"Could not start the setup (PowerShell is required)."); return 0;
    }
    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1; GetExitCodeProcess(pi.hProcess, &code); CloseHandle(pi.hProcess);
    if (code) {
        swprintf(cmd, PATHMAX, L"Setup did not finish. The details are in\n%ls\\setup.log\n\nRun Baroque again to retry.", data);
        fail(cmd);
    }
    return code == 0;
}

/* ---------------------------------------------------------- CPU placement */
/* The Saturn CPUs are interpreted on one host thread, so it must stay on the
 * fastest cores: those in the highest efficiency class (all cores on a CPU
 * without hybrid cores). Also opt the game out of Windows' power throttling. */
static void place_on_fast_cores(HANDLE process) {
    ULONG len = 0; DWORD_PTR mask = 0; BYTE best = 0;
    GetSystemCpuSetInformation(NULL, 0, &len, process, 0);
    BYTE *buf = len ? HeapAlloc(GetProcessHeap(), 0, len) : NULL;
    if (buf && GetSystemCpuSetInformation((PSYSTEM_CPU_SET_INFORMATION)buf, len, &len, process, 0)) {
        for (int pass = 0; pass < 2; pass++)
            for (ULONG off = 0; off < len; ) {
                PSYSTEM_CPU_SET_INFORMATION c = (PSYSTEM_CPU_SET_INFORMATION)(buf + off);
                if (c->Type == CpuSetInformation && c->CpuSet.Group == 0 &&
                    c->CpuSet.LogicalProcessorIndex < sizeof(DWORD_PTR) * 8) {
                    if (pass == 0) { if (c->CpuSet.EfficiencyClass > best) best = c->CpuSet.EfficiencyClass; }
                    else if (c->CpuSet.EfficiencyClass == best) mask |= (DWORD_PTR)1 << c->CpuSet.LogicalProcessorIndex;
                }
                off += c->Size;
            }
    }
    if (buf) HeapFree(GetProcessHeap(), 0, buf);
    if (mask) SetProcessAffinityMask(process, mask);
    SetPriorityClass(process, HIGH_PRIORITY_CLASS);
    PROCESS_POWER_THROTTLING_STATE t = { PROCESS_POWER_THROTTLING_CURRENT_VERSION,
                                          PROCESS_POWER_THROTTLING_EXECUTION_SPEED, 0 };
    SetProcessInformation(process, ProcessPowerThrottling, &t, sizeof t);
}

/* ---------------------------------------------------------- power mode */
/* Windows' power-mode overlay is per power source and powercfg can only set
 * the one in use. Originals are kept in power-restore.txt ("AC <guid>" /
 * "DC <guid>") until put back; one that cannot be restored at exit (the other
 * source) is left to a detached "--restore-power" copy of this program. */
#define BEST_PERFORMANCE L"ded574b5-45a0-4f42-8737-46345c09c238"
enum { AC, DC };
typedef struct { int pending[2]; wchar_t guid[2][64]; } power_state;

static int power_source(void) { SYSTEM_POWER_STATUS s; return GetSystemPowerStatus(&s) && s.ACLineStatus == 0 ? DC : AC; }
static int overlay(int src, wchar_t *out, DWORD cap) {
    DWORD bytes = cap * sizeof(wchar_t);
    return RegGetValueW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Power\\User\\PowerSchemes",
                        src == DC ? L"ActiveOverlayDcPowerScheme" : L"ActiveOverlayAcPowerScheme",
                        RRF_RT_REG_SZ, NULL, out, &bytes) == ERROR_SUCCESS;
}
static void set_overlay(const wchar_t *guid) {
    wchar_t cmd[128]; STARTUPINFOW si = {0}; PROCESS_INFORMATION pi = {0}; si.cb = sizeof si;
    swprintf(cmd, 128, L"powercfg.exe /overlaysetactive %ls", guid);
    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 10000); CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    }
}
static void load_power(power_state *st) {
    wchar_t p[PATHMAX], key[64], guid[64]; FILE *f;
    memset(st, 0, sizeof *st); path(p, L"power-restore.txt");
    if (!(f = _wfopen(p, L"r"))) return;
    while (fwscanf(f, L"%63ls %63ls", key, guid) == 2) {
        int src = !_wcsicmp(key, L"AC") ? AC : !_wcsicmp(key, L"DC") ? DC : -1;
        if (src >= 0 && wcslen(guid) == 36) { wcscpy(st->guid[src], guid); st->pending[src] = 1; }
    }
    fclose(f);
}
static void save_power(const power_state *st) {
    wchar_t p[PATHMAX]; path(p, L"power-restore.txt");
    if (!st->pending[AC] && !st->pending[DC]) { DeleteFileW(p); return; }
    FILE *f = _wfopen(p, L"w"); if (!f) return;
    for (int s = AC; s <= DC; s++) if (st->pending[s]) fwprintf(f, L"%ls %ls\n", s == AC ? L"AC" : L"DC", st->guid[s]);
    fclose(f);
}
static void boost_power(void) {
    power_state st; wchar_t cur[64]; int src = power_source();
    if (!overlay(src, cur, 64) || !_wcsicmp(cur, BEST_PERFORMANCE)) return;
    load_power(&st);
    if (!st.pending[src]) { wcscpy(st.guid[src], cur); st.pending[src] = 1; save_power(&st); }
    set_overlay(BEST_PERFORMANCE);
}
static int restore_power(void) {   /* 1 when nothing is left to restore */
    power_state st; wchar_t cur[64]; int src = power_source();
    load_power(&st);
    if (st.pending[src]) {
        if (!overlay(src, cur, 64) || _wcsicmp(cur, st.guid[src])) set_overlay(st.guid[src]);
        st.pending[src] = 0; save_power(&st);
    }
    return !st.pending[AC] && !st.pending[DC];
}

/* ---------------------------------------------------------- play */
static int play(void) {
    wchar_t exe[PATHMAX], cmd[PATHMAX], dir[PATHMAX], settings[PATHMAX], log[PATHMAX];
    path(exe, L"runtime\\saturnwin.exe"); path(dir, L"game");
    path(settings, L"settings.ini"); path(log, L"game\\runtime.log");

    /* Start from a clean environment: no diagnostic SATURN_* overrides. */
    LPWCH env = GetEnvironmentStringsW();
    if (env) {
        for (wchar_t *e = env; *e; e += wcslen(e) + 1) {
            const wchar_t *eq = wcschr(e, L'=');
            if (eq && eq > e && !wcsncmp(e, L"SATURN_", 7) && eq - e < 256 &&
                !(quiet && !wcsncmp(e, L"SATURN_VK_HIDDEN=", 17))) {
                wchar_t name[256]; wcsncpy(name, e, eq - e); name[eq - e] = 0;
                SetEnvironmentVariableW(name, NULL);
            }
        }
        FreeEnvironmentStringsW(env);
    }
    SetEnvironmentVariableW(L"SATURN_SETTINGS_FILE", settings);
    SetEnvironmentVariableW(L"SATURN_SMPCFILE", L"console.bin");
    /* A deeper sound lead than the runtime's 1024 frames rides out the boot
     * logo's slow fields without crackling, for ~23 ms more audio latency. */
    {
        wchar_t frames[16];
        UINT n = GetPrivateProfileIntW(L"Audio", L"AudioBuffer", 2048, settings);
        swprintf(frames, 16, L"%u", n < 256 ? 256 : n > 8192 ? 8192 : n);
        SetEnvironmentVariableW(L"SATURN_SNDBUF", frames);
    }

    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE out = CreateFileW(log, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE in = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
    STARTUPINFOW si = {0}; PROCESS_INFORMATION pi = {0}; si.cb = sizeof si;
    if (out != INVALID_HANDLE_VALUE && in != INVALID_HANDLE_VALUE) {
        si.dwFlags = STARTF_USESTDHANDLES; si.hStdOutput = out; si.hStdError = out; si.hStdInput = in;
    }
    swprintf(cmd, PATHMAX, L"\"%ls\" game.toml", exe);
    BOOL ok = CreateProcessW(exe, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, NULL, dir, &si, &pi);
    if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    if (in != INVALID_HANDLE_VALUE) CloseHandle(in);
    if (!ok) { fail(L"The game could not start. Run Baroque.exe --rebuild to rebuild it."); return 1; }

    place_on_fast_cores(pi.hProcess);
    if (!quiet) boost_power();
    AllowSetForegroundWindow(pi.dwProcessId);   /* let the game window take keyboard focus */
    ResumeThread(pi.hThread); CloseHandle(pi.hThread);
    while (WaitForSingleObject(pi.hProcess, 2000) == WAIT_TIMEOUT) if (!quiet) boost_power();   /* follows plug/unplug */

    DWORD code = 1; GetExitCodeProcess(pi.hProcess, &code); CloseHandle(pi.hProcess);
    if (!restore_power()) {
        wchar_t self[PATHMAX], watch[PATHMAX];
        STARTUPINFOW ws = {0}; PROCESS_INFORMATION wp = {0}; ws.cb = sizeof ws;
        if (GetModuleFileNameW(NULL, self, PATHMAX)) {
            swprintf(watch, PATHMAX, L"\"%ls\" --restore-power", self);
            if (CreateProcessW(self, watch, NULL, NULL, FALSE, DETACHED_PROCESS | CREATE_NO_WINDOW, NULL, root, &ws, &wp)) {
                CloseHandle(wp.hThread); CloseHandle(wp.hProcess);
            }
        }
    }
    if (code) {
        swprintf(cmd, PATHMAX, L"The game stopped with an error. The details are in\n%ls", log);
        fail(cmd);
    }
    return (int)code;
}

int WINAPI wWinMain(HINSTANCE a, HINSTANCE b, PWSTR args, int show) {
    (void)a; (void)b; (void)show;
    if (!GetModuleFileNameW(NULL, root, PATHMAX)) return 1;
    wchar_t *slash = wcsrchr(root, L'\\'); if (!slash) return 1; *slash = 0;
    find_data();
    quiet = GetEnvironmentVariableW(L"SATURN_VK_HIDDEN", NULL, 0) > 1;

    if (wcsstr(args, L"--restore-power")) {   /* detached: wait up to a week for the other power source */
        for (int i = 0; i < 7 * 24 * 720 && !restore_power(); i++) Sleep(5000);
        return 0;
    }
    migrate();
    restore_power();   /* repair anything a killed launcher left behind */

    if (wcsstr(args, L"--rebuild")) { if (!run_setup(L"-Rebuild")) return 1; }
    else if (wcsstr(args, L"--setup")) {
        wchar_t p[PATHMAX]; path(p, L"game\\game.toml"); DeleteFileW(p);
        path(p, L"bios\\saturn.bin"); DeleteFileW(p);
        if (!run_setup(L"")) return 1;
    }
    else if (!runtime_current() || !exists(L"bios\\saturn.bin") || !exists(L"game\\game.toml")) {
        if (!run_setup(L"")) return 1;
    }
    return play();
}
