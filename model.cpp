#include "editor.h"
#include <cmath>

std::vector<Node>  g_nodes;
std::vector<Link>  g_links;
std::vector<Group> g_groups;
int   g_nid = 1, g_pid = 1000, g_lid = 5000, g_gid = 100;
float g_camX = 0, g_camY = 0, g_zoom = 1.0f;
ItemType g_drag = ItemType::None;
int   g_dragId = -1;
float g_offX = 0, g_offY = 0;
int   g_selNode = -1, g_selLink = -1, g_selGroup = -1;
bool  g_pan = false, g_linking = false;
int   g_linkPin = -1;
float g_panSX = 0, g_panSY = 0, g_panCX = 0, g_panCY = 0;
float g_linkEX = 0, g_linkEY = 0;
HWND  g_hwnd = nullptr, g_hedit = nullptr;
int   g_editNode = -1, g_editTask = -1;
bool  g_editDesc = false;
WNDPROC g_oldEdit = nullptr;

void Node::Recalc() {
    int pins = (int)std::max(inputs.size(), outputs.size());
    int body = pins * PIN_HEIGHT;
    if (!tasks.empty())
        body = (std::max)(body, (TASK_LIST_TOP - NODE_HEADER_H) + TASK_ROW_H * (int)tasks.size() + 10);
    if (isSummary) body = (std::max)(body, 160);
    if (isNote) {
        int lines = 1; for (wchar_t c : desc) if (c == L'\n') lines++;
        int pins = (int)std::max(inputs.size(), outputs.size());
        body = (std::max)(body, pins * PIN_HEIGHT + 6 + lines * 20 + 8);
    }
}

Pin* FindPin(int id) { for (auto& n : g_nodes) { for (auto& p : n.inputs) if (p.id == id) return &p; for (auto& p : n.outputs) if (p.id == id) return &p; } return nullptr; }
Node* FindNode(int id) { for (auto& n : g_nodes) if (n.id == id) return &n; return nullptr; }
Node* NodeByPin(int id) { for (auto& n : g_nodes) { for (auto& p : n.inputs) if (p.id == id) return &n; for (auto& p : n.outputs) if (p.id == id) return &n; } return nullptr; }

bool HitPin(const Node& n, const Pin& p, float mx, float my) {
    float px, py; PPos(n, p, px, py);
    float r = PIN_RADIUS * g_zoom + 4, dx = mx - px, dy = my - py;
    return dx * dx + dy * dy <= r * r;
}
bool HitNode(const Node& n, float mx, float my) { float rx, ry, rw, rh; NRect(n, rx, ry, rw, rh); return mx >= rx && mx <= rx + rw && my >= ry && my <= ry + rh; }
bool HitGroup(const Group& g, float mx, float my) { float rx, ry, rw, rh; GRect(g, rx, ry, rw, rh); return mx >= rx && mx <= rx + rw && my >= ry && my <= ry + rh; }

static float DistBez(float mx, float my, float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4) {
    float md = 1e9f;
    for (int i = 0;i <= 20;i++) {
        float t = i / 20.0f, u = 1 - t, tt = t * t, uu = u * u;
        float bx = uu * u * x1 + 3 * uu * t * x2 + 3 * u * tt * x3 + tt * t * x4;
        float by = uu * u * y1 + 3 * uu * t * y2 + 3 * u * tt * y3 + tt * t * y4;
        float d2 = (mx - bx) * (mx - bx) + (my - by) * (my - by);
        if (d2 < md) md = d2;
    }
    return sqrtf(md);
}
Link* HitLink(float mx, float my) {
    for (auto& l : g_links) {
        Pin* a = FindPin(l.fromPin); Pin* b = FindPin(l.toPin); if (!a || !b) continue;
        Node* na = NodeByPin(l.fromPin); Node* nb = NodeByPin(l.toPin); if (!na || !nb) continue;
        float p1x, p1y, p4x, p4y; PPos(*na, *a, p1x, p1y); PPos(*nb, *b, p4x, p4y);
        if (DistBez(mx, my, p1x, p1y, p1x + 80 * g_zoom, p1y, p4x - 80 * g_zoom, p4y, p4x, p4y) < 8) return &l;
    }
    return nullptr;
}

