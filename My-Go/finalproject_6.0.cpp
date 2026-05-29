#include <windows.h>
#include <commdlg.h>
#include <graphics.h>
#include <conio.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <ctype.h>

// ===================== 基本参数 =====================
#define BOARD_SIZE 19
#define GRID_SIZE_UNDEF_MARKER 1
const int BASE_GRID = 32;
const int BASE_MARGIN = 44;
const int BASE_RADIUS = 13;

int GRID_SIZE = BASE_GRID;
int MARGIN = BASE_MARGIN;
int RADIUS = BASE_RADIUS;
#define MAX_HISTORY (BOARD_SIZE * BOARD_SIZE + 100)
#define MAX_SAVES 10
#define MAX_PLAYERS 2

// 棋子逻辑状态
#define EMPTY 0
#define CHESS_BLACK 1
#define CHESS_WHITE 2

// 游戏模式
#define MODE_PVC 0   // 人机对战
#define MODE_PVP 1   // 人人对战

// ===================== 结构体定义 =====================
typedef struct {
    char name[50];
    int wins;
    int losses;
    int draws;
} PlayerProfile;

typedef struct {
    int x;
    int y;
    int player;
    int capturedStones;
    int capturedPos[BOARD_SIZE * BOARD_SIZE][2];
} MoveRecord;

typedef struct {
    int board[BOARD_SIZE][BOARD_SIZE];
    int currentPlayer;
    int moveCount;
    int history[BOARD_SIZE * BOARD_SIZE + 100][2];
    int historyTop;
    MoveRecord moveRecords[MAX_HISTORY];
    int moveRecordCount;
    char dateTime[50];
    char blackPlayer[50];
    char whitePlayer[50];
    char comment[200];
} GameSave;

// ===================== 全局变量 =====================
int board[BOARD_SIZE][BOARD_SIZE];
int currentPlayer = CHESS_BLACK;
int gameOver = 0;
int gameMode = MODE_PVC;
int pendingGameMode = MODE_PVC;
int humanColor = CHESS_BLACK;
int aiColor = CHESS_WHITE;
int appState = 0; // 0: 选择模式, 1: 选择执棋方, 2: 对局中
int showValidMoves = 0;
int showLibertyCount = 0;
int showCoordinate = 1;
int showLastMove = 1;
int soundEnabled = 1;
int stepDelay = 0;

int history[MAX_HISTORY][2];
int historyTop = 0;
MoveRecord moveRecords[MAX_HISTORY];
int moveRecordCount = 0;
unsigned char boardHistory[MAX_HISTORY][BOARD_SIZE][BOARD_SIZE];
int boardHistoryCount = 0;

int dx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
int dy[8] = {0, 0, 1, -1, 1, -1, 1, -1};

int visited[BOARD_SIZE][BOARD_SIZE];
int territory[BOARD_SIZE][BOARD_SIZE];

PlayerProfile playerProfiles[MAX_PLAYERS];
int profileCount = 0;

GameSave gameSaves[MAX_SAVES];
int saveCount = 0;

time_t gameStartTime;
time_t blackTime = 0;
time_t whiteTime = 0;
time_t lastMoveTime = 0;

// KataGo 外挂 AI
int useKataGoAI = 1;
int kataGoAvailable = 0;
int kataGoStartupTried = 0;
int kataGoBoardDirty = 1;
HANDLE kataGoStdoutRead = NULL;
HANDLE kataGoStdinWrite = NULL;
PROCESS_INFORMATION kataGoProcessInfo = {0};
int kataGoAnalysisAvailable = 0;
int kataGoAnalysisStartupTried = 0;
int kataGoAnalysisBoardDirty = 1;
HANDLE kataGoAnalysisStdoutRead = NULL;
HANDLE kataGoAnalysisStdinWrite = NULL;
PROCESS_INFORMATION kataGoAnalysisProcessInfo = {0};
char kataGoExePath[MAX_PATH] = "";
char kataGoModelPath[MAX_PATH] = "";
char kataGoConfigPath[MAX_PATH] = "";
// 调试日志开关（默认关闭，设置为 1 可开启 kata_go.log 日志）
int debugLogEnabled = 0;
// 可配置项：KataGo 思考时长与是否显示胜率
int kataGoThinkMs = 5000; // 默认 5 秒
int showKataGoWinrate = 0; // 0 不显示, 1 显示
double lastKataGoWinrate = -1.0;
int thinkTimeEditing = 0;
char thinkTimeEditBuffer[32] = {0};
int thinkTimeEditLen = 0;
int hoveredMenuCard = -1;
int pressedMenuCard = -1;
int hoveredSideCard = -1;
int pressedSideCard = -1;
int hoveredSetKataGo = 0;
int pressedSetKataGo = 0;

// 启动界面背景图支持
IMAGE startMenuBgImg;
int startMenuBgLoadState = 0; // 0=未知, 1=已加载, -1=不存在或加载失败
IMAGE startMenuTitleImg;
int startMenuTitleLoadState = 0; // 0=未知, 1=已加载, -1=不存在或加载失败

void tryLoadStartMenuBg() {
    if (startMenuBgLoadState != 0) return;
    FILE* f = fopen("image0.jpg", "rb");
    if (!f) {
        startMenuBgLoadState = -1;
        return;
    }
    fclose(f);
    // 尝试加载图片到 IMAGE 结构
    // loadimage 在 EasyX 中用于加载外部图片
    loadimage(&startMenuBgImg, "image0.jpg");
    startMenuBgLoadState = 1;
}

void tryLoadStartMenuTitleImg() {
    if (startMenuTitleLoadState != 0) return;
    FILE* f = fopen("title.png", "rb");
    if (!f) {
        startMenuTitleLoadState = -1;
        return;
    }
    fclose(f);
    loadimage(&startMenuTitleImg, "title.png");
    startMenuTitleLoadState = 1;
}

static int colorBrightness(COLORREF color) {
    return (GetRValue(color) + GetGValue(color) + GetBValue(color)) / 3;
}

static void drawTitleImageAlphaAwareFit(const IMAGE* img, int dstX, int dstY, int maxW, int maxH) {
    if (!img) return;
    int srcW = img->getwidth();
    int srcH = img->getheight();
    if (srcW <= 0 || srcH <= 0 || maxW <= 0 || maxH <= 0) return;

    DWORD* buffer = GetImageBuffer(img);
    if (!buffer) return;

    double scaleW = (double)maxW / (double)srcW;
    double scaleH = (double)maxH / (double)srcH;
    double scale = scaleW < scaleH ? scaleW : scaleH;
    int dstW = (int)(srcW * scale);
    int dstH = (int)(srcH * scale);
    if (dstW < 1) dstW = 1;
    if (dstH < 1) dstH = 1;
    int drawX = dstX + (maxW - dstW) / 2;
    int drawY = dstY + (maxH - dstH) / 2;

    for (int y = 0; y < dstH; y++) {
        int srcY = (int)((long long)y * srcH / dstH);
        if (srcY < 0) srcY = 0;
        if (srcY >= srcH) srcY = srcH - 1;
        for (int x = 0; x < dstW; x++) {
            int srcX = (int)((long long)x * srcW / dstW);
            if (srcX < 0) srcX = 0;
            if (srcX >= srcW) srcX = srcW - 1;
            DWORD pixel = buffer[srcY * srcW + srcX];
            BYTE alpha = (BYTE)((pixel >> 24) & 0xFF);
            if (alpha < 12) continue;
            putpixel(drawX + x, drawY + y, (COLORREF)(pixel & 0x00FFFFFF));
        }
    }
}

static void drawTitleOverlay(const IMAGE* img, int winW, int winH, float scale) {
    if (!img) return;
    int titleW = (int)(540 * scale);
    int titleH = (int)(210 * scale);
    int titleX = (winW - titleW) / 2;
    int titleY = (int)(42 * scale);
    drawTitleImageAlphaAwareFit(img, titleX, titleY, titleW, titleH);
}

void syncModeNames() {
    strcpy(playerProfiles[0].name, "玩家1");
    strcpy(playerProfiles[1].name, gameMode == MODE_PVC ? "AI" : "玩家2");
}

void updateWindowTitle() {
    SetWindowTextA(GetHWnd(), "My Go !");
}

extern int useKataGoAI;
extern int kataGoAvailable;
extern int kataGoStartupTried;
extern HANDLE kataGoStdoutRead;
extern HANDLE kataGoStdinWrite;
extern PROCESS_INFORMATION kataGoProcessInfo;
extern char kataGoExePath[MAX_PATH];
extern char kataGoModelPath[MAX_PATH];
extern char kataGoConfigPath[MAX_PATH];
void stopKataGoEngine();
int startKataGoEngine();
int xyToGtpMove(int x, int y, char* outMove, int outSize);
void extractParentDir(const char* path, char* outDir, int outSize);
void markKataGoBoardDirty();
int syncKataGoBoardFromHistory();
int syncKataGoMove(int x, int y, int player);
void applyKataGoSearchLimits();
void syncKataGoSearchSettings();
void adjustKataGoThinkMs(int deltaMs);
void toggleKataGoWinrateDisplay();
int refreshKataGoWinrateSnapshot();
int parseKataGoWinrateFromLine(const char* line, double* outWinrate);
int syncKataGoAnalysisBoardFromHistory();
void beginKataGoThinkTimeEdit();
void cancelKataGoThinkTimeEdit();
void commitKataGoThinkTimeEdit();
void handleKataGoThinkTimeEditKey(int ch);

static int startKataGoProcessCommon(HANDLE* stdoutReadOut, HANDLE* stdinWriteOut, PROCESS_INFORMATION* processInfoOut, int* startupTriedFlag, int* availableFlag, const char* startupErrorTitle, int mergeStderrToStdout) {
    if (*availableFlag) return 1;
    if (*startupTriedFlag) return 0;
    *startupTriedFlag = 1;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    HANDLE stdinRead = NULL;
    HANDLE stdinWrite = NULL;
    HANDLE stdoutRead = NULL;
    HANDLE stdoutWrite = NULL;
    HANDLE nullErr = NULL;
    BOOL ok = FALSE;

    if (!CreatePipe(&stdinRead, &stdinWrite, &sa, 0)) goto fail;
    if (!SetHandleInformation(stdinWrite, HANDLE_FLAG_INHERIT, 0)) goto fail;

    if (!CreatePipe(&stdoutRead, &stdoutWrite, &sa, 0)) goto fail;
    if (!SetHandleInformation(stdoutRead, HANDLE_FLAG_INHERIT, 0)) goto fail;

    nullErr = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (nullErr == INVALID_HANDLE_VALUE) nullErr = NULL;

    char workDir[MAX_PATH];
    extractParentDir(kataGoExePath, workDir, sizeof(workDir));

    char commandLine[2048];
    snprintf(commandLine, sizeof(commandLine),
        "\"%s\" gtp -model \"%s\" -config \"%s\"",
        kataGoExePath, kataGoModelPath, kataGoConfigPath);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = stdinRead;
    si.hStdOutput = stdoutWrite;
    si.hStdError = mergeStderrToStdout ? stdoutWrite : (nullErr ? nullErr : stdoutWrite);

    ok = CreateProcessA(NULL, commandLine, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, workDir, &si, &pi);

    CloseHandle(stdinRead);
    CloseHandle(stdoutWrite);
    if (nullErr) CloseHandle(nullErr);

    if (!ok) {
        goto fail_after_process;
    }

    *stdoutReadOut = stdoutRead;
    *stdinWriteOut = stdinWrite;
    *processInfoOut = pi;
    *availableFlag = 1;
    return 1;

fail_after_process:
    if (stdinWrite) CloseHandle(stdinWrite);
    if (stdoutRead) CloseHandle(stdoutRead);
fail:
    *startupTriedFlag = 0;
    return 0;
}

static void stopKataGoProcessCommon(HANDLE* stdoutReadInOut, HANDLE* stdinWriteInOut, PROCESS_INFORMATION* processInfoInOut, int* startupTriedFlag, int* availableFlag, int* boardDirtyFlag) {
    *availableFlag = 0;
    *startupTriedFlag = 0;
    *boardDirtyFlag = 1;

    if (*stdinWriteInOut) {
        CloseHandle(*stdinWriteInOut);
        *stdinWriteInOut = NULL;
    }
    if (*stdoutReadInOut) {
        CloseHandle(*stdoutReadInOut);
        *stdoutReadInOut = NULL;
    }
    if (processInfoInOut->hProcess) {
        TerminateProcess(processInfoInOut->hProcess, 0);
        CloseHandle(processInfoInOut->hProcess);
        processInfoInOut->hProcess = NULL;
    }
    if (processInfoInOut->hThread) {
        CloseHandle(processInfoInOut->hThread);
        processInfoInOut->hThread = NULL;
    }
}

void rebuildBoardHistoryFromMoves() {
    memset(boardHistory, 0, sizeof(boardHistory));
    memset(board, 0, sizeof(board));
    boardHistoryCount = 1;
    memcpy(boardHistory[0], board, sizeof(board));

    for (int i = 0; i < moveRecordCount; i++) {
        MoveRecord* record = &moveRecords[i];
        for (int j = 0; j < record->capturedStones; j++) {
            int cx = record->capturedPos[j][0];
            int cy = record->capturedPos[j][1];
            board[cx][cy] = EMPTY;
        }
        board[record->x][record->y] = record->player;
        memcpy(boardHistory[boardHistoryCount], board, sizeof(board));
        boardHistoryCount++;
    }
}

void extractParentDir(const char* path, char* outDir, int outSize) {
    strncpy(outDir, path, outSize - 1);
    outDir[outSize - 1] = '\0';
    char* lastSlash = strrchr(outDir, '\\');
    if (lastSlash) {
        *lastSlash = '\0';
    }
}

// 打开文件选择对话框，返回非零表示选择成功，路径写入 outPath
int openFileDialog(const char* filter, char* outPath, int outSize) {
    if (!outPath || outSize <= 0) return 0;
    ZeroMemory(outPath, outSize);
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetHWnd();
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = outPath;
    ofn.nMaxFile = outSize;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = NULL;
    return GetOpenFileNameA(&ofn);
}

// 配置文件读写，用于保存 KataGo 路径
void loadKataGoConfig() {
    FILE* f = fopen("kata_config.ini", "r");
    if (!f) return;
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        // 去掉末尾换行
        char* p = line;
        while (*p && *p != '\n' && *p != '\r') p++;
        *p = '\0';

        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char* key = line;
        char* val = eq + 1;
        while (*key == ' ' || *key == '\t') key++;
        while (*val == ' ' || *val == '\t') val++;

        if (_stricmp(key, "exe") == 0) {
            strncpy(kataGoExePath, val, sizeof(kataGoExePath)-1);
            kataGoExePath[sizeof(kataGoExePath)-1] = '\0';
        } else if (_stricmp(key, "model") == 0) {
            strncpy(kataGoModelPath, val, sizeof(kataGoModelPath)-1);
            kataGoModelPath[sizeof(kataGoModelPath)-1] = '\0';
        } else if (_stricmp(key, "config") == 0) {
            strncpy(kataGoConfigPath, val, sizeof(kataGoConfigPath)-1);
            kataGoConfigPath[sizeof(kataGoConfigPath)-1] = '\0';
        }
    }
    fclose(f);
}

