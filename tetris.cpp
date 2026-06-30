// tetris.cpp
// Single-file Tetris clone using Win32/GDI. Builds with CMake on Windows (x64).
// Controls: Left/Right - move, Up - rotate, Down - soft drop, Space - hard drop, P - pause

#define UNICODE
#define _UNICODE

#include <windows.h>
#include <vector>
#include <array>
#include <random>
#include <chrono>
#include <string>
#include <algorithm>

constexpr int COLS = 10;
constexpr int ROWS = 20;
constexpr int BLOCK = 28; // pixel size
constexpr int BORDER = 8;

struct Point { int x, y; };

using Shape = std::vector<Point>;

static const std::array<std::array<Shape,4>,7> TETROMINOS = []{
    std::array<std::array<Shape,4>,7> m{};
    // I
    m[0] = { Shape{{0,1},{1,1},{2,1},{3,1}}, Shape{{2,0},{2,1},{2,2},{2,3}}, Shape{{0,2},{1,2},{2,2},{3,2}}, Shape{{1,0},{1,1},{1,2},{1,3}} };
    // J
    m[1] = { Shape{{0,0},{0,1},{1,1},{2,1}}, Shape{{1,0},{2,0},{1,1},{1,2}}, Shape{{0,1},{1,1},{2,1},{2,2}}, Shape{{1,0},{1,1},{0,2},{1,2}} };
    // L
    m[2] = { Shape{{2,0},{0,1},{1,1},{2,1}}, Shape{{1,0},{1,1},{1,2},{2,2}}, Shape{{0,1},{1,1},{2,1},{0,2}}, Shape{{0,0},{1,0},{1,1},{1,2}} };
    // O
    m[3] = { Shape{{1,0},{2,0},{1,1},{2,1}}, Shape{{1,0},{2,0},{1,1},{2,1}}, Shape{{1,0},{2,0},{1,1},{2,1}}, Shape{{1,0},{2,0},{1,1},{2,1}} };
    // S
    m[4] = { Shape{{1,0},{2,0},{0,1},{1,1}}, Shape{{1,0},{1,1},{2,1},{2,2}}, Shape{{1,1},{2,1},{0,2},{1,2}}, Shape{{0,0},{0,1},{1,1},{1,2}} };
    // T
    m[5] = { Shape{{1,0},{0,1},{1,1},{2,1}}, Shape{{1,0},{1,1},{2,1},{1,2}}, Shape{{0,1},{1,1},{2,1},{1,2}}, Shape{{1,0},{0,1},{1,1},{1,2}} };
    // Z
    m[6] = { Shape{{0,0},{1,0},{1,1},{2,1}}, Shape{{2,0},{1,1},{2,1},{1,2}}, Shape{{0,1},{1,1},{1,2},{2,2}}, Shape{{1,0},{0,1},{1,1},{0,2}} };
    return m;
}();

static const std::array<COLORREF,7> COLORS = { RGB(0,240,240), RGB(0,0,240), RGB(240,160,0), RGB(240,240,0), RGB(0,240,0), RGB(160,0,240), RGB(240,0,0) };

// Forward declaration of WndProc
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
static HWND g_hwnd = NULL;

struct Piece {
    int type = 0;
    int rot = 0;
    int x = 3;
    int y = 0;
    const Shape& cells() const { return TETROMINOS[type][rot]; }
};

struct Game {
    std::array<std::array<int, COLS>, ROWS> board{}; // -1 empty, otherwise color index
    Piece cur;
    Piece next;
    bool gameover = false;
    bool paused = false;
    int score = 0;
    int level = 1;
    int speed_ms = 500;
    std::mt19937 rng;
    Game() {
        rng.seed((unsigned)std::chrono::high_resolution_clock::now().time_since_epoch().count());
        for (auto &r : board) r.fill(-1);
        next = randomPiece();
        spawn();
    }
    Piece randomPiece(){
        Piece p; p.type = std::uniform_int_distribution<int>(0,6)(rng); p.rot=0; p.x=3; p.y=0; return p;
    }
    void spawn(){
        cur = next;
        next = randomPiece();
        cur.x = 3; cur.y = 0; cur.rot = 0;
        if (collides(cur, 0, 0)) gameover = true;
    }
    bool collides(const Piece &p, int dx, int dy, int drot=0) const{
        int rot = (p.rot + drot) & 3;
        const Shape &cells = TETROMINOS[p.type][rot];
        for (auto &c: cells){
            int nx = p.x + c.x + dx;
            int ny = p.y + c.y + dy;
            if (nx < 0 || nx >= COLS || ny < 0 || ny >= ROWS) return true;
            if (ny>=0 && board[ny][nx] != -1) return true;
        }
        return false;
    }
    void lock(){
        for (auto &c: cur.cells()){
            int nx = cur.x + c.x;
            int ny = cur.y + c.y;
            if (ny>=0 && ny<ROWS && nx>=0 && nx<COLS) board[ny][nx] = cur.type;
        }
        clearLines();
        spawn();
    }
    void clearLines(){
        int cleared = 0;
        for (int r = ROWS-1; r>=0; --r){
            bool full = true;
            for (int c=0;c<COLS;++c) if (board[r][c]==-1){ full=false; break; }
            if (full){
                ++cleared;
                for (int rr=r; rr>0; --rr) board[rr] = board[rr-1];
                board[0].fill(-1);
                ++r; // re-check same row after shift
            }
        }
        if (cleared){
            score += (cleared * 100) * level;
            level = 1 + score / 1000;
            int new_speed = 500 - (level - 1) * 30;
            speed_ms = (std::min)(80, new_speed);
            // Timer may be created on HWND or thread queue; safe to call even if HWND set later
            if (g_hwnd) SetTimer(g_hwnd, 1, speed_ms, NULL);
        }
    }
    void rotate(){ if (!collides(cur,0,0,1)) cur.rot = (cur.rot+1)&3; }
    void move(int dx){ if (!collides(cur,dx,0)) cur.x += dx; }
    bool softDrop(){ if (!collides(cur,0,1)){ cur.y+=1; return true; } else { lock(); return false; } }
    void hardDrop(){ while(!collides(cur,0,1)) cur.y++; lock(); }
} game;

