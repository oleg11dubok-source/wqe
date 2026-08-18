#pragma once
#pragma once
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <vector>
#include <string>
#include <algorithm>

// ---- константы ----
const int   NODE_WIDTH = 200;
const int   NODE_HEADER_H = 30;
const int   PIN_HEIGHT = 26;
const int   PIN_RADIUS = 7;
const int   GROUP_HEADER_H = 26;
const int   GRID_SIZE = 40;
const float MIN_ZOOM = 0.2f;
const float MAX_ZOOM = 3.0f;

// layout task-ноды (в мировых единицах)
const int TASK_PIE_R = 24;
const int TASK_PIE_CY = 64;   // центр диаграммы от верха ноды
const int TASK_LIST_TOP = 96;   // начало списка задач
const int TASK_ROW_H = 22;

enum class PinType { Input, Output };
enum class ItemType { None, Node, Pin, Link, Group };

struct Task {
    std::wstring name;
    bool done;
    Task(const std::wstring& n, bool d = false) : name(n), done(d) {}
};
struct Pin {
    int id, nodeId, idx;
    PinType type;
    std::wstring name;
    Pin(int _id, int _node, PinType _t, const std::wstring& _n, int _i)
        : id(_id), nodeId(_node), type(_t), name(_n), idx(_i) {
    }
};
struct Node {
    int id, w, h, cr, cg, cb;
    std::wstring title;
    std::wstring desc;
    float x, y;
    std::vector<Pin> inputs, outputs;
    bool selected = false;
    int groupId = -1;
    std::vector<Task> tasks;
    bool isSummary = false;
    bool isNote = false;
    Node(int _id, const std::wstring& _t, float _x, float _y, int r = 60, int g = 60, int b = 75)
        : id(_id), title(_t), x(_x), y(_y), w(NODE_WIDTH), h(NODE_HEADER_H), cr(r), cg(g), cb(b) {
    }
    float Progress() const {
        if (tasks.empty()) return 0.0f;
        int d = 0; for (auto& t : tasks) if (t.done) ++d;
        return (float)d / (float)tasks.size() * 100.0f;
    }
    void Recalc();
};
struct Link {
    int id, fromPin, toPin;
    bool selected = false;
    Link(int _id, int _f, int _t) :id(_id), fromPin(_f), toPin(_t) {}
};
struct Group {
    int id, w, h;
    std::wstring title;
    float x, y;
    bool selected = false;
    Group(int _id, const std::wstring& _t, float _x, float _y, int _w, int _h)
        : id(_id), title(_t), x(_x), y(_y), w(_w), h(_h) {
    }
};

// ---- глобалы (определены в model.cpp) ----
extern std::vector<Node>  g_nodes;
extern std::vector<Link>  g_links;
extern std::vector<Group> g_groups;
extern int   g_nid, g_pid, g_lid, g_gid;
extern float g_camX, g_camY, g_zoom;
extern ItemType g_drag;
extern int   g_dragId;
extern float g_offX, g_offY;
extern int   g_selNode, g_selLink, g_selGroup;
extern bool  g_pan, g_linking;
extern int   g_linkPin;
extern float g_panSX, g_panSY, g_panCX, g_panCY, g_linkEX, g_linkEY;
extern HWND  g_hwnd, g_hedit;
extern int   g_editNode, g_editTask;
extern bool  g_editDesc;
extern WNDPROC g_oldEdit;

// ---- общие инлайны ----
static inline int f2i(float v) { return (int)(v + 0.5f); }
static inline void W2S(float wx, float wy, float& sx, float& sy) { sx = (wx - g_camX) * g_zoom; sy = (wy - g_camY) * g_zoom; }
static inline void S2W(float sx, float sy, float& wx, float& wy) { wx = sx / g_zoom + g_camX; wy = sy / g_zoom + g_camY; }
static inline void NRect(const Node& n, float& rx, float& ry, float& rw, float& rh) { W2S(n.x, n.y, rx, ry); rw = n.w * g_zoom; rh = n.h * g_zoom; }
static inline void GRect(const Group& g, float& rx, float& ry, float& rw, float& rh) { W2S(g.x, g.y, rx, ry); rw = g.w * g_zoom; rh = g.h * g_zoom; }
static inline void PPos(const Node& n, const Pin& p, float& px, float& py) {
    float fx = (p.type == PinType::Input) ? n.x : n.x + n.w;
    float fy = n.y + NODE_HEADER_H + p.idx * PIN_HEIGHT + PIN_HEIGHT / 2.0f;
    W2S(fx, fy, px, py);
}

// ---- model.cpp ----
Pin* FindPin(int id);
Node* FindNode(int id);
Node* NodeByPin(int id);
bool  HitPin(const Node& n, const Pin& p, float mx, float my);
bool  HitNode(const Node& n, float mx, float my);
bool  HitGroup(const Group& g, float mx, float my);
Link* HitLink(float mx, float my);
void  DelNode(int id);
void  DelLink(int id);
void  DelGroup(int id);
void  DelPin(int pid);
void  ClearSel();
int   AddNode(const std::wstring& t, float x, float y, int r = 60, int g = 60, int b = 75, bool note = false);
int   AddGroup(const std::wstring& t, float x, float y, float w, float h);
void  TaskPie(const Node& n, float& cx, float& cy, float& r);              // экран
void  TaskRects(const Node& n, int idx, RECT& checkbox, RECT& text);       // экран
RECT  NoteBodyRect(const Node& n);                                      // экран

// ---- io.cpp ----
void Save(const wchar_t* fn);
bool Load(const wchar_t* fn);
void SaveAs();    // диалог "Сохранить как..."
void LoadFrom();  // диалог "Открыть..."

// ---- render.cpp ----
void Render(HWND hWnd);

// ---- input.cpp ----
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void EndEdit(bool commit);