void saveKataGoConfig() {
    FILE* f = fopen("kata_config.ini", "w");
    if (!f) return;
    fprintf(f, "exe=%s\n", kataGoExePath[0] ? kataGoExePath : "");
    fprintf(f, "model=%s\n", kataGoModelPath[0] ? kataGoModelPath : "");
    fprintf(f, "config=%s\n", kataGoConfigPath[0] ? kataGoConfigPath : "");
    fclose(f);
}

extern int useKataGoAI;
extern int kataGoAvailable;
extern int kataGoStartupTried;
extern HANDLE kataGoStdoutRead;
extern HANDLE kataGoStdinWrite;
extern PROCESS_INFORMATION kataGoProcessInfo;
extern char kataGoExePath[MAX_PATH];
extern char kataGoModelPath[MAX_PATH];
extern char kataGoConfigPath[MAX_PATH];
void stopKataGoEngine();

int readPipeLine(HANDLE handle, char* buffer, int bufferSize, int timeoutMs) {
    int position = 0;
    DWORD startTime = GetTickCount();

    while (position < bufferSize - 1) {
        DWORD available = 0;
        if (!PeekNamedPipe(handle, NULL, 0, NULL, &available, NULL)) {
            return 0;
        }

        if (available == 0) {
            if ((int)(GetTickCount() - startTime) > timeoutMs) {
                return 0;
            }
            Sleep(10);
            continue;
        }

        char ch = 0;
        DWORD bytesRead = 0;
        if (!ReadFile(handle, &ch, 1, &bytesRead, NULL) || bytesRead == 0) {
            return 0;
        }

        if (ch == '\r') continue;
        if (ch == '\n') break;
        buffer[position++] = ch;
    }

    buffer[position] = '\0';
    return 1;
}

// 简单日志记录（受 debugLogEnabled 控制）
void kataGoLog(const char* fmt, ...) {
    if (!debugLogEnabled) return;
    FILE* f = fopen("kata_go.log", "a");
    if (!f) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fclose(f);
}

int sendKataGoCommandTimed(const char* command, char* response, int responseSize, int timeoutMs) {
    if (kataGoStdinWrite == NULL || kataGoStdoutRead == NULL) {
        return 0;
    }

    DWORD bytesWritten = 0;
    char commandLine[1024];
    snprintf(commandLine, sizeof(commandLine), "%s\n", command);
    kataGoLog("KATA_CMD: %s\n", command);
    if (!WriteFile(kataGoStdinWrite, commandLine, (DWORD)strlen(commandLine), &bytesWritten, NULL)) {
        return 0;
    }

    if (response && responseSize > 0) {
        response[0] = '\0';
    }

    char line[1024];
    int gotFinal = 0;
    for (int guard = 0; guard < 200; guard++) {
        if (!readPipeLine(kataGoStdoutRead, line, sizeof(line), timeoutMs)) {
            return gotFinal;
        }

        kataGoLog("KATA_LINE: %s\n", line);

        if (line[0] == '\0') {
            if (gotFinal) return 1;
            continue;
        }

        if (line[0] == '=' || line[0] == '?') {
            const char* content = line + 1;
            while (*content == ' ') content++;
            if (response && responseSize > 0) {
                strncpy(response, content, responseSize - 1);
                response[responseSize - 1] = '\0';
            }
            gotFinal = 1;
            continue;
        }
    }

    return gotFinal;
}

int sendKataGoCommand(const char* command, char* response, int responseSize) {
    return sendKataGoCommandTimed(command, response, responseSize, 10000);
}

void markKataGoBoardDirty() {
    kataGoBoardDirty = 1;
}

int syncKataGoBoardFromHistory() {
    if (!useKataGoAI || gameMode != MODE_PVC) return 1;
    if (!startKataGoEngine()) return 0;

    char response[1024];
    if (!sendKataGoCommand("clear_board", response, sizeof(response))) return 0;
    if (!sendKataGoCommand("boardsize 19", response, sizeof(response))) return 0;
    if (!sendKataGoCommand("komi 7.5", response, sizeof(response))) return 0;

    for (int i = 0; i < moveRecordCount; i++) {
        if (moveRecords[i].x < 0 || moveRecords[i].y < 0) continue;
        char moveText[16];
        if (!xyToGtpMove(moveRecords[i].x, moveRecords[i].y, moveText, sizeof(moveText))) continue;
        char playCmd[64];
        snprintf(playCmd, sizeof(playCmd), "play %c %s", moveRecords[i].player == CHESS_BLACK ? 'b' : 'w', moveText);
        if (!sendKataGoCommand(playCmd, response, sizeof(response))) return 0;
    }

    kataGoBoardDirty = 0;
    return 1;
}

int syncKataGoMove(int x, int y, int player) {
    if (!useKataGoAI || gameMode != MODE_PVC) return 1;
    if (!startKataGoEngine()) return 0;
    // 为避免不同步导致的盘面分叉，每次人类落子后都完整同步历史到引擎
    return syncKataGoBoardFromHistory();
}

int startKataGoEngine() {
    if (kataGoAvailable) return 1;

    if (!startKataGoProcessCommon(&kataGoStdoutRead, &kataGoStdinWrite, &kataGoProcessInfo, &kataGoStartupTried, &kataGoAvailable, "引擎启动失败", 0)) return 0;

    char response[256];
    if (!sendKataGoCommandTimed("protocol_version", response, sizeof(response), 60000)) {
        MessageBoxA(GetHWnd(), "KataGo 已启动，但 60 秒内没有完成 GTP 握手。请检查模型路径、配置文件或显卡驱动。", "引擎启动失败", MB_OK | MB_ICONERROR);
        stopKataGoEngine();
        return 0;
    }

    // 不在此处清盘或设置 komi，避免每次检查/启动时清空引擎盘面。
    // 由 syncKataGoBoardFromHistory() 在需要时执行完整的清盘与历史同步。
    kataGoAvailable = 1;
    applyKataGoSearchLimits();
    return 1;
}

void stopKataGoEngine() {
    stopKataGoProcessCommon(&kataGoStdoutRead, &kataGoStdinWrite, &kataGoProcessInfo, &kataGoStartupTried, &kataGoAvailable, &kataGoBoardDirty);
}

int startKataGoAnalysisEngine() {
    if (kataGoAnalysisAvailable) return 1;
    return startKataGoProcessCommon(&kataGoAnalysisStdoutRead, &kataGoAnalysisStdinWrite, &kataGoAnalysisProcessInfo, &kataGoAnalysisStartupTried, &kataGoAnalysisAvailable, "胜率引擎启动失败", 1);
}

void stopKataGoAnalysisEngine() {
    stopKataGoProcessCommon(&kataGoAnalysisStdoutRead, &kataGoAnalysisStdinWrite, &kataGoAnalysisProcessInfo, &kataGoAnalysisStartupTried, &kataGoAnalysisAvailable, &kataGoAnalysisBoardDirty);
}

int sendKataGoCommandTimedToEngine(HANDLE stdinWrite, HANDLE stdoutRead, const char* command, char* response, int responseSize, int timeoutMs) {
    if (stdinWrite == NULL || stdoutRead == NULL) {
        return 0;
    }

    DWORD bytesWritten = 0;
    char commandLine[1024];
    snprintf(commandLine, sizeof(commandLine), "%s\n", command);
    if (!WriteFile(stdinWrite, commandLine, (DWORD)strlen(commandLine), &bytesWritten, NULL)) {
        return 0;
    }

    if (response && responseSize > 0) {
        response[0] = '\0';
    }

    char line[1024];
    int gotFinal = 0;
    for (int guard = 0; guard < 200; guard++) {
        if (!readPipeLine(stdoutRead, line, sizeof(line), timeoutMs)) {
            return gotFinal;
        }

        if (line[0] == '\0') {
            if (gotFinal) return 1;
            continue;
        }

        if (line[0] == '=' || line[0] == '?') {
            const char* content = line + 1;
            while (*content == ' ') content++;
            if (response && responseSize > 0) {
                strncpy(response, content, responseSize - 1);
                response[responseSize - 1] = '\0';
            }
            gotFinal = 1;
            continue;
        }
    }

    return gotFinal;
}

int sendKataGoSearchCommandTimedToEngine(HANDLE stdinWrite, HANDLE stdoutRead, const char* command, char* response, int responseSize, int timeoutMs, double* outWinrate) {
    if (stdinWrite == NULL || stdoutRead == NULL) {
        return 0;
    }

    DWORD bytesWritten = 0;
    char commandLine[1024];
    snprintf(commandLine, sizeof(commandLine), "%s\n", command);
    if (!WriteFile(stdinWrite, commandLine, (DWORD)strlen(commandLine), &bytesWritten, NULL)) {
        return 0;
    }

    if (response && responseSize > 0) {
        response[0] = '\0';
    }

    if (outWinrate) {
        *outWinrate = -1.0;
    }

    char line[2048];
    int gotFinal = 0;
    for (int guard = 0; guard < 400; guard++) {
        if (!readPipeLine(stdoutRead, line, sizeof(line), timeoutMs)) {
            return gotFinal;
        }

        if (line[0] == '\0') {
            if (gotFinal) return 1;
            continue;
        }

        if (parseKataGoWinrateFromLine(line, outWinrate)) {
            continue;
        }

        if (line[0] == '=' || line[0] == '?') {
            const char* content = line + 1;
            while (*content == ' ') content++;
            if (response && responseSize > 0) {
                strncpy(response, content, responseSize - 1);
                response[responseSize - 1] = '\0';
            }
            gotFinal = 1;
            continue;
        }
    }

    return gotFinal;
}

int refreshKataGoWinrateSnapshot() {
    if (gameMode != MODE_PVC || !useKataGoAI) return 1;
    if (!startKataGoAnalysisEngine()) return 0;
    if (!syncKataGoAnalysisBoardFromHistory()) return 0;

    char response[1024];
    double winrate = -1.0;
    char player = (humanColor == CHESS_BLACK) ? 'b' : 'w';
    char command[128];
    snprintf(command, sizeof(command), "kata-genmove_analyze %c rootInfo true", player);
    kataGoLog("ANALYZE_CMD: %s\n", command);
    if (!sendKataGoSearchCommandTimedToEngine(kataGoAnalysisStdinWrite, kataGoAnalysisStdoutRead, command, response, sizeof(response), 15000, &winrate)) {
        kataGoLog("ANALYZE_FAILED\n");
        return 0;
    }

    if (winrate >= 0.0) {
        lastKataGoWinrate = winrate;
        kataGoLog("ANALYZE_WINRATE: %.4f\n", winrate);
    }
    return 1;
}

int syncKataGoAnalysisBoardFromHistory() {
    if (!showKataGoWinrate) return 1;
    if (!startKataGoAnalysisEngine()) return 0;

    char response[1024];
    if (!sendKataGoCommandTimedToEngine(kataGoAnalysisStdinWrite, kataGoAnalysisStdoutRead, "clear_board", response, sizeof(response), 5000)) return 0;
    if (!sendKataGoCommandTimedToEngine(kataGoAnalysisStdinWrite, kataGoAnalysisStdoutRead, "boardsize 19", response, sizeof(response), 5000)) return 0;
    if (!sendKataGoCommandTimedToEngine(kataGoAnalysisStdinWrite, kataGoAnalysisStdoutRead, "komi 7.5", response, sizeof(response), 5000)) return 0;

    for (int i = 0; i < moveRecordCount; i++) {
        if (moveRecords[i].x < 0 || moveRecords[i].y < 0) continue;
        char moveText[16];
        if (!xyToGtpMove(moveRecords[i].x, moveRecords[i].y, moveText, sizeof(moveText))) continue;
        char playCmd[64];
        snprintf(playCmd, sizeof(playCmd), "play %c %s", moveRecords[i].player == CHESS_BLACK ? 'b' : 'w', moveText);
        if (!sendKataGoCommandTimedToEngine(kataGoAnalysisStdinWrite, kataGoAnalysisStdoutRead, playCmd, response, sizeof(response), 5000)) return 0;
    }

    kataGoAnalysisBoardDirty = 0;
    return 1;
}

int xyToGtpMove(int x, int y, char* outMove, int outSize) {
    if (x < 0 || y < 0 || x >= BOARD_SIZE || y >= BOARD_SIZE || outSize < 4) return 0;
    char col = (char)('A' + x);
    if (col >= 'I') col++;
    snprintf(outMove, outSize, "%c%d", col, BOARD_SIZE - y);
    return 1;
}

int gtpMoveToXY(const char* move, int* x, int* y) {
    if (!move || !x || !y) return 0;
    if (_stricmp(move, "pass") == 0 || _stricmp(move, "resign") == 0) return 2;
    if (strlen(move) < 2) return 0;

    char colChar = (char)toupper((unsigned char)move[0]);
    int col = colChar - 'A';
    if (colChar > 'I') col--;
    int row = atoi(move + 1);
    if (col < 0 || col >= BOARD_SIZE || row < 1 || row > BOARD_SIZE) return 0;

    *x = col;
    *y = BOARD_SIZE - row;
    return 1;
}

void applyKataGoSearchLimits() {
    if (!kataGoAvailable) return;

    char response[256];
    char command[128];
    double maxTimeSeconds = (double)kataGoThinkMs / 1000.0;
    if (maxTimeSeconds < 1.0) maxTimeSeconds = 1.0;

    snprintf(command, sizeof(command), "kata-set-param maxTime %.3f", maxTimeSeconds);
    sendKataGoCommandTimed(command, response, sizeof(response), 2000);

    sendKataGoCommandTimed("kata-set-param maxVisits 1000000", response, sizeof(response), 2000);
}

void syncKataGoSearchSettings() {
    if (!kataGoAvailable) return;
    applyKataGoSearchLimits();
    if (!showKataGoWinrate) {
        lastKataGoWinrate = -1.0;
    }
}

void adjustKataGoThinkMs(int deltaMs) {
    kataGoThinkMs += deltaMs;
    if (kataGoThinkMs < 1000) kataGoThinkMs = 1000;
    if (kataGoThinkMs > 120000) kataGoThinkMs = 120000;
    syncKataGoSearchSettings();
}

