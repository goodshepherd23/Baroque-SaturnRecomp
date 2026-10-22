/* Baroque.exe -- launcher for the Baroque (SaturnRecomp) kit.
 *
 * 1. Runs kit\setup.ps1 when the runtime has not been built for this kit
 *    version yet, or the BIOS/disc are not set up (it asks for them).
 * 2. Starts runtime\saturnwin.exe on game\game.toml with this kit's settings,
 *    on the fastest CPU cores, at high priority, with the window focused.
 * 3. While the game runs, switches Windows' power mode to "Best performance"
 *    for the power source in use, and puts the original back afterwards.
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

/* -DKIT_VER=0.1.0 (a bare token, so no shell quoting is involved) */
#define KIT_STR_(x) #x
#define KIT_STR(x) KIT_STR_(x)
#define KIT_WIDE_(s) L##s
#define KIT_WIDE(s) KIT_WIDE_(s)
#ifdef KIT_VER
#define KIT_VERSION KIT_WIDE(KIT_STR(KIT_VER))
#else
#define KIT_VERSION L"dev"
#endif
#define PATHMAX 32768

static wchar_t root[PATHMAX];

static void path(wchar_t *out, const wchar_t *rel) { swprintf(out, PATHMAX, L"%ls\\%ls", root, rel); }
static int exists(const wchar_t *rel) { wchar_t p[PATHMAX]; path(p, rel); return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES; }
static void fail(const wchar_t *msg) { MessageBoxW(NULL, msg, L"Baroque", MB_ICONERROR); }

static int runtime_current(void) {
    wchar_t p[PATHMAX], v[64] = L"";
    path(p, L"runtime\\.kit-version");
    FILE *f = _wfopen(p, L"r");
    if (!f) return 0;
    int ok = fwscanf(f, L"%63ls", v) == 1 && !wcscmp(v, KIT_VERSION);
    fclose(f);
    return ok && exists(L"runtime\\saturnwin.exe");
}

static int run_setup(const wchar_t *extra) {
    wchar_t cmd[PATHMAX];
    STARTUPINFOW si = {0}; PROCESS_INFORMATION pi = {0}; si.cb = sizeof si;
    swprintf(cmd, PATHMAX,
        L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"%ls\\kit\\setup.ps1\" -Root \"%ls\" %ls",
        root, root, extra);
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, root, &si, &pi)) {
        fail(L"Could not start the setup (PowerShell is required)."); return 0;
    }
    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1; GetExitCodeProcess(pi.hProcess, &code); CloseHandle(pi.hProcess);
    if (code) fail(L"Setup did not finish. kit\\setup.log in the game folder has the details; "
                   L"run Baroque again to retry.");
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
            if (eq && eq > e && !wcsncmp(e, L"SATURN_", 7) && eq - e < 256) {
                wchar_t name[256]; wcsncpy(name, e, eq - e); name[eq - e] = 0;
                SetEnvironmentVariableW(name, NULL);
            }
        }
        FreeEnvironmentStringsW(env);
    }
    SetEnvironmentVariableW(L"SATURN_SETTINGS_FILE", settings);
    SetEnvironmentVariableW(L"SATURN_SMPCFILE", L"console.bin");

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
    boost_power();
    AllowSetForegroundWindow(pi.dwProcessId);   /* let the game window take keyboard focus */
    ResumeThread(pi.hThread); CloseHandle(pi.hThread);
    while (WaitForSingleObject(pi.hProcess, 2000) == WAIT_TIMEOUT) boost_power();   /* follows plug/unplug */

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
    if (code) fail(L"The game stopped with an error. game\\runtime.log in the game folder has the details.");
    return (int)code;
}

int WINAPI wWinMain(HINSTANCE a, HINSTANCE b, PWSTR args, int show) {
    (void)a; (void)b; (void)show;
    if (!GetModuleFileNameW(NULL, root, PATHMAX)) return 1;
    wchar_t *slash = wcsrchr(root, L'\\'); if (!slash) return 1; *slash = 0;

    if (wcsstr(args, L"--restore-power")) {   /* detached: wait up to a week for the other power source */
        for (int i = 0; i < 7 * 24 * 720 && !restore_power(); i++) Sleep(5000);
        return 0;
    }
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