void DelNode(int id) {
    if (g_editNode == id) { ShowWindow(g_hedit, SW_HIDE); g_editNode = -1; g_editTask = -1; g_editDesc = false; }
    g_links.erase(std::remove_if(g_links.begin(), g_links.end(), [id](const Link& l) {
        Pin* a = FindPin(l.fromPin); Pin* b = FindPin(l.toPin);
        return (a && a->nodeId == id) || (b && b->nodeId == id); }), g_links.end());
    g_nodes.erase(std::remove_if(g_nodes.begin(), g_nodes.end(), [id](const Node& n) {return n.id == id;}), g_nodes.end());
    if (g_selNode == id) g_selNode = -1;
}
void DelLink(int id) { g_links.erase(std::remove_if(g_links.begin(), g_links.end(), [id](const Link& l) {return l.id == id;}), g_links.end()); if (g_selLink == id) g_selLink = -1; }
void DelGroup(int id) { for (auto& n : g_nodes) if (n.groupId == id) n.groupId = -1; g_groups.erase(std::remove_if(g_groups.begin(), g_groups.end(), [id](const Group& g) {return g.id == id;}), g_groups.end()); if (g_selGroup == id) g_selGroup = -1; }
void ClearSel() { g_selNode = g_selLink = g_selGroup = -1; for (auto& n : g_nodes)n.selected = false; for (auto& l : g_links)l.selected = false; for (auto& g : g_groups)g.selected = false; }

void DelPin(int pid) {
    Pin* p = FindPin(pid); if (!p) return;
    Node* n = FindNode(p->nodeId); if (!n) return;
    g_links.erase(std::remove_if(g_links.begin(), g_links.end(), [pid](const Link& l) {
        return l.fromPin == pid || l.toPin == pid; }), g_links.end());
    std::vector<Pin>& vec = (p->type == PinType::Input) ? n->inputs : n->outputs;
    vec.erase(std::remove_if(vec.begin(), vec.end(),
        [pid](const Pin& q) { return q.id == pid; }), vec.end());
    for (size_t i = 0;i < vec.size();++i) vec[i].idx = (int)i;
    n->Recalc();
}

int AddNode(const std::wstring& t, float x, float y, int r, int g, int b, bool note) {
    Node n(g_nid++, t, x, y, r, g, b);
    n.isNote = note;
    n.inputs.emplace_back(g_pid++, n.id, PinType::Input, L"in", 0);
    n.outputs.emplace_back(g_pid++, n.id, PinType::Output, L"out", 0);
    n.Recalc(); g_nodes.push_back(n); return n.id;
}
int AddGroup(const std::wstring& t, float x, float y, float w, float h) { g_groups.emplace_back(g_gid++, t, x, y, (int)w, (int)h); return g_groups.back().id; }

// ---- общий layout task-ноды (экран) ----
void TaskPie(const Node& n, float& cx, float& cy, float& r) {
    float rx, ry, rw, rh; NRect(n, rx, ry, rw, rh);
    float hh = NODE_HEADER_H * g_zoom;
    cx = rx + rw * 0.5f;                                   // по центру
    cy = ry + hh + (TASK_PIE_CY - NODE_HEADER_H) * g_zoom;
    r = TASK_PIE_R * g_zoom; if (r < 6)r = 6;
}
void TaskRects(const Node& n, int idx, RECT& cb, RECT& tx) {
    float rx, ry, rw, rh; NRect(n, rx, ry, rw, rh);
    float z = g_zoom, hh = NODE_HEADER_H * z;
    float ty = ry + hh + (TASK_LIST_TOP - NODE_HEADER_H) * z + idx * TASK_ROW_H * z;
    cb.left = (LONG)(rx + 10 * z);
    cb.top = (LONG)ty;
    cb.right = (LONG)(rx + 10 * z + (LONG)(14 * z));
    cb.bottom = (LONG)(ty + 14 * z);
    tx.left = (LONG)(rx + 30 * z);
    tx.top = (LONG)(ty - 2 * z);
    tx.right = (LONG)(rx + rw - 10 * z);
    tx.bottom = (LONG)(ty + 16 * z);
}
RECT NoteBodyRect(const Node& n) {
    float rx, ry, rw, rh; NRect(n, rx, ry, rw, rh);
    float z = g_zoom, hh = NODE_HEADER_H * z;
    float pinsH = (float)std::max(n.inputs.size(), n.outputs.size()) * PIN_HEIGHT * z;
    RECT r;
    r.left = f2i(rx + 8 * z);
    r.top = f2i(ry + hh + pinsH + 6 * z);   // <-- текст ниже последней строки пинов
    r.right = f2i(rx + rw - 8 * z);
    r.bottom = f2i(ry + rh - 6 * z);
    if (r.bottom < r.top) r.bottom = r.top;
    return r;
}