void beginKataGoThinkTimeEdit() {
    snprintf(thinkTimeEditBuffer, sizeof(thinkTimeEditBuffer), "%d", kataGoThinkMs);
    thinkTimeEditLen = (int)strlen(thinkTimeEditBuffer);
    thinkTimeEditing = 1;
}

void cancelKataGoThinkTimeEdit() {
    thinkTimeEditing = 0;
    thinkTimeEditLen = 0;
    thinkTimeEditBuffer[0] = '\0';
}

void commitKataGoThinkTimeEdit() {
    if (thinkTimeEditLen <= 0) {
        cancelKataGoThinkTimeEdit();
        return;
    }

    int value = atoi(thinkTimeEditBuffer);
    if (value < 1000) value = 1000;
    if (value > 120000) value = 120000;
    kataGoThinkMs = value;
    syncKataGoSearchSettings();
    cancelKataGoThinkTimeEdit();
}

void handleKataGoThinkTimeEditKey(int ch) {
    if (!thinkTimeEditing) return;

    if (ch == 27) {
        cancelKataGoThinkTimeEdit();
        return;
    }

    if (ch == 13) {
        commitKataGoThinkTimeEdit();
        return;
    }

    if (ch == 8) {
        if (thinkTimeEditLen > 0) {
            thinkTimeEditLen--;
            thinkTimeEditBuffer[thinkTimeEditLen] = '\0';
        }
        return;
    }

    if (isdigit((unsigned char)ch)) {
        if (thinkTimeEditLen < (int)sizeof(thinkTimeEditBuffer) - 1) {
            thinkTimeEditBuffer[thinkTimeEditLen++] = (char)ch;
            thinkTimeEditBuffer[thinkTimeEditLen] = '\0';
        }
    }
}

void toggleKataGoWinrateDisplay() {
    showKataGoWinrate = !showKataGoWinrate;
    if (!showKataGoWinrate) {
        lastKataGoWinrate = -1.0;
    } else {
        lastKataGoWinrate = -1.0;
        refreshKataGoWinrateSnapshot();
    }
}

int parseKataGoWinrateFromLine(const char* line, double* outWinrate) {
    if (!line) return 0;
    const char* p = strstr(line, "rootInfo");
    if (!p) p = strstr(line, "info");
    if (!p) return 0;

    p = strstr(p, "winrate");
    if (!p) return 0;
    p += strlen("winrate");
    while (*p == ' ' || *p == '=' || *p == ':' || *p == '\t') p++;

    char* endPtr = NULL;
    double value = strtod(p, &endPtr);
    if (endPtr == p) return 0;
    if (value > 1.5) {
        value /= 10000.0;
    }
    if (outWinrate) *outWinrate = value;
    lastKataGoWinrate = value;
    return 1;
}

int sendKataGoSearchCommandTimed(const char* command, char* response, int responseSize, int timeoutMs, double* outWinrate) {
    if (kataGoStdinWrite == NULL || kataGoStdoutRead == NULL) {
        return 0;
    }

    DWORD bytesWritten = 0;
    char commandLine[1024];
    snprintf(commandLine, sizeof(commandLine), "%s\n", command);
    if (!WriteFile(kataGoStdinWrite, commandLine, (DWORD)strlen(commandLine), &bytesWritten, NULL)) {
        return 0;
    }

    if (response && responseSize > 0) {
        response[0] = '\0';
    }

    if (outWinrate) {
        *outWinrate = -1.0;
    }

    char line[2048];
    int gotFinal = 0;
    for (int guard = 0; guard < 400; guard++) {
        if (!readPipeLine(kataGoStdoutRead, line, sizeof(line), timeoutMs)) {
            return gotFinal;
        }

        if (line[0] == '\0') {
            if (gotFinal) return 1;
            continue;
        }

        if (parseKataGoWinrateFromLine(line, outWinrate)) {
            continue;
        }

        if (strncmp(line, "play ", 5) == 0) {
            const char* content = line + 5;
            while (*content == ' ') content++;
            if (response && responseSize > 0) {
                strncpy(response, content, responseSize - 1);
                response[responseSize - 1] = '\0';
            }
            gotFinal = 1;
            continue;
        }

        if (line[0] == '=' || line[0] == '?') {
            const char* content = line + 1;
            while (*content == ' ') content++;
            if (response && responseSize > 0) {
                strncpy(response, content, responseSize - 1);
                response[responseSize - 1] = '\0';
            }
            gotFinal = 1;
            continue;
        }
    }

    return gotFinal;
}

int kataGoBestMove(int* outX, int* outY, int* isPass) {
    if (isPass) *isPass = 0;
    if (!startKataGoEngine()) return 0;

    char response[1024];
    if (kataGoBoardDirty && !syncKataGoBoardFromHistory()) return 0;

    int moveTimeoutMs = kataGoThinkMs + 20000;
    if (moveTimeoutMs < 15000) moveTimeoutMs = 15000;
    if (moveTimeoutMs > 60000) moveTimeoutMs = 60000;

    char playerChar = (aiColor == CHESS_BLACK) ? 'b' : 'w';
    char genCmd[64];
    snprintf(genCmd, sizeof(genCmd), "genmove %c", playerChar);
    if (!sendKataGoCommandTimed(genCmd, response, sizeof(response), moveTimeoutMs)) return 0;

    const char* move = response;
    while (*move == ' ' || *move == '\t') move++;
    if (_strnicmp(move, "play ", 5) == 0) {
        move += 5;
        while (*move == ' ' || *move == '\t') move++;
    }
    while (*move == '=' || *move == '?' || *move == ' ') move++;
    if (_stricmp(move, "pass") == 0 || _stricmp(move, "resign") == 0) {
        if (isPass) *isPass = 1;
        return 1;
    }

    int x = -1, y = -1;
    int parsed = gtpMoveToXY(move, &x, &y);
    if (parsed != 1) return 0;
    if (outX) *outX = x;
    if (outY) *outY = y;
    kataGoBoardDirty = 0;
    return 1;
}

// UI / animation 控制
int uiStyle = 1; // 0: 正式灰色, 1: 现代面板(默认)
int animateEnabled = 1;
int animating = 0;
int animX = -1, animY = -1, animPlayer = 0;
int animSteps = 10;
// 按钮交互状态
int hoveredButton = -1;
int pressedButton = -1;
int hoveredStyleBtn = 0;
int pressedStyleBtn = 0;
int hoveredAnimBtn = 0;
int pressedAnimBtn = 0;
int hoveredEngineCard = 0;
int pressedEngineCard = 0;
int hoveredThinkCard = 0;
int pressedThinkCard = 0;
int hoveredWinrateCard = 0;
int pressedWinrateCard = 0;
// 共享 UI 布局（由 drawBoard 计算，鼠标处理使用）
int uiWinW = 0;
int uiWinH = 0;
int uiBoardLeft = 0;
int uiBoardTop = 0;
int uiBoardRight = 0;
int uiBoardBottom = 0;
int uiPanelLeft = 0;
int uiPanelTop = 0;
int uiPanelRight = 0;
int uiPanelBottom = 0;
int ui_btnLeft = 0;
int ui_btnRight = 0;
int ui_buttonStartY = 0;
int ui_buttonHeight = 0;
int ui_buttonSpacing = 0;
int ui_modeY = 0;
int ui_modeH = 0;
// 全局 UI 缩放因子（用于放大字体与部分控件）
float uiScale = 1.5f; // 1.0 = 原始, 可调整（增大以改善可读性）

// 全屏控制
int isFullscreen = 0;
int restoreWinW = 0;
int restoreWinH = 0;

// 防抖：上一次全屏切换时间（ms）
unsigned long lastFullToggleTime = 0;
// 上一次 F11 按键状态
int prevF11Down = 0;

void updateUILayout();

// 切换全屏：关闭并重新初始化画面，同时调整缩放因子
void toggleFullScreen() {
    unsigned long now = GetTickCount();
    if (now - lastFullToggleTime < 800) return; // 防抖 800ms
    lastFullToggleTime = now;

    if (!isFullscreen) {
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);
        restoreWinW = uiWinW;
        restoreWinH = uiWinH;
        EndBatchDraw();
        closegraph();
        initgraph(sw, sh);
        uiScale = 1.0f;
        BeginBatchDraw();
        updateUILayout();
        isFullscreen = 1;
    } else {
        EndBatchDraw();
        closegraph();
        int backW = restoreWinW > 0 ? restoreWinW : (int)(1360 * 1.15f);
        int backH = restoreWinH > 0 ? restoreWinH : (int)(1040 * 1.15f);
        initgraph(backW, backH);
        uiScale = 1.0f;
        BeginBatchDraw();
        updateUILayout();
        isFullscreen = 0;
    }
}

// 便捷文本样式设置（按缩放应用高度）
void setTextStyleScaled(int height, int width, const char* font) {
    int h = (int)(height * uiScale);
    if (h < 8) h = 8;
    if (h > 34) h = 34; // 限制最大字号，避免溢出和卡顿
    settextstyle(h, width, font);
}

static void drawStartMenuCard(int x1, int y1, int x2, int y2, const char* title, const char* subtitle, COLORREF base, int hovered, int pressed) {
    COLORREF drawCol = base;
    if (hovered) {
        int r = GetRValue(base) + 18; if (r > 255) r = 255;
        int g = GetGValue(base) + 18; if (g > 255) g = 255;
        int b = GetBValue(base) + 18; if (b > 255) b = 255;
        drawCol = RGB(r, g, b);
    }
    if (pressed) {
        int r = GetRValue(base) - 18; if (r < 0) r = 0;
        int g = GetGValue(base) - 18; if (g < 0) g = 0;
        int b = GetBValue(base) - 18; if (b < 0) b = 0;
        drawCol = RGB(r, g, b);
    }
    int baseLight = colorBrightness(drawCol);
    COLORREF titleColor = (baseLight > 180) ? RGB(28, 28, 32) : RGB(250, 250, 252);

    // shadow
    setfillcolor(RGB(0,0,0));
    setlinecolor(RGB(0,0,0));
    solidrectangle(x1 + (int)(6 * uiScale), y1 + (int)(8 * uiScale), x2 + (int)(6 * uiScale), y2 + (int)(8 * uiScale));

    // card body
    setfillcolor(drawCol);
    solidrectangle(x1, y1, x2, y2);
    setlinecolor(RGB(60, 70, 90));
    rectangle(x1, y1, x2, y2);

    // left icon: two stones
    int iconCX = x1 + (int)(40 * uiScale);
    int iconCY = y1 + (y2 - y1) / 2;
    int stoneR = (int)(17 * uiScale);
    setfillcolor(RGB(20,20,20));
    solidcircle(iconCX - (int)(6 * uiScale), iconCY - (int)(6 * uiScale), stoneR);
    setfillcolor(RGB(240,240,240));
    solidcircle(iconCX + (int)(6 * uiScale), iconCY + (int)(8 * uiScale), stoneR);

    // centered title/subtitle block
    setTextStyleScaled(52, 0, "微软雅黑");
    settextcolor(titleColor);
    int titleW = textwidth(title);
    int tx = x1 + (x2 - x1 - titleW) / 2;
    if (tx < iconCX + (int)(55 * uiScale)) tx = iconCX + (int)(55 * uiScale);
    int ty = y1 + (int)(42 * uiScale);
    outtextxy(tx, ty, title);

    if (subtitle && subtitle[0]) {
        setTextStyleScaled(26, 0, "微软雅黑");
        settextcolor(titleColor);
        int subW = textwidth(subtitle);
        int subX = x1 + (x2 - x1 - subW) / 2;
        if (subX < iconCX + (int)(55 * uiScale)) subX = iconCX + (int)(55 * uiScale);
        outtextxy(subX, ty + (int)(52 * uiScale), subtitle);
    }

    // small accent bar on right when hovered
    if (hovered) {
        setfillcolor(RGB(255, 200, 80));
        solidrectangle(x2 - (int)(8 * uiScale), y1 + (int)(12 * uiScale), x2, y2 - (int)(12 * uiScale));
    }
}