void drawBlock(HDC hdc, int cx, int cy, COLORREF col){
    RECT r{cx, cy, cx+BLOCK-1, cy+BLOCK-1};
    HBRUSH br = CreateSolidBrush(col);
    FillRect(hdc, &r, br);
    DeleteObject(br);
    // border
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(0,0,0));
    HPEN old = (HPEN)SelectObject(hdc, pen);
    Rectangle(hdc, cx, cy, cx+BLOCK, cy+BLOCK);
    SelectObject(hdc, old);
    DeleteObject(pen);
}

void draw(HDC hdc){
    // background
    RECT rc; GetClientRect(g_hwnd, &rc);
    FillRect(hdc, &rc, (HBRUSH)(COLOR_WINDOW+1));
    int fieldW = COLS*BLOCK;
    int fieldH = ROWS*BLOCK;
    int sx = BORDER;
    int sy = BORDER;
    // draw board background
    RECT fr{sx-1, sy-1, sx+fieldW+1, sy+fieldH+1};
    FillRect(hdc, &fr, (HBRUSH)GetStockObject(WHITE_BRUSH));
    // grid
    for (int r=0;r<ROWS;++r){
        for (int c=0;c<COLS;++c){
            int x = sx + c*BLOCK;
            int y = sy + r*BLOCK;
            if (game.board[r][c] != -1){
                drawBlock(hdc, x, y, COLORS[game.board[r][c]]);
            } else {
                // draw empty cell
                HBRUSH br = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
                RECT cr{ x, y, x+BLOCK-1, y+BLOCK-1 };
                FillRect(hdc, &cr, br);
                // cell border
                HPEN pen = CreatePen(PS_SOLID, 1, RGB(200,200,200));
                HPEN old = (HPEN)SelectObject(hdc, pen);
                Rectangle(hdc, x, y, x+BLOCK, y+BLOCK);
                SelectObject(hdc, old);
                DeleteObject(pen);
            }
        }
    }
    // draw current piece
    for (auto &c: game.cur.cells()){
        int x = sx + (game.cur.x + c.x)*BLOCK;
        int y = sy + (game.cur.y + c.y)*BLOCK;
        drawBlock(hdc, x, y, COLORS[game.cur.type]);
    }
    // draw next piece
    int nx = sx + fieldW + 24;
    int ny = sy;
    TextOutW(hdc, nx, ny, L"Next:", 5);
    ny += 24;
    for (auto &c: game.next.cells()){
        int x = nx + c.x*BLOCK/2;
        int y = ny + c.y*BLOCK/2;
        RECT r{ x, y, x+BLOCK/2-1, y+BLOCK/2-1 };
        HBRUSH br = CreateSolidBrush(COLORS[game.next.type]);
        FillRect(hdc, &r, br);
        DeleteObject(br);
    }
    // score
    std::wstring s = L"Score: " + std::to_wstring(game.score);
    std::wstring l = L"Level: " + std::to_wstring(game.level);
    TextOutW(hdc, nx, ny + 120, s.c_str(), (int)s.size());
    TextOutW(hdc, nx, ny + 150, l.c_str(), (int)l.size());
    if (game.paused){
        std::wstring p = L"PAUSED";
        TextOutW(hdc, sx + fieldW/2 - 30, sy + fieldH/2, p.c_str(), (int)p.size());
    }
    if (game.gameover){
        std::wstring g = L"GAME OVER";
        TextOutW(hdc, sx + fieldW/2 - 40, sy + fieldH/2, g.c_str(), (int)g.size());
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam){
    switch (msg){
        case WM_CREATE:
            {
                SetTimer(hwnd, 1, game.speed_ms, NULL);
            }
            return 0;
        case WM_TIMER:
            if (!game.paused && !game.gameover){
                if (!game.softDrop()){
                    // piece locked and spawn may cause gameover
                }
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        case WM_KEYDOWN:
            if (game.gameover){
                if (wParam == 'R'){
                    game = Game();
                    InvalidateRect(hwnd, NULL, TRUE);
                }
                break;
            }
            switch (wParam){
                case VK_LEFT: game.move(-1); InvalidateRect(hwnd, NULL, TRUE); break;
                case VK_RIGHT: game.move(1); InvalidateRect(hwnd, NULL, TRUE); break;
                case VK_UP: game.rotate(); InvalidateRect(hwnd, NULL, TRUE); break;
                case VK_DOWN: game.softDrop(); InvalidateRect(hwnd, NULL, TRUE); break;
                case VK_SPACE: game.hardDrop(); InvalidateRect(hwnd, NULL, TRUE); break;
                case 'P': case 'p': game.paused = !game.paused; InvalidateRect(hwnd, NULL, TRUE); break;
            }
            return 0;
        case WM_PAINT:
            {
                PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
                draw(hdc);
                EndPaint(hwnd, &ps);
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow){
    const wchar_t CLASS_NAME[]  = L"TetrisClass";
    WNDCLASS wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    RegisterClass(&wc);

    int width = COLS*BLOCK + BORDER*2 + 200;
    int height = ROWS*BLOCK + BORDER*2;
    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"MiniTetris",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        NULL, NULL, hInstance, NULL
    );
    if (!hwnd) return 0;
    g_hwnd = hwnd;
    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0) > 0){
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