void drawStartMenu() {
    COLORREF bgColor = (uiStyle == 1) ? RGB(18, 23, 33) : RGB(245, 242, 236);
    COLORREF cardShadow = (uiStyle == 1) ? RGB(10, 13, 20) : RGB(211, 201, 189);

    // 优先尝试绘制启动背景图片（image0.jpg），若不存在则使用纯色背景
    tryLoadStartMenuBg();
    if (startMenuBgLoadState == 1) {
        int w = uiWinW > 0 ? uiWinW : 1360;
        int h = uiWinH > 0 ? uiWinH : 1040;
        putimage(0, 0, w, h, &startMenuBgImg, 0, 0);
    } else {
        setbkcolor(bgColor);
        cleardevice();
    }
    setbkmode(TRANSPARENT);

    int w = uiWinW > 0 ? uiWinW : 1360;
    int h = uiWinH > 0 ? uiWinH : 1040;
    int cx = w / 2;

    tryLoadStartMenuTitleImg();
    if (startMenuTitleLoadState == 1) {
        drawTitleOverlay(&startMenuTitleImg, w, h, uiScale);
    }

    int cardW = (int)(420 * uiScale);
    int cardH = (int)(170 * uiScale);
    int gap = (int)(32 * uiScale);
    int x1 = cx - cardW - gap / 2;
    int x2 = cx + gap / 2;
    int y1 = (int)(265 * uiScale);
    int y2 = y1 + cardH;

    setfillcolor(cardShadow);
    solidrectangle(x1 + (int)(8 * uiScale), y1 + (int)(10 * uiScale), x1 + (int)(8 * uiScale) + cardW, y2 + (int)(10 * uiScale));
    solidrectangle(x2 + (int)(8 * uiScale), y1 + (int)(10 * uiScale), x2 + (int)(8 * uiScale) + cardW, y2 + (int)(10 * uiScale));
    drawStartMenuCard(x1, y1, x1 + cardW, y2, "人机对战", "", RGB(66, 126, 209), hoveredMenuCard == 0, pressedMenuCard == 0);
    drawStartMenuCard(x2, y1, x2 + cardW, y2, "人人对战", "", RGB(90, 168, 96), hoveredMenuCard == 1, pressedMenuCard == 1);

    int infoW = (int)(220 * uiScale);
    int infoH = (int)(40 * uiScale);
    int infoX = w - infoW - (int)(18 * uiScale);
    int infoY = h - infoH - (int)(18 * uiScale);
    setfillcolor(RGB(18, 23, 33));
    solidroundrect(infoX, infoY, infoX + infoW, infoY + infoH, (int)(8 * uiScale), (int)(8 * uiScale));
    setlinecolor(RGB(95, 110, 135));
    roundrect(infoX, infoY, infoX + infoW, infoY + infoH, (int)(8 * uiScale), (int)(8 * uiScale));
    setTextStyleScaled(14, 0, "Arial");
    settextcolor(RGB(235, 240, 247));
    outtextxy(infoX + (int)(12 * uiScale), infoY + (int)(6 * uiScale), "ver 6.0");
    outtextxy(infoX + (int)(12 * uiScale), infoY + (int)(20 * uiScale), "My Go !");

    if (appState == 1) {
        int sideCardW = (int)(300 * uiScale);
        int sideCardH = (int)(170 * uiScale);
        int sideGap = (int)(28 * uiScale);
        int sy1 = y2 + (int)(92 * uiScale);
        int sx1 = cx - sideCardW - sideGap / 2;
        int sx2 = cx + sideGap / 2;

        setfillcolor(cardShadow);
        solidrectangle(sx1 + (int)(8 * uiScale), sy1 + (int)(8 * uiScale), sx1 + (int)(8 * uiScale) + sideCardW, sy1 + (int)(8 * uiScale) + sideCardH);
        solidrectangle(sx2 + (int)(8 * uiScale), sy1 + (int)(8 * uiScale), sx2 + (int)(8 * uiScale) + sideCardW, sy1 + (int)(8 * uiScale) + sideCardH);
        drawStartMenuCard(sx1, sy1, sx1 + sideCardW, sy1 + sideCardH, "黑棋", "先手", RGB(45, 45, 48), hoveredSideCard == 0, pressedSideCard == 0);
        drawStartMenuCard(sx2, sy1, sx2 + sideCardW, sy1 + sideCardH, "白棋", "后手", RGB(235, 235, 235), hoveredSideCard == 1, pressedSideCard == 1);
    }

    // 在启动页底部绘制“设置 KataGo”按钮，便于在进入对局前配置引擎
    {
        int setW = (int)(240 * uiScale);
        int setH = (int)(46 * uiScale);
        int setX = (int)(40 * uiScale); // 左下角位置
        int setY = h - setH - (int)(36 * uiScale);

        COLORREF base = RGB(86, 203, 124);
        COLORREF drawCol = base;
        if (hoveredSetKataGo) {
            int r = GetRValue(base) + 20; if (r > 255) r = 255;
            int g = GetGValue(base) + 20; if (g > 255) g = 255;
            int b = GetBValue(base) + 20; if (b > 255) b = 255;
            drawCol = RGB(r, g, b);
        }
        if (pressedSetKataGo) {
            int r = GetRValue(base) - 30; if (r < 0) r = 0;
            int g = GetGValue(base) - 30; if (g < 0) g = 0;
            int b = GetBValue(base) - 30; if (b < 0) b = 0;
            drawCol = RGB(r, g, b);
        }

        // shadow
        setfillcolor(RGB(10, 14, 20));
        solidrectangle(setX + (int)(6 * uiScale), setY + (int)(6 * uiScale), setX + setW + (int)(6 * uiScale), setY + setH + (int)(6 * uiScale));
        setfillcolor(drawCol);
        solidrectangle(setX, setY, setX + setW, setY + setH);
        setlinecolor(RGB(60, 70, 90));
        rectangle(setX, setY, setX + setW, setY + setH);
        setTextStyleScaled(20, 0, "微软雅黑");
        settextcolor(RGB(20,20,20));
        int tw = textwidth("设置 KataGo");
        outtextxy(setX + (setW - tw) / 2, setY + (int)(12 * uiScale), "设置 KataGo");
    }
}

void stopKataGoEngine();

int startKataGoEngine();
int sendKataGoCommand(const char* command, char* response, int responseSize);
int kataGoBestMove(int* outX, int* outY, int* isPass);
void judgeGame();

// 根据当前窗口尺寸重算所有布局坐标
void updateUILayout() {
    RECT rc;
    HWND hwnd = GetHWnd();
    if (hwnd && GetClientRect(hwnd, &rc)) {
        uiWinW = rc.right - rc.left;
        uiWinH = rc.bottom - rc.top;
    }
    if (uiWinW <= 0) uiWinW = (int)(1360 * uiScale);
    if (uiWinH <= 0) uiWinH = (int)(1040 * uiScale);

    int pad = (int)(uiWinW * 0.016f);
    if (pad < 14) pad = 14;

    int minPanelWidth = (int)(uiWinW * 0.28f);
    if (minPanelWidth < 340) minPanelWidth = 340;

    int usableHeight = uiWinH - pad * 2;
    int usableWidth = uiWinW - pad * 3 - minPanelWidth;
    int grid = usableHeight / 20;
    int gridByWidth = usableWidth / 20;
    if (gridByWidth < grid) grid = gridByWidth;
    if (grid < 20) grid = 20;
    if (grid > 44) grid = 44;

    GRID_SIZE = grid;
    MARGIN = GRID_SIZE;
    RADIUS = GRID_SIZE / 2 - 3;
    if (RADIUS < 6) RADIUS = 6;

    uiScale = GRID_SIZE / 32.0f;
    if (uiScale < 0.95f) uiScale = 0.95f;
    if (uiScale > 1.35f) uiScale = 1.35f;

    uiBoardLeft = pad;
    uiBoardTop = pad;
    uiBoardRight = uiBoardLeft + GRID_SIZE * 20;
    uiBoardBottom = uiBoardTop + GRID_SIZE * 20;

    uiPanelLeft = uiBoardRight + pad;
    uiPanelTop = pad;
    uiPanelRight = uiWinW - pad;
    uiPanelBottom = uiWinH - pad;

    if (uiPanelRight - uiPanelLeft < minPanelWidth) {
        uiPanelLeft = uiWinW - pad - minPanelWidth;
    }

    // 棋盘在卡片内留出一个格子的边距
    MARGIN = uiBoardLeft + GRID_SIZE;
}

// ===================== 工具函数 =====================
int inBoard(int x, int y) {
    return x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE;
}

// ===================== 音效函数 =====================
void playSound(int type) {
    if (!soundEnabled) return;
    
    switch(type) {
        case 0: // 落子声
            MessageBeep(MB_OK);
            break;
        case 1: // 提子声
            MessageBeep(MB_OK);
            break;
        case 2: // 游戏结束
            MessageBeep(MB_ICONEXCLAMATION);
            break;
    }
}

// ===================== 检查棋子是否有气 =====================
int hasLibertyDFS(int x, int y, int color, int* libertyCount) {
    visited[x][y] = 1;
    int hasLib = 0;
    
    for (int k = 0; k < 4; k++) {
        int nx = x + dx[k];
        int ny = y + dy[k];
        
        if (!inBoard(nx, ny))
            continue;
            
        if (board[nx][ny] == EMPTY) {
            hasLib = 1;
            if (libertyCount) (*libertyCount)++;
        }
        else if (board[nx][ny] == color && !visited[nx][ny]) {
            if (hasLibertyDFS(nx, ny, color, libertyCount))
                hasLib = 1;
        }
    }
    
    return hasLib;
}

// 收集同色连通块，并可顺带判断是否存在气
int collectGroup(int x, int y, int color, int stones[][2], int* stoneCount, unsigned char seen[][BOARD_SIZE]) {
    int stack[BOARD_SIZE * BOARD_SIZE][2];
    int top = 0;
    int count = 0;

    stack[top][0] = x;
    stack[top][1] = y;
    top++;
    seen[x][y] = 1;

    while (top > 0) {
        top--;
        int cx = stack[top][0];
        int cy = stack[top][1];

        stones[count][0] = cx;
        stones[count][1] = cy;
        count++;

        for (int k = 0; k < 4; k++) {
            int nx = cx + dx[k];
            int ny = cy + dy[k];
            if (!inBoard(nx, ny) || seen[nx][ny] || board[nx][ny] != color) {
                continue;
            }
            seen[nx][ny] = 1;
            stack[top][0] = nx;
            stack[top][1] = ny;
            top++;
        }
    }

    if (stoneCount) *stoneCount = count;
    return count;
}

int groupHasLiberty(int stones[][2], int stoneCount) {
    for (int i = 0; i < stoneCount; i++) {
        int x = stones[i][0];
        int y = stones[i][1];
        for (int k = 0; k < 4; k++) {
            int nx = x + dx[k];
            int ny = y + dy[k];
            if (inBoard(nx, ny) && board[nx][ny] == EMPTY) {
                return 1;
            }
        }
    }
    return 0;
}

int countGroupLiberties(int stones[][2], int stoneCount) {
    unsigned char libertySeen[BOARD_SIZE][BOARD_SIZE];
    memset(libertySeen, 0, sizeof(libertySeen));

    int liberties = 0;
    for (int i = 0; i < stoneCount; i++) {
        int x = stones[i][0];
        int y = stones[i][1];
        for (int k = 0; k < 4; k++) {
            int nx = x + dx[k];
            int ny = y + dy[k];
            if (!inBoard(nx, ny) || board[nx][ny] != EMPTY || libertySeen[nx][ny]) {
                continue;
            }
            libertySeen[nx][ny] = 1;
            liberties++;
        }
    }

    return liberties;
}

// ===================== 检查禁着点 =====================
int isValidMove(int x, int y, int color, int* captureCount, int captured[][2]) {
    if (!inBoard(x, y) || board[x][y] != EMPTY) {
        kataGoLog("MAKE_MOVE failed: invalid pos x=%d y=%d inBoard=%d occupied=%d historyTop=%d moveRecordCount=%d\n",
                  x, y, inBoard(x,y), board[x][y] != EMPTY, historyTop, moveRecordCount);
        int start = moveRecordCount - 6;
        if (start < 0) start = 0;
        for (int rr = start; rr < moveRecordCount; rr++) {
            kataGoLog("  recent move %d: p=%c x=%d y=%d\n", rr, moveRecords[rr].player == CHESS_BLACK ? 'b' : 'w', moveRecords[rr].x, moveRecords[rr].y);
        }
        return 0;
    }

    board[x][y] = color;

    int enemy = (color == CHESS_BLACK) ? CHESS_WHITE : CHESS_BLACK;
    int totalCaptured = 0;
    unsigned char processed[BOARD_SIZE][BOARD_SIZE];
    memset(processed, 0, sizeof(processed));
    
    for (int k = 0; k < 4; k++) {
        int nx = x + dx[k];
        int ny = y + dy[k];
        if (!inBoard(nx, ny) || board[nx][ny] != enemy || processed[nx][ny]) 
            continue;

        int groupStones[BOARD_SIZE * BOARD_SIZE][2];
        int groupCount = 0;
        collectGroup(nx, ny, enemy, groupStones, &groupCount, processed);

        int hasLib = 0;
        for (int i = 0; i < groupCount && !hasLib; i++) {
            int gx = groupStones[i][0];
            int gy = groupStones[i][1];
            for (int d = 0; d < 4; d++) {
                int ex = gx + dx[d];
                int ey = gy + dy[d];
                if (inBoard(ex, ey) && board[ex][ey] == EMPTY) {
                    hasLib = 1;
                    break;
                }
            }
        }

        if (!hasLib) {
            for (int i = 0; i < groupCount && totalCaptured < BOARD_SIZE * BOARD_SIZE; i++) {
                captured[totalCaptured][0] = groupStones[i][0];
                captured[totalCaptured][1] = groupStones[i][1];
                totalCaptured++;
            }
        }
    }
    
    if (captureCount) *captureCount = totalCaptured;

    for (int i = 0; i < totalCaptured; i++) {
        board[captured[i][0]][captured[i][1]] = EMPTY;
    }

    int ownGroup[BOARD_SIZE * BOARD_SIZE][2];
    int ownCount = 0;
    unsigned char ownSeen[BOARD_SIZE][BOARD_SIZE];
    memset(ownSeen, 0, sizeof(ownSeen));
    collectGroup(x, y, color, ownGroup, &ownCount, ownSeen);

    int isValid = groupHasLiberty(ownGroup, ownCount) || totalCaptured > 0;
    
    if (isValid && boardHistoryCount >= 2) {
        unsigned char candidate[BOARD_SIZE][BOARD_SIZE];
        memcpy(candidate, board, sizeof(candidate));
        candidate[x][y] = color;
        for (int i = 0; i < totalCaptured; i++) {
            candidate[captured[i][0]][captured[i][1]] = EMPTY;
        }

        if (memcmp(candidate, boardHistory[boardHistoryCount - 2], sizeof(candidate)) == 0) {
            board[x][y] = EMPTY;
            for (int i = 0; i < totalCaptured; i++) {
                board[captured[i][0]][captured[i][1]] = enemy;
            }
            return 0;
        }
    }
    
    board[x][y] = EMPTY;
    for (int i = 0; i < totalCaptured; i++) {
        board[captured[i][0]][captured[i][1]] = enemy;
    }
    return isValid;
}

// ===================== 绘制棋盘 =====================
void drawBoard() {
    updateUILayout();
    int boardLeft = uiBoardLeft;
    int boardTop = uiBoardTop;
    int boardRight = uiBoardRight;
    int boardBottom = uiBoardBottom;
    int panelLeft = uiPanelLeft;
    int panelTop = uiPanelTop;
    int panelRight = uiPanelRight;
    int panelBottom = uiPanelBottom;

    COLORREF bgColor = (uiStyle == 1) ? RGB(18, 23, 33) : RGB(245, 242, 236);
    COLORREF boardFill = RGB(240, 216, 166);
    COLORREF boardBorder = (uiStyle == 1) ? RGB(131, 93, 49) : RGB(145, 109, 68);
    COLORREF gridColor = (uiStyle == 1) ? RGB(115, 81, 42) : RGB(152, 116, 74);
    COLORREF panelFill = (uiStyle == 1) ? RGB(31, 38, 52) : RGB(252, 249, 242);
    COLORREF panelBorder = (uiStyle == 1) ? RGB(78, 90, 114) : RGB(180, 167, 147);
    COLORREF titleFill = (uiStyle == 1) ? RGB(49, 82, 123) : RGB(212, 195, 168);
    COLORREF titleText = (uiStyle == 1) ? RGB(245, 248, 252) : RGB(78, 58, 35);
    COLORREF subtitleText = (uiStyle == 1) ? RGB(194, 210, 229) : RGB(110, 84, 53);
    COLORREF cardFill = (uiStyle == 1) ? RGB(42, 50, 66) : RGB(247, 241, 231);
    COLORREF cardBorder = (uiStyle == 1) ? RGB(67, 79, 99) : RGB(210, 196, 176);
    COLORREF cardText = (uiStyle == 1) ? RGB(203, 214, 228) : RGB(82, 67, 49);
    COLORREF valueText = (uiStyle == 1) ? RGB(246, 248, 250) : RGB(55, 45, 35);
    COLORREF shadowColor = (uiStyle == 1) ? RGB(10, 13, 20) : RGB(211, 201, 189);

    const char* engineLabel = "引擎状态";
    char engineStatusBuf[160];
    char engineWinrateBuf[160];
    const char* engineStatus = engineStatusBuf;
    const char* engineWinrate = engineWinrateBuf;
    COLORREF engineAccent = RGB(126, 139, 153);
    if (gameMode == MODE_PVC) {
        if (!useKataGoAI) {
            snprintf(engineStatusBuf, sizeof(engineStatusBuf), "KataGo 已关闭");
            snprintf(engineWinrateBuf, sizeof(engineWinrateBuf), "胜率 --");
            engineAccent = RGB(144, 144, 144);
        } else if (kataGoAvailable) {
            snprintf(engineStatusBuf, sizeof(engineStatusBuf), "KataGo 已连接");
            if (showKataGoWinrate && lastKataGoWinrate >= 0.0) {
                snprintf(engineWinrateBuf, sizeof(engineWinrateBuf), "胜率 %0.1f%%", lastKataGoWinrate * 100.0);
            } else if (showKataGoWinrate) {
                snprintf(engineWinrateBuf, sizeof(engineWinrateBuf), "胜率 --");
            } else {
                snprintf(engineWinrateBuf, sizeof(engineWinrateBuf), "");
            }
            engineAccent = RGB(86, 203, 124);
        } else if (kataGoStartupTried) {
            snprintf(engineStatusBuf, sizeof(engineStatusBuf), "KataGo 未就绪");
            snprintf(engineWinrateBuf, sizeof(engineWinrateBuf), "胜率 --");
            engineAccent = RGB(255, 159, 67);
        } else {
            snprintf(engineStatusBuf, sizeof(engineStatusBuf), "KataGo 待启动");
            snprintf(engineWinrateBuf, sizeof(engineWinrateBuf), "胜率 --");
            engineAccent = RGB(88, 144, 255);
        }
    } else {
        snprintf(engineStatusBuf, sizeof(engineStatusBuf), "人人对战无需引擎");
        snprintf(engineWinrateBuf, sizeof(engineWinrateBuf), "");
        engineAccent = RGB(126, 139, 153);
    }

    setbkcolor(bgColor);
    cleardevice();
    setbkmode(TRANSPARENT);

    // 背景卡片阴影
    setfillcolor(shadowColor);
    solidrectangle(boardLeft + (int)(GRID_SIZE * 0.25f), boardTop + (int)(GRID_SIZE * 0.25f), boardRight + (int)(GRID_SIZE * 0.25f), boardBottom + (int)(GRID_SIZE * 0.25f));
    solidrectangle(panelLeft + (int)(GRID_SIZE * 0.25f), panelTop + (int)(GRID_SIZE * 0.25f), panelRight + (int)(GRID_SIZE * 0.25f), panelBottom + (int)(GRID_SIZE * 0.25f));

    // 棋盘区域
    setfillcolor(boardFill);
    solidrectangle(boardLeft, boardTop, boardRight, boardBottom);
    setlinecolor(boardBorder);
    setlinestyle(PS_SOLID, 2);
    rectangle(boardLeft, boardTop, boardRight, boardBottom);

    // 顶部标题条
    setfillcolor(titleFill);
    solidrectangle(boardLeft, 0, boardRight, (int)(GRID_SIZE * 0.75f));
    setTextStyleScaled(26, 0, "微软雅黑");
    settextcolor(titleText);
    outtextxy(boardLeft + (int)(GRID_SIZE * 0.45f), (int)(GRID_SIZE * 0.06f), "围棋对局");
    settextcolor(subtitleText);
    setTextStyleScaled(18, 0, "微软雅黑");
    outtextxy(boardLeft + (int)(GRID_SIZE * 8.7f), (int)(GRID_SIZE * 0.1f), "快捷键: Z / R / S / L / M / [ / ] / V / F11");

    // 绘制网格线
    setlinecolor(gridColor);
    setlinestyle(PS_SOLID, 1);
    for (int i = 0; i < BOARD_SIZE; i++) {
        line(MARGIN, MARGIN + i * GRID_SIZE,
            MARGIN + (BOARD_SIZE - 1) * GRID_SIZE,
            MARGIN + i * GRID_SIZE);

        line(MARGIN + i * GRID_SIZE, MARGIN,
            MARGIN + i * GRID_SIZE,
            MARGIN + (BOARD_SIZE - 1) * GRID_SIZE);
    }

    // 绘制坐标
    if (showCoordinate) {
        setTextStyleScaled(20, 0, "Arial");
        settextcolor(RGB(93, 64, 34));
        char coord[3];

        for (int i = 0; i < BOARD_SIZE; i++) {
            coord[0] = (i < 8) ? 'A' + i : 'A' + i + 1;
            coord[1] = '\0';
            outtextxy(MARGIN + i * GRID_SIZE - 6, MARGIN - 28, coord);
            outtextxy(MARGIN + i * GRID_SIZE - 6, MARGIN + BOARD_SIZE * GRID_SIZE + 12, coord);
        }

        for (int i = 0; i < BOARD_SIZE; i++) {
            sprintf(coord, "%2d", i + 1);
            outtextxy(MARGIN - (int)(34*uiScale), MARGIN + i * GRID_SIZE - (int)(8*uiScale), coord);
            outtextxy(MARGIN + BOARD_SIZE * GRID_SIZE + (int)(18*uiScale), MARGIN + i * GRID_SIZE - (int)(8*uiScale), coord);
        }
    }

    // 绘制星位
    setfillcolor(RGB(84, 58, 29));
    int stars[9][2] = {
        {3, 3}, {3, 9}, {3, 15},
        {9, 3}, {9, 9}, {9, 15},
        {15, 3}, {15, 9}, {15, 15}
    };
    for (int i = 0; i < 9; i++) {
        int cx = MARGIN + stars[i][0] * GRID_SIZE;
        int cy = MARGIN + stars[i][1] * GRID_SIZE;
        solidcircle(cx, cy, 4);
    }

    // 右侧信息面板
    setfillcolor(panelFill);
    solidrectangle(panelLeft, panelTop, panelRight, panelBottom);
    setlinecolor(panelBorder);
    setlinestyle(PS_SOLID, 2);
    rectangle(panelLeft, panelTop, panelRight, panelBottom);

    // 面板标题
    setfillcolor(titleFill);
    solidrectangle(panelLeft, panelTop, panelRight, panelTop + 60);
    setTextStyleScaled(30, 0, "微软雅黑");
    settextcolor(titleText);
    outtextxy(panelLeft + (int)(18*uiScale), panelTop + (int)(12*uiScale), "对局信息");

    // 风格与动画开关按钮，放到标题栏右侧，避免被信息卡遮挡
    int styleBtnW = 94;
    int styleBtnH = 24;
    int styleBtnY = panelTop + 18;
    int styleBtnX = panelRight - 16 - styleBtnW;

    // 风格切换（支持悬停/按下视觉）
    {
        COLORREF base = (uiStyle==0?RGB(237,224,201):RGB(42,50,66));
        COLORREF drawCol = base;
        if (hoveredStyleBtn) {
            int r = GetRValue(base) + 30; if (r>255) r=255;
            int g = GetGValue(base) + 30; if (g>255) g=255;
            int b = GetBValue(base) + 30; if (b>255) b=255;
            drawCol = RGB(r,g,b);
        }
        if (pressedStyleBtn) {
            int r = GetRValue(base) - 30; if (r<0) r=0;
            int g = GetGValue(base) - 30; if (g<0) g=0;
            int b = GetBValue(base) - 30; if (b<0) b=0;
            drawCol = RGB(r,g,b);
        }
        setfillcolor(drawCol);
        solidrectangle(styleBtnX, styleBtnY, styleBtnX + styleBtnW, styleBtnY + styleBtnH);
        setlinecolor(panelBorder);
        rectangle(styleBtnX, styleBtnY, styleBtnX + styleBtnW, styleBtnY + styleBtnH);
        setTextStyleScaled(14, 0, "微软雅黑");
        settextcolor(uiStyle==0?RGB(82, 58, 32):RGB(236,242,250));
        outtextxy(styleBtnX + 8, styleBtnY + 4, uiStyle==0?"风格: 正式":"风格: 现代");
    }

    // 动画开关（支持悬停/按下视觉）
    int animBtnW = 94;
    int animBtnX = styleBtnX - animBtnW - 8;
    int animBtnY = styleBtnY;
    {
        COLORREF base = animateEnabled?RGB(86,203,124):RGB(180,180,180);
        COLORREF drawCol = base;
        if (hoveredAnimBtn) {
            int r = GetRValue(base) + 30; if (r>255) r=255;
            int g = GetGValue(base) + 30; if (g>255) g=255;
            int b = GetBValue(base) + 30; if (b>255) b=255;
            drawCol = RGB(r,g,b);
        }
        if (pressedAnimBtn) {
            int r = GetRValue(base) - 30; if (r<0) r=0;
            int g = GetGValue(base) - 30; if (g<0) g=0;
            int b = GetBValue(base) - 30; if (b<0) b=0;
            drawCol = RGB(r,g,b);
        }
        setfillcolor(drawCol);
        solidrectangle(animBtnX, animBtnY, animBtnX + styleBtnW, animBtnY + styleBtnH);
        setlinecolor(panelBorder);
        rectangle(animBtnX, animBtnY, animBtnX + styleBtnW, animBtnY + styleBtnH);
        setTextStyleScaled(14, 0, "微软雅黑");
        settextcolor(animateEnabled?RGB(20,20,20):RGB(60,60,60));
        outtextxy(animBtnX + 8, animBtnY + 4, animateEnabled?"动画: 开启":"动画: 关闭");
    }

    auto drawInfoCard = [&](int y, const char* label, const char* value, COLORREF accent) {
        setfillcolor(cardFill);
        solidrectangle(panelLeft + 12, y, panelRight - 12, y + 64);
        setlinecolor(cardBorder);
        rectangle(panelLeft + 12, y, panelRight - 12, y + 64);
        setfillcolor(accent);
        solidrectangle(panelLeft + 12, y, panelLeft + 18, y + 64);
        setTextStyleScaled(19, 0, "微软雅黑");
        settextcolor(cardText);
        outtextxy(panelLeft + 26, y + 9, label);
        setTextStyleScaled(30, 0, "微软雅黑");
        settextcolor(valueText);
        outtextxy(panelLeft + 26, y + 34, value);
    };

    char infoText[100];
    sprintf(infoText, "%s", gameOver ? "已结束" : "进行中");
    drawInfoCard(88, "游戏状态", infoText, gameOver ? RGB(255, 92, 92) : RGB(86, 203, 124));

    {
        int y = 160;
        COLORREF cardAccent = engineAccent;
        if (hoveredEngineCard) {
            int r = GetRValue(cardAccent) + 24; if (r > 255) r = 255;
            int g = GetGValue(cardAccent) + 24; if (g > 255) g = 255;
            int b = GetBValue(cardAccent) + 24; if (b > 255) b = 255;
            cardAccent = RGB(r, g, b);
        }
        setfillcolor(cardFill);
        solidrectangle(panelLeft + 12, y, panelRight - 12, y + 64);
        setlinecolor(cardBorder);
        rectangle(panelLeft + 12, y, panelRight - 12, y + 64);
        setfillcolor(cardAccent);
        solidrectangle(panelLeft + 12, y, panelLeft + 18, y + 64);
        setTextStyleScaled(19, 0, "微软雅黑");
        settextcolor(cardText);
        outtextxy(panelLeft + 26, y + 9, engineLabel);
        setTextStyleScaled(24, 0, "微软雅黑");
        settextcolor(valueText);
        outtextxy(panelLeft + 26, y + 29, engineStatus);
        setTextStyleScaled(16, 0, "微软雅黑");
        settextcolor(RGB(255, 184, 82));
        outtextxy(panelRight - 142, y + 34, useKataGoAI ? "点击可切换" : "已关闭");
    }

    if (currentPlayer == CHESS_BLACK) {
        drawInfoCard(232, "当前玩家", "黑方", RGB(38, 38, 38));
        setfillcolor(RGB(255, 255, 255));
        solidcircle(panelLeft + 172, 262, 8);
        setfillcolor(RGB(60, 60, 60));
        solidcircle(panelLeft + 169, 259, 3);
    } else {
        drawInfoCard(232, "当前玩家", "白方", RGB(220, 220, 220));
        setfillcolor(RGB(250, 250, 250));
        solidcircle(panelLeft + 172, 262, 8);
        setlinecolor(RGB(90, 90, 90));
        circle(panelLeft + 172, 262, 8);
    }

    int blackCount = 0, whiteCount = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == CHESS_BLACK) blackCount++;
            else if (board[i][j] == CHESS_WHITE) whiteCount++;
        }
    }

    sprintf(infoText, "%d", historyTop);
    drawInfoCard(304, "步数", infoText, RGB(88, 144, 255));
    sprintf(infoText, "黑/白: %d / %d", blackCount, whiteCount);
    drawInfoCard(376, "棋子统计", infoText, RGB(255, 184, 82));

    {
        int y = 448;
        COLORREF accent = RGB(90, 168, 96);
        if (hoveredThinkCard) {
            int r = GetRValue(accent) + 24; if (r > 255) r = 255;
            int g = GetGValue(accent) + 24; if (g > 255) g = 255;
            int b = GetBValue(accent) + 24; if (b > 255) b = 255;
            accent = RGB(r, g, b);
        }
        if (pressedThinkCard) {
            int r = GetRValue(accent) - 20; if (r < 0) r = 0;
            int g = GetGValue(accent) - 20; if (g < 0) g = 0;
            int b = GetBValue(accent) - 20; if (b < 0) b = 0;
            accent = RGB(r, g, b);
        }
        setfillcolor(cardFill);
        solidrectangle(panelLeft + 12, y, panelRight - 12, y + 64);
        setlinecolor(cardBorder);
        rectangle(panelLeft + 12, y, panelRight - 12, y + 64);
        setfillcolor(accent);
        solidrectangle(panelLeft + 12, y, panelLeft + 18, y + 64);
        setTextStyleScaled(19, 0, "微软雅黑");
        settextcolor(cardText);
        outtextxy(panelLeft + 26, y + 9, "思考时间");
        setTextStyleScaled(24, 0, "微软雅黑");
        settextcolor(valueText);
        if (thinkTimeEditing) {
            char editLine[64];
            snprintf(editLine, sizeof(editLine), "%s ms", thinkTimeEditBuffer);
            outtextxy(panelLeft + 26, y + 29, editLine);
            setTextStyleScaled(14, 0, "微软雅黑");
            settextcolor(RGB(203, 214, 228));
            outtextxy(panelLeft + 26, y + 50, "回车确认 / Esc取消");
        } else {
            char timeText[64];
            snprintf(timeText, sizeof(timeText), "%d ms", kataGoThinkMs);
            outtextxy(panelLeft + 26, y + 29, timeText);
            setTextStyleScaled(14, 0, "微软雅黑");
            settextcolor(RGB(203, 214, 228));
            outtextxy(panelLeft + 26, y + 50, "点击后输入数值");
        }
    }

    {
        int y = 520;
        COLORREF accent = showKataGoWinrate ? RGB(255, 184, 82) : RGB(101, 112, 124);
        if (hoveredWinrateCard) {
            int r = GetRValue(accent) + 24; if (r > 255) r = 255;
            int g = GetGValue(accent) + 24; if (g > 255) g = 255;
            int b = GetBValue(accent) + 24; if (b > 255) b = 255;
            accent = RGB(r, g, b);
        }
        if (pressedWinrateCard) {
            int r = GetRValue(accent) - 20; if (r < 0) r = 0;
            int g = GetGValue(accent) - 20; if (g < 0) g = 0;
            int b = GetBValue(accent) - 20; if (b < 0) b = 0;
            accent = RGB(r, g, b);
        }
        setfillcolor(cardFill);
        solidrectangle(panelLeft + 12, y, panelRight - 12, y + 64);
        setlinecolor(cardBorder);
        rectangle(panelLeft + 12, y, panelRight - 12, y + 64);
        setfillcolor(accent);
        solidrectangle(panelLeft + 12, y, panelLeft + 18, y + 64);
        setTextStyleScaled(19, 0, "微软雅黑");
        settextcolor(cardText);
        outtextxy(panelLeft + 26, y + 9, "胜率");
        setTextStyleScaled(24, 0, "微软雅黑");
        settextcolor(valueText);
        if (!showKataGoWinrate) {
            outtextxy(panelLeft + 26, y + 29, "未开启");
            setTextStyleScaled(14, 0, "微软雅黑");
            settextcolor(RGB(203, 214, 228));
            outtextxy(panelLeft + 26, y + 50, "点击切换显示");
        } else if (lastKataGoWinrate >= 0.0) {
            char winrateText[64];
            snprintf(winrateText, sizeof(winrateText), "%0.1f%%", lastKataGoWinrate * 100.0);
            outtextxy(panelLeft + 26, y + 29, winrateText);
            setTextStyleScaled(14, 0, "微软雅黑");
            settextcolor(RGB(203, 214, 228));
            outtextxy(panelLeft + 26, y + 50, "最新局面胜率");
        } else {
            outtextxy(panelLeft + 26, y + 29, "读取中");
            setTextStyleScaled(14, 0, "微软雅黑");
            settextcolor(RGB(203, 214, 228));
            outtextxy(panelLeft + 26, y + 50, "正在请求 KataGo");
        }
    }

    // 按钮区域（计算以避免与底部模式面板重叠）
    int buttonStartY = 592;
    int buttonHeight = 38;
    int buttonSpacing = 42;
    int totalButtons = 9;

    // 先计算 mode 面板放在右侧面板底部区域，保证不与按钮重叠
    ui_modeH = 70;
    ui_modeY = panelBottom - ui_modeH - 12; // 紧贴底部上方

    // 可用按钮绘制区域高度
    int maxButtonAreaBottom = ui_modeY - 12;
    int availableHeight = maxButtonAreaBottom - buttonStartY;
    if (availableHeight < totalButtons * (buttonHeight + 4)) {
        // 缩小间距以适配
        buttonSpacing = availableHeight / totalButtons;
        if (buttonSpacing < 28) buttonSpacing = 28;
        if (buttonHeight > buttonSpacing - 4) buttonHeight = buttonSpacing - 4;
        if (buttonHeight < 24) buttonHeight = 24;
    }

    // 写入全局，以便事件处理使用
    ui_btnLeft = panelLeft + 16;
    ui_btnRight = panelRight - 24;
    ui_buttonStartY = buttonStartY;
    ui_buttonHeight = buttonHeight;
    ui_buttonSpacing = buttonSpacing;

    const char* buttons[] = {
        "悔棋", "结束比赛", "重新开始",
        "保存棋局", "载入棋局", "思考时间",
        "胜率显示", "音效开关", "对战模式"
    };

    COLORREF buttonColors[9] = {
        RGB(61, 118, 252), RGB(230, 79, 79), RGB(255, 159, 67),
        RGB(57, 177, 166), RGB(125, 89, 210), RGB(90, 168, 96),
        RGB(101, 112, 124), RGB(101, 112, 124), RGB(47, 162, 219)
    };

    int btnLeft = ui_btnLeft; // panelLeft + 16
    int btnRight = ui_btnRight; // panelRight - 24
    int buttonY = buttonStartY;
    for (int i = 0; i < totalButtons; i++) {
        // 阴影
        setfillcolor(RGB(24, 30, 41));
        solidrectangle(btnLeft + 4, buttonY + 4, btnRight + 4, buttonY + buttonHeight + 4);

        // 按钮主体颜色，会根据悬停/按下状态调整
        COLORREF base = buttonColors[i];
        COLORREF drawCol = base;
        if (hoveredButton == i) {
            int r = GetRValue(base) + 20; if (r>255) r=255;
            int g = GetGValue(base) + 20; if (g>255) g=255;
            int b = GetBValue(base) + 20; if (b>255) b=255;
            drawCol = RGB(r,g,b);
        }
        if (pressedButton == i) {
            int r = GetRValue(base) - 30; if (r<0) r=0;
            int g = GetGValue(base) - 30; if (g<0) g=0;
            int b = GetBValue(base) - 30; if (b<0) b=0;
            drawCol = RGB(r,g,b);
        }

        setfillcolor(drawCol);
        solidrectangle(btnLeft, buttonY, btnRight, buttonY + ui_buttonHeight);
        setlinecolor(RGB(74, 84, 102));
        rectangle(btnLeft, buttonY, btnRight, buttonY + ui_buttonHeight);

        // 左侧装饰条
        setfillcolor(RGB(255, 255, 255));
        solidrectangle(btnLeft, buttonY, btnLeft + 6, buttonY + ui_buttonHeight);

        setTextStyleScaled(20, 0, "微软雅黑");
        settextcolor(RGB(248, 250, 252));
        int textWidth = (int)(strlen(buttons[i]) * 15 * uiScale);
        outtextxy((btnLeft + btnRight)/2 - textWidth/2, buttonY + (int)(10 * uiScale), buttons[i]);
        buttonY += ui_buttonSpacing;
    }

    // 绘制“模式”区域（锚定到底部，避免遮挡按钮）
    setfillcolor(cardFill);
    solidrectangle(panelLeft + 12, ui_modeY, panelRight - 12, ui_modeY + ui_modeH);
    setlinecolor(cardBorder);
    rectangle(panelLeft + 12, ui_modeY, panelRight - 12, ui_modeY + ui_modeH);
    setTextStyleScaled(20, 0, "微软雅黑");
    settextcolor(cardText);
    outtextxy(panelLeft + (int)(22*uiScale), ui_modeY + (int)(23*uiScale), "模式:");
    settextcolor(valueText);
    if (gameMode == MODE_PVC)
        outtextxy(panelLeft + (int)(72*uiScale), ui_modeY + (int)(23*uiScale), "人机对战");
    else if (gameMode == MODE_PVP)
        outtextxy(panelLeft + (int)(72*uiScale), ui_modeY + (int)(23*uiScale), "人人对战");

    // (已将“模式”面板绘制移至按钮下方，以避免遮挡)
}

// ===================== 绘制棋子 =====================
void drawPieces() {
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] != EMPTY) {
                // 如果正在对 (animX,animY) 做动画，skip 那个格子的常规绘制
                if (animating && i == animX && j == animY) continue;
                int cx = MARGIN + i * GRID_SIZE;
                int cy = MARGIN + j * GRID_SIZE;
                
                if (showLastMove && historyTop > 0) {
                    int lastX = history[historyTop-1][0];
                    int lastY = history[historyTop-1][1];
                    if (i == lastX && j == lastY) {
                        setlinecolor(RGB(255, 120, 60));
                        setlinestyle(PS_SOLID, 3);
                        circle(cx, cy, RADIUS + 4);
                    }
                }
                
                if (board[i][j] == CHESS_BLACK) {
                    setfillcolor(RGB(40, 40, 40));
                    solidcircle(cx + 2, cy + 3, RADIUS + 1);
                    setfillcolor(RGB(0, 0, 0));
                    solidcircle(cx, cy, RADIUS);
                    setfillcolor(RGB(85, 85, 85));
                    solidcircle(cx - 4, cy - 4, 2);
                } else {
                    setfillcolor(RGB(170, 170, 170));
                    solidcircle(cx + 2, cy + 3, RADIUS + 1);
                    setfillcolor(RGB(255, 255, 255));
                    solidcircle(cx, cy, RADIUS);
                    setlinecolor(RGB(0, 0, 0));
                    circle(cx, cy, RADIUS);
                    setfillcolor(RGB(245, 245, 245));
                    solidcircle(cx - 4, cy - 4, 2);
                }
                
                if (showLibertyCount) {
                    memset(visited, 0, sizeof(visited));
                    int liberty = 0;
                    hasLibertyDFS(i, j, board[i][j], &liberty);
                    
                    char libText[10];
                    sprintf(libText, "%d", liberty);
                    settextcolor(board[i][j] == CHESS_BLACK ? RGB(255, 255, 255) : RGB(15, 15, 15));
                    setTextStyleScaled(10, 0, "Arial");
                    outtextxy(cx - (int)(4*uiScale), cy - (int)(5*uiScale), libText);
                }
            }
        }
    }
}

// 简单的棋子放置动画：放大半径
void drawValidMoves();
void startAnimation(int x, int y, int player) {
    if (!animateEnabled) return;
    animating = 1;
    animX = x; animY = y; animPlayer = player;

    int cx = MARGIN + x * GRID_SIZE;
    int cy = MARGIN + y * GRID_SIZE;

    int steps = animSteps;
    if (steps > 4) steps = 4; // 让落子反馈尽量轻量，不拖慢手感
    for (int step = 1; step <= steps; step++) {
        drawBoard();
        drawValidMoves();
        drawPieces();

        float t = (float)step / (float)animSteps;
        int r = (int)(t * RADIUS);

        // 绘制放置中的棋子
        if (animPlayer == CHESS_BLACK) {
            setfillcolor(RGB(40, 40, 40));
            solidcircle(cx + 2, cy + 3, r + 1);
            setfillcolor(RGB(0, 0, 0));
            solidcircle(cx, cy, r);
            setfillcolor(RGB(85, 85, 85));
            if (r > 4) solidcircle(cx - 4, cy - 4, 2);
        } else {
            setfillcolor(RGB(170, 170, 170));
            solidcircle(cx + 2, cy + 3, r + 1);
            setfillcolor(RGB(255, 255, 255));
            solidcircle(cx, cy, r);
            setlinecolor(RGB(0, 0, 0));
            if (r > 2) circle(cx, cy, r);
            setfillcolor(RGB(245, 245, 245));
            if (r > 4) solidcircle(cx - 4, cy - 4, 2);
        }

        FlushBatchDraw();
        Sleep(1);
    }

    animating = 0;
    animX = animY = -1;
}

// ===================== 显示有效着点 =====================
void drawValidMoves() {
    if (!showValidMoves || gameOver) return;
    
    setfillcolor(RGB(74, 184, 104));
    setlinecolor(RGB(38, 120, 68));
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == EMPTY) {
                int captured[10][2];
                if (isValidMove(i, j, currentPlayer, NULL, captured)) {
                    int cx = MARGIN + i * GRID_SIZE;
                    int cy = MARGIN + j * GRID_SIZE;
                    solidcircle(cx, cy, 4);
                }
            }
        }
    }
}

// ===================== 执行走棋 =====================
int makeMove(int x, int y, int player) {
    if (!inBoard(x, y) || board[x][y] != EMPTY)
        return 0;
    
    int captured[BOARD_SIZE * BOARD_SIZE][2];
    int captureCount = 0;
    
    if (!isValidMove(x, y, player, &captureCount, captured)) {
        kataGoLog("MAKE_MOVE failed: isValidMove returned false for player=%c x=%d y=%d historyTop=%d moveRecordCount=%d\n",
                  player == CHESS_BLACK ? 'b' : 'w', x, y, historyTop, moveRecordCount);
        int start = moveRecordCount - 6;
        if (start < 0) start = 0;
        for (int rr = start; rr < moveRecordCount; rr++) {
            kataGoLog("  recent move %d: p=%c x=%d y=%d\n", rr, moveRecords[rr].player == CHESS_BLACK ? 'b' : 'w', moveRecords[rr].x, moveRecords[rr].y);
        }
        return 0;
    }
    
    MoveRecord record;
    record.x = x;
    record.y = y;
    record.player = player;
    record.capturedStones = captureCount;
    for (int i = 0; i < captureCount; i++) {
        record.capturedPos[i][0] = captured[i][0];
        record.capturedPos[i][1] = captured[i][1];
    }
    
    for (int i = 0; i < captureCount; i++) {
        int cx = captured[i][0];
        int cy = captured[i][1];
        board[cx][cy] = EMPTY;
    }
    
    board[x][y] = player;
    
    history[historyTop][0] = x;
    history[historyTop][1] = y;
    historyTop++;
    
    moveRecords[moveRecordCount++] = record;
    memcpy(boardHistory[boardHistoryCount], board, sizeof(board));
    boardHistoryCount++;
    
    if (captureCount > 0)
        playSound(1);
    else
        playSound(0);
    
    if (animateEnabled) {
        startAnimation(x, y, player);
    }

    if (gameMode == MODE_PVC && useKataGoAI) {
        kataGoLog("MAKE_MOVE sync to kata: %c %d %d\n", player == CHESS_BLACK ? 'b' : 'w', x, y);
        if (!syncKataGoMove(x, y, player)) {
            markKataGoBoardDirty();
        }
    }
    
    return 1;
}

// ===================== AI 走棋 =====================
int aiMoveHeuristic() {
    int bestX = -1, bestY = -1;
    float bestScore = -1000000.0f;
    unsigned char candidateMark[BOARD_SIZE][BOARD_SIZE];
    memset(candidateMark, 0, sizeof(candidateMark));

    int hasStone = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == EMPTY) continue;
            hasStone = 1;
            for (int di = -2; di <= 2; di++) {
                for (int dj = -2; dj <= 2; dj++) {
                    int ni = i + di;
                    int nj = j + dj;
                    if (inBoard(ni, nj) && board[ni][nj] == EMPTY) {
                        candidateMark[ni][nj] = 1;
                    }
                }
            }
        }
    }

    if (!hasStone) {
        int anchors[9][2] = {
            {3, 3}, {3, 9}, {3, 15},
            {9, 3}, {9, 9}, {9, 15},
            {15, 3}, {15, 9}, {15, 15}
        };
        for (int i = 0; i < 9; i++) {
            int ax = anchors[i][0];
            int ay = anchors[i][1];
            if (inBoard(ax, ay) && board[ax][ay] == EMPTY) {
                candidateMark[ax][ay] = 1;
            }
        }
        candidateMark[BOARD_SIZE / 2][BOARD_SIZE / 2] = 1;
    }

    int moveCount = 0;
    int validMoves[BOARD_SIZE * BOARD_SIZE][2];
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == EMPTY && candidateMark[i][j]) {
                validMoves[moveCount][0] = i;
                validMoves[moveCount][1] = j;
                moveCount++;
            }
        }
    }

    if (moveCount == 0) {
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                if (board[i][j] == EMPTY) {
                    validMoves[moveCount][0] = i;
                    validMoves[moveCount][1] = j;
                    moveCount++;
                }
            }
        }
    }

    if (moveCount == 0) return 0;

    for (int idx = 0; idx < moveCount; idx++) {
        int i = validMoves[idx][0];
        int j = validMoves[idx][1];

        int captured[BOARD_SIZE * BOARD_SIZE][2];
        int captureCount = 0;
        if (!isValidMove(i, j, aiColor, &captureCount, captured)) {
            continue;
        }

        board[i][j] = aiColor;
        for (int c = 0; c < captureCount; c++) {
            board[captured[c][0]][captured[c][1]] = EMPTY;
        }

        int ownGroup[BOARD_SIZE * BOARD_SIZE][2];
        unsigned char seen[BOARD_SIZE][BOARD_SIZE];
        memset(seen, 0, sizeof(seen));
        int ownCount = 0;
        collectGroup(i, j, CHESS_WHITE, ownGroup, &ownCount, seen);
        int ownLiberties = countGroupLiberties(ownGroup, ownCount);

        unsigned char enemySeen[BOARD_SIZE][BOARD_SIZE];
        memset(enemySeen, 0, sizeof(enemySeen));
        int atariThreats = 0;
        int connectCount = 0;
        for (int d = 0; d < 4; d++) {
            int nx = i + dx[d];
            int ny = j + dy[d];
            if (!inBoard(nx, ny)) continue;

            if (board[nx][ny] == CHESS_WHITE) {
                connectCount++;
            } else if (board[nx][ny] == CHESS_BLACK && !enemySeen[nx][ny]) {
                int enemyGroup[BOARD_SIZE * BOARD_SIZE][2];
                int enemyCount = 0;
                collectGroup(nx, ny, CHESS_BLACK, enemyGroup, &enemyCount, enemySeen);
                int enemyLiberties = countGroupLiberties(enemyGroup, enemyCount);
                if (enemyLiberties == 1) {
                    atariThreats++;
                }
            }
        }

        float score = 0.0f;
        score += captureCount * 1200.0f;
        score += atariThreats * 180.0f;
        score += connectCount * 30.0f;

        if (ownLiberties <= 1 && captureCount == 0) {
            score -= 2500.0f;
        } else {
            score += ownLiberties * 28.0f;
        }

        float centerX = (BOARD_SIZE - 1) / 2.0f;
        float centerY = (BOARD_SIZE - 1) / 2.0f;
        float distanceToCenter = sqrtf((i - centerX) * (i - centerX) + (j - centerY) * (j - centerY));
        score += (BOARD_SIZE - distanceToCenter) * (historyTop < 16 ? 2.0f : 0.6f);

        if (historyTop > 0) {
            int lastX = history[historyTop - 1][0];
            int lastY = history[historyTop - 1][1];
            float distanceToLast = sqrtf((i - lastX) * (i - lastX) + (j - lastY) * (j - lastY));
            score += (5.0f - distanceToLast) * 8.0f;
        }

        score += ((i * 31 + j * 17 + historyTop * 13) % 7) * 0.05f;

        board[i][j] = EMPTY;
        for (int c = 0; c < captureCount; c++) {
            board[captured[c][0]][captured[c][1]] = CHESS_BLACK;
        }

        if (score > bestScore) {
            bestScore = score;
            bestX = i;
            bestY = j;
        }
    }
    
    if (bestX != -1 && bestY != -1) {
        makeMove(bestX, bestY, aiColor);
        return 1;
    }
    
    return 0;
}

int aiMove() {
    if (gameMode == MODE_PVC && useKataGoAI) {
        kataGoLog("AI_MOVE invoked\n");
        int x = -1;
        int y = -1;
        int isPass = 0;
        if (kataGoBestMove(&x, &y, &isPass)) {
            kataGoLog("AI_MOVE best result x=%d y=%d pass=%d\n", x, y, isPass);
            if (isPass) {
                gameOver = 1;
                judgeGame();
                return 1;
            }
            if (x >= 0 && y >= 0 && makeMove(x, y, aiColor)) {
                kataGoLog("AI_MOVE makeMove success\n");
                return 1;
            }
            kataGoLog("AI_MOVE makeMove failed\n");
        }

        markKataGoBoardDirty();
        kataGoLog("AI_MOVE failed\n");
        return 0;
    }

    return aiMoveHeuristic();
}

// ===================== 计算领地 =====================
void calculateTerritory() {
    memset(territory, 0, sizeof(territory));
    memset(visited, 0, sizeof(visited));
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == EMPTY && !visited[i][j]) {
                int queue[BOARD_SIZE * BOARD_SIZE][2];
                int front = 0, rear = 0;
                queue[rear][0] = i;
                queue[rear][1] = j;
                rear++;
                visited[i][j] = 1;
                
                int touchesBlack = 0;
                int touchesWhite = 0;
                int areaSize = 0;
                int area[BOARD_SIZE * BOARD_SIZE][2];
                
                while (front < rear) {
                    int x = queue[front][0];
                    int y = queue[front][1];
                    front++;
                    
                    area[areaSize][0] = x;
                    area[areaSize][1] = y;
                    areaSize++;
                    
                    for (int k = 0; k < 4; k++) {
                        int nx = x + dx[k];
                        int ny = y + dy[k];
                        
                        if (!inBoard(nx, ny)) continue;
                        
                        if (board[nx][ny] == CHESS_BLACK) {
                            touchesBlack = 1;
                        } else if (board[nx][ny] == CHESS_WHITE) {
                            touchesWhite = 1;
                        } else if (!visited[nx][ny]) {
                            visited[nx][ny] = 1;
                            queue[rear][0] = nx;
                            queue[rear][1] = ny;
                            rear++;
                        }
                    }
                }
                
                int owner = 0;
                if (touchesBlack && !touchesWhite) owner = CHESS_BLACK;
                else if (touchesWhite && !touchesBlack) owner = CHESS_WHITE;
                
                for (int idx = 0; idx < areaSize; idx++) {
                    territory[area[idx][0]][area[idx][1]] = owner;
                }
            }
        }
    }
}

// ===================== 判断胜负 =====================
void judgeGame() {
    calculateTerritory();
    
    int blackTerritory = 0;
    int whiteTerritory = 0;
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (territory[i][j] == CHESS_BLACK) blackTerritory++;
            else if (territory[i][j] == CHESS_WHITE) whiteTerritory++;
        }
    }
    
    int blackPrisoners = 0;
    int whitePrisoners = 0;
    for (int i = 0; i < moveRecordCount; i++) {
        if (moveRecords[i].player == CHESS_BLACK) {
            blackPrisoners += moveRecords[i].capturedStones;
        } else {
            whitePrisoners += moveRecords[i].capturedStones;
        }
    }
    
    float finalBlack = blackTerritory + blackPrisoners;
    float finalWhite = whiteTerritory + whitePrisoners + 7.5f;
    
    char result[200];
    if (finalBlack > finalWhite) {
        sprintf(result, "黑方胜！\n黑: %.1f目\n白: %.1f目\n差距: %.1f目", 
                finalBlack, finalWhite, finalBlack - finalWhite);
    } else if (finalWhite > finalBlack) {
        sprintf(result, "白方胜！\n白: %.1f目\n黑: %.1f目\n差距: %.1f目", 
                finalWhite, finalBlack, finalWhite - finalBlack);
    } else {
        sprintf(result, "平局！\n双方都是 %.1f目", finalBlack);
    }
    
    MessageBox(GetHWnd(), result, "游戏结束", MB_OK | MB_ICONINFORMATION);
    playSound(2);
    gameOver = 1;
}

// ===================== 保存棋局 =====================
void saveGame() {
    if (saveCount >= MAX_SAVES) {
        MessageBox(GetHWnd(), "保存空间已满！", "错误", MB_OK | MB_ICONERROR);
        return;
    }
    
    GameSave save;
    memcpy(save.board, board, sizeof(board));
    save.currentPlayer = currentPlayer;
    save.moveCount = historyTop;
    save.historyTop = historyTop;
    save.moveRecordCount = moveRecordCount;
    memcpy(save.history, history, sizeof(history));
    memcpy(save.moveRecords, moveRecords, sizeof(moveRecords));
    save.moveRecordCount = moveRecordCount;
    
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(save.dateTime, sizeof(save.dateTime), "%Y-%m-%d %H:%M:%S", tm_info);
    
    sprintf(save.blackPlayer, "玩家1");
    sprintf(save.whitePlayer, gameMode == MODE_PVC ? "AI" : "玩家2");
    sprintf(save.comment, "手动保存的棋局");
    
    gameSaves[saveCount++] = save;
    
    MessageBox(GetHWnd(), "棋局保存成功！", "保存", MB_OK | MB_ICONINFORMATION);
}

// ===================== 载入棋局 =====================
void loadGame(int index) {
    if (index < 0 || index >= saveCount) {
        MessageBox(GetHWnd(), "无效的保存记录！", "错误", MB_OK | MB_ICONERROR);
        return;
    }
    
    GameSave save = gameSaves[index];
    memcpy(board, save.board, sizeof(board));
    currentPlayer = save.currentPlayer;
    historyTop = save.historyTop;
    gameOver = 0;
    
    moveRecordCount = save.moveRecordCount;
    memcpy(history, save.history, sizeof(history));
    memcpy(moveRecords, save.moveRecords, sizeof(moveRecords));
    rebuildBoardHistoryFromMoves();
    markKataGoBoardDirty();
}

// ===================== 悔棋 =====================
void undoMove() {
    if (historyTop < 1) return;
    
    int steps = (gameMode == MODE_PVC && currentPlayer == CHESS_BLACK) ? 2 : 1;
    
    for (int s = 0; s < steps && historyTop > 0; s++) {
        historyTop--;
        int lastX = history[historyTop][0];
        int lastY = history[historyTop][1];
        
        if (moveRecordCount > 0) {
            MoveRecord lastRecord = moveRecords[moveRecordCount-1];
            for (int i = 0; i < lastRecord.capturedStones; i++) {
                int cx = lastRecord.capturedPos[i][0];
                int cy = lastRecord.capturedPos[i][1];
                board[cx][cy] = (lastRecord.player == CHESS_BLACK) ? CHESS_WHITE : CHESS_BLACK;
            }
            moveRecordCount--;
        }
        
        board[lastX][lastY] = EMPTY;
        currentPlayer = (moveRecordCount > 0 && moveRecords[moveRecordCount-1].player == CHESS_WHITE) ? CHESS_BLACK : CHESS_WHITE;
    }

    rebuildBoardHistoryFromMoves();
    markKataGoBoardDirty();
}

// ===================== 重新开始 =====================
void restartGame() {
    memset(board, 0, sizeof(board));
    historyTop = 0;
    moveRecordCount = 0;
    boardHistoryCount = 1;
    memset(boardHistory, 0, sizeof(boardHistory));
    memcpy(boardHistory[0], board, sizeof(board));
    currentPlayer = CHESS_BLACK;
    gameOver = 0;
    gameStartTime = time(NULL);
    blackTime = whiteTime = 0;
    markKataGoBoardDirty();
}

// ===================== 主函数 =====================
// ===================== 主函数 =====================
int main() {
    SetProcessDPIAware();
    initgraph(1360, 1040);
    uiScale = 1.0f;
    updateUILayout();
    BeginBatchDraw();
    
    srand((unsigned)time(NULL));
    
    // 初始化玩家名称
    strcpy(playerProfiles[0].name, "玩家1");
    playerProfiles[1].wins = 0;
    playerProfiles[1].losses = 0;
    playerProfiles[1].draws = 0;
    playerProfiles[0].wins = 0;
    playerProfiles[0].losses = 0;
    playerProfiles[0].draws = 0;
    syncModeNames();
    
    profileCount = 2;
    
    gameStartTime = time(NULL);
    // 加载 KataGo 配置（若存在）
    loadKataGoConfig();

    if (gameMode == MODE_PVC && useKataGoAI && kataGoExePath[0] != '\0') {
        startKataGoEngine();
    }
    
    while (1) {
        static int prevAppState = -1;
        updateWindowTitle();
        if (appState == 0 || appState == 1) {
            drawStartMenu();
            FlushBatchDraw();
            if (MouseHit()) {
                MOUSEMSG m = GetMouseMsg();
                int mx = m.x, my = m.y;
                int w = uiWinW, h = uiWinH;
                int cx = w/2;
                int cardW = (int)(420 * uiScale);
                int cardH = (int)(170 * uiScale);
                int gap = (int)(32 * uiScale);
                int x1 = cx - cardW - gap/2;
                int x2 = cx + gap/2;
                int y1 = (int)(265 * uiScale);
                int y2 = y1 + cardH;

                hoveredMenuCard = -1;
                if (mx >= x1 && mx <= x1 + cardW && my >= y1 && my <= y2) hoveredMenuCard = 0;
                if (mx >= x2 && mx <= x2 + cardW && my >= y1 && my <= y2) hoveredMenuCard = 1;

                // 启动页上的设置 KataGo 按钮区域（左下角）
                hoveredSetKataGo = 0;
                int setW = (int)(240 * uiScale);
                int setH = (int)(46 * uiScale);
                int setX = (int)(40 * uiScale);
                int setY = h - setH - (int)(36 * uiScale);
                if (mx >= setX && mx <= setX + setW && my >= setY && my <= setY + setH) hoveredSetKataGo = 1;

                if (m.uMsg == WM_LBUTTONDOWN) {
                    // 若点击在设置按钮上，优先处理设置按钮
                    if (hoveredSetKataGo) {
                        pressedSetKataGo = 1;
                        pressedMenuCard = -1;
                    } else {
                        pressedMenuCard = hoveredMenuCard;
                    }
                }
                if (m.uMsg == WM_LBUTTONUP) {
                    if (pressedSetKataGo && hoveredSetKataGo) {
                        // 处理启动页上的设置 KataGo
                        char sel[MAX_PATH];
                        if (openFileDialog("Katago Executable\0*.exe\0All Files\0*.*\0", sel, sizeof(sel))) {
                            strncpy(kataGoExePath, sel, sizeof(kataGoExePath)-1);
                            kataGoExePath[sizeof(kataGoExePath)-1] = '\0';
                            if (openFileDialog("KataGo Model\0*.bin;*.bin.gz;*.bin.zst\0All Files\0*.*\0", sel, sizeof(sel))) {
                                strncpy(kataGoModelPath, sel, sizeof(kataGoModelPath)-1);
                                kataGoModelPath[sizeof(kataGoModelPath)-1] = '\0';
                            }
                            if (openFileDialog("KataGo Config\0*.cfg;*.txt\0All Files\0*.*\0", sel, sizeof(sel))) {
                                strncpy(kataGoConfigPath, sel, sizeof(kataGoConfigPath)-1);
                                kataGoConfigPath[sizeof(kataGoConfigPath)-1] = '\0';
                            }
                            // 保存配置以便下次启动自动加载
                            saveKataGoConfig();
                            MessageBox(GetHWnd(), "KataGo 路径已更新。若引擎正在运行，将尝试重启。", "KataGo", MB_OK | MB_ICONINFORMATION);
                            if (kataGoAvailable) {
                                stopKataGoEngine();
                                startKataGoEngine();
                            } else {
                                if (gameMode == MODE_PVC && useKataGoAI) startKataGoEngine();
                            }
                        }
                    }

                    if (pressedMenuCard != -1 && pressedMenuCard == hoveredMenuCard) {
                        if (pressedMenuCard == 0) {
                            // 进入人机选择执棋方
                            pendingGameMode = MODE_PVC;
                            appState = 1;
                        } else if (pressedMenuCard == 1) {
                            // 直接开始人人对战
                            pendingGameMode = MODE_PVP;
                            gameMode = MODE_PVP;
                            syncModeNames();
                            restartGame();
                            appState = 2;
                        }
                    }
                    pressedMenuCard = -1;
                    pressedSetKataGo = 0;
                }

                // second step: choose side
                    if (appState == 1) {
                    int sideCardW = (int)(300 * uiScale);
                    int sideCardH = (int)(170 * uiScale);
                    int sideGap = (int)(28 * uiScale);
                    int sy1 = y2 + (int)(92 * uiScale);
                    int sx1 = cx - sideCardW - sideGap/2;
                    int sx2 = cx + sideGap/2;
                    hoveredSideCard = -1;
                    if (mx >= sx1 && mx <= sx1 + sideCardW && my >= sy1 && my <= sy1 + sideCardH) hoveredSideCard = 0;
                    if (mx >= sx2 && mx <= sx2 + sideCardW && my >= sy1 && my <= sy1 + sideCardH) hoveredSideCard = 1;

                    if (m.uMsg == WM_LBUTTONDOWN) pressedSideCard = hoveredSideCard;
                    if (m.uMsg == WM_LBUTTONUP) {
                        if (pressedSideCard != -1 && pressedSideCard == hoveredSideCard) {
                            if (pressedSideCard == 0) { humanColor = CHESS_BLACK; aiColor = CHESS_WHITE; }
                            else { humanColor = CHESS_WHITE; aiColor = CHESS_BLACK; }
                            gameMode = pendingGameMode;
                            syncModeNames();
                            restartGame();
                            appState = 2;
                            // ensure KataGo starts if needed
                            if (gameMode == MODE_PVC && useKataGoAI) startKataGoEngine();
                        }
                        pressedSideCard = -1;
                    }
                }
            }
            // on state transition into gameplay, if AI is to play first, invoke it
            if (prevAppState != appState && appState == 2) {
                if (gameMode == MODE_PVC && aiColor == CHESS_BLACK && useKataGoAI) {
                    // let AI play first (call once)
                    if (!gameOver) {
                        Sleep(50);
                        int moved = aiMove();
                        if (moved) {
                            currentPlayer = humanColor; // after AI moves, human's turn
                        }
                    }
                }
            }
            prevAppState = appState;
            continue;
        }

        drawBoard();
        drawValidMoves();
        drawPieces();
        FlushBatchDraw();
        
        if (MouseHit()) {
            MOUSEMSG m = GetMouseMsg();

            int mx = m.x, my = m.y;
            int panelLeft = uiPanelLeft;
            int panelRight = uiPanelRight;
            int panelTop = uiPanelTop;
            int styleBtnW = 94;
            int styleBtnH = 24;
            int styleBtnY = panelTop + (int)(18 * uiScale);
            int styleBtnX = panelRight - (int)(16 * uiScale) - styleBtnW;
            int animBtnX = styleBtnX;
            int animBtnW = 94;
            int animBtnY = styleBtnY;
            animBtnX = styleBtnX - animBtnW - (int)(8 * uiScale);

            int btnLeft = ui_btnLeft ? ui_btnLeft : (panelLeft + (int)(16 * uiScale));
            int btnRight = ui_btnRight ? ui_btnRight : (panelRight - (int)(24 * uiScale));
            int buttonY = ui_buttonStartY ? ui_buttonStartY : (panelTop + (int)(520 * uiScale));
            int buttonHeight = ui_buttonHeight ? ui_buttonHeight : (int)(38 * uiScale);
            int buttonSpacing = ui_buttonSpacing ? ui_buttonSpacing : (int)(42 * uiScale);
            const char* buttons[] = {
                "悔棋", "结束比赛", "重新开始",
                "保存棋局", "载入棋局", "思考时间",
                "胜率显示", "音效开关", "对战模式", "设置 KataGo"
            };
            int btnCount = sizeof(buttons) / sizeof(buttons[0]);

            if (m.uMsg == WM_MOUSEMOVE) {
                // 计算悬停状态
                hoveredButton = -1;
                hoveredStyleBtn = 0;
                hoveredAnimBtn = 0;
                hoveredEngineCard = 0;
                hoveredThinkCard = 0;
                hoveredWinrateCard = 0;

                if (mx >= styleBtnX && mx <= styleBtnX + styleBtnW && my >= styleBtnY && my <= styleBtnY + styleBtnH) {
                    hoveredStyleBtn = 1;
                } else if (mx >= animBtnX && mx <= animBtnX + animBtnW && my >= animBtnY && my <= animBtnY + styleBtnH) {
                    hoveredAnimBtn = 1;
                } else if (gameMode == MODE_PVC && mx >= panelLeft + 12 && mx <= panelRight - 12 && my >= 160 && my <= 224) {
                    hoveredEngineCard = 1;
                } else if (mx >= panelLeft + 12 && mx <= panelRight - 12 && my >= 448 && my <= 512) {
                    hoveredThinkCard = 1;
                } else if (mx >= panelLeft + 12 && mx <= panelRight - 12 && my >= 520 && my <= 584) {
                    hoveredWinrateCard = 1;
                } else {
                    int ty = buttonY;
                    for (int i = 0; i < btnCount; i++) {
                        if (mx >= btnLeft && mx <= btnRight && my >= ty && my <= ty + buttonHeight) {
                            hoveredButton = i;
                            break;
                        }
                        ty += buttonSpacing;
                    }
                }
            }

            if (m.uMsg == WM_LBUTTONDOWN) {
                // 鼠标按下时记录按下的按钮（用于按下反馈），若按在按钮上则不进行棋盘落子
                if (hoveredButton != -1) {
                    pressedButton = hoveredButton;
                } else if (hoveredStyleBtn) {
                    pressedStyleBtn = 1;
                } else if (hoveredAnimBtn) {
                    pressedAnimBtn = 1;
                } else if (hoveredEngineCard) {
                    pressedEngineCard = 1;
                } else if (hoveredThinkCard) {
                    pressedThinkCard = 1;
                } else if (hoveredWinrateCard) {
                    pressedWinrateCard = 1;
                } else {
                    // 不是点击 UI 按钮，则视为棋盘点击（即时落子）
                    if (!gameOver && mx >= MARGIN && mx <= MARGIN + (BOARD_SIZE-1)*GRID_SIZE &&
                        my >= MARGIN && my <= MARGIN + (BOARD_SIZE-1)*GRID_SIZE) {

                        int x = (mx - MARGIN + GRID_SIZE / 2) / GRID_SIZE;
                        int y = (my - MARGIN + GRID_SIZE / 2) / GRID_SIZE;

                        if (gameMode == MODE_PVP) {
                            if (makeMove(x, y, currentPlayer)) {
                                currentPlayer = (currentPlayer == CHESS_BLACK) ? CHESS_WHITE : CHESS_BLACK;
                            }
                        } else if (gameMode == MODE_PVC) {
                            if (currentPlayer == humanColor) {
                                if (makeMove(x, y, humanColor)) {
                                    currentPlayer = aiColor;

                                    if (stepDelay > 0) Sleep(stepDelay);
                                    if (!gameOver) {
                                        if (aiMove()) {
                                            currentPlayer = humanColor;
                                            refreshKataGoWinrateSnapshot();
                                        } else {
                                            currentPlayer = aiColor;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (m.uMsg == WM_LBUTTONUP) {
                // 鼠标弹起时，如果是按下并弹起在同一按钮上，则执行对应动作
                if (pressedButton != -1 && hoveredButton == pressedButton) {
                    switch(pressedButton) {
                        case 0: undoMove(); break;
                        case 1: if (!gameOver) judgeGame(); break;
                        case 2:
                            restartGame();
                            // 如果是人机对战且 AI 执黑，重开后让 AI 先走
                            if (gameMode == MODE_PVC && aiColor == CHESS_BLACK && useKataGoAI && !gameOver) {
                                startKataGoEngine();
                                Sleep(50);
                                int moved = aiMove();
                                if (moved) currentPlayer = humanColor;
                            }
                            break;
                        case 3: saveGame(); break;
                        case 4: if (saveCount > 0) loadGame(0); break;
                        case 5: beginKataGoThinkTimeEdit(); break;
                        case 6:
                            toggleKataGoWinrateDisplay();
                            if (showKataGoWinrate) {
                                refreshKataGoWinrateSnapshot();
                            }
                            break;
                        case 7: soundEnabled = !soundEnabled; break;
                        case 8:
                            gameMode = (gameMode + 1) % 2;
                            syncModeNames();
                            restartGame();
                            break;
                        case 9: {
                            // 选择 KataGo 可执行文件、模型和配置
                            char sel[MAX_PATH];
                            if (openFileDialog("Katago Executable\0*.exe\0All Files\0*.*\0", sel, sizeof(sel))) {
                                strncpy(kataGoExePath, sel, sizeof(kataGoExePath)-1);
                                kataGoExePath[sizeof(kataGoExePath)-1] = '\0';
                                // 选择模型
                                if (openFileDialog("KataGo Model\0*.bin;*.bin.gz;*.bin.zst\0All Files\0*.*\0", sel, sizeof(sel))) {
                                    strncpy(kataGoModelPath, sel, sizeof(kataGoModelPath)-1);
                                    kataGoModelPath[sizeof(kataGoModelPath)-1] = '\0';
                                }
                                // 选择配置文件
                                if (openFileDialog("KataGo Config\0*.cfg;*.txt\0All Files\0*.*\0", sel, sizeof(sel))) {
                                    strncpy(kataGoConfigPath, sel, sizeof(kataGoConfigPath)-1);
                                    kataGoConfigPath[sizeof(kataGoConfigPath)-1] = '\0';
                                }
                                MessageBox(GetHWnd(), "KataGo 路径已更新。若引擎正在运行，将尝试重启。", "KataGo", MB_OK | MB_ICONINFORMATION);
                                // 保存配置
                                saveKataGoConfig();
                                // 如果引擎已在运行，重启以应用新路径
                                if (kataGoAvailable) {
                                    stopKataGoEngine();
                                    startKataGoEngine();
                                } else {
                                    if (gameMode == MODE_PVC && useKataGoAI) startKataGoEngine();
                                }
                            }
                        } break;
                    }
                }

                if (pressedStyleBtn && hoveredStyleBtn) {
                    uiStyle = (uiStyle + 1) % 2;
                }
                if (pressedAnimBtn && hoveredAnimBtn) {
                    animateEnabled = !animateEnabled;
                }
                if (pressedEngineCard && hoveredEngineCard) {
                    if (gameMode == MODE_PVC) {
                        useKataGoAI = !useKataGoAI;
                        if (useKataGoAI) {
                            if (!startKataGoEngine()) {
                                useKataGoAI = 0;
                                MessageBox(GetHWnd(), "KataGo 启动失败，已回退到本地 AI。", "引擎", MB_OK | MB_ICONWARNING);
                            }
                        } else {
                            stopKataGoEngine();
                        }
                    }
                }
                if (pressedThinkCard && hoveredThinkCard) {
                    beginKataGoThinkTimeEdit();
                }
                if (pressedWinrateCard && hoveredWinrateCard) {
                    toggleKataGoWinrateDisplay();
                    if (showKataGoWinrate) {
                        refreshKataGoWinrateSnapshot();
                    }
                }

                // 重置按下状态
                pressedButton = -1;
                pressedStyleBtn = 0;
                pressedAnimBtn = 0;
                pressedEngineCard = 0;
                pressedThinkCard = 0;
                pressedWinrateCard = 0;
            }
        }
        
        if (kbhit()) {
            char key = getch();
            if (thinkTimeEditing) {
                handleKataGoThinkTimeEditKey((unsigned char)key);
            } else {
            switch(key) {
                case 'z':
                case 'Z': undoMove(); break;
                case 'r':
                case 'R': restartGame(); break;
                case 's':
                case 'S': saveGame(); break;
                case 'l':
                case 'L': if (saveCount > 0) loadGame(0); break;
                case 'c':
                case 'C': showCoordinate = !showCoordinate; break;
                case '[': adjustKataGoThinkMs(-5000); break;
                case ']': adjustKataGoThinkMs(5000); break;
                case 'v':
                case 'V': toggleKataGoWinrateDisplay(); break;
                case 'm':
                case 'M': 
                    gameMode = (gameMode + 1) % 2;
                    syncModeNames();
                    restartGame(); 
                    break;
                case 27:
                    EndBatchDraw();
                    closegraph();
                    return 0;
            }
            }
        }
                // F11 全屏切换：检测按键上升沿并防抖，避免重复重建导致闪屏或循环
                int f11down = (GetAsyncKeyState(VK_F11) & 0x8000) ? 1 : 0;
                if (f11down && !prevF11Down) {
                    toggleFullScreen();
                }
                prevF11Down = f11down;

        Sleep(10);
    }
    
    EndBatchDraw();
    closegraph();
    return 0;
}
