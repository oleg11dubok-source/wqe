#include "editor.h"
#include <cstdio>

static LRESULT CALLBACK EditProc(HWND, UINT, WPARAM, LPARAM); // fwd

static void EnsureEdit(bool multi, int x, int y, int w, int h) {
    DWORD want = WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL;
    if (multi) want |= ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL;
    if (!g_hedit) {
        g_hedit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", want, x, y, w, h,
            g_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
        SendMessageW(g_hedit, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
        g_oldEdit = (WNDPROC)SetWindowLongPtrW(g_hedit, GWLP_WNDPROC, (LONG_PTR)EditProc);
    }
    else {
        SetWindowLongPtrW(g_hedit, GWL_STYLE, (LONG_PTR)want);
        SetWindowPos(g_hedit, nullptr, x, y, w, h, SWP_SHOWWINDOW | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
}

void EndEdit(bool commit) {
    if (g_editNode < 0 || !g_hedit) return;
    int nid = g_editNode, tid = g_editTask; bool desc = g_editDesc;
    wchar_t buf[1024]; GetWindowTextW(g_hedit, buf, 1024);
    g_editNode = -1; g_editTask = -1; g_editDesc = false;
    ShowWindow(g_hedit, SW_HIDE);
    if (commit) {
        Node* n = FindNode(nid);
        if (n) {
            if (desc) n->desc = buf;
            else if (tid >= 0 && tid < (int)n->tasks.size()) n->tasks[tid].name = buf;
            else n->title = buf;
            n->Recalc();
        }
    }
    SetFocus(g_hwnd);
    InvalidateRect(g_hwnd, nullptr, FALSE);
}
static LRESULT CALLBACK EditProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN) {
        bool multi = ((GetWindowLongPtrW(hWnd, GWL_STYLE) & ES_MULTILINE) != 0);
        if (wParam == VK_RETURN && !multi) { EndEdit(true);  return 0; }
        if (wParam == VK_RETURN && multi && (GetKeyState(VK_CONTROL) & 0x8000)) { EndEdit(true); return 0; }
        if (wParam == VK_ESCAPE) { EndEdit(false); return 0; }
    }
    return CallWindowProcW(g_oldEdit, hWnd, msg, wParam, lParam);
}

static void RepositionEdit() {
    if (g_editNode < 0 || !g_hedit) return;
    Node* n = FindNode(g_editNode); if (!n) return;
    int x, y, w, h;
    if (g_editDesc) { RECT r = NoteBodyRect(*n); x = r.left; y = r.top; w = r.right - r.left; h = r.bottom - r.top; }
    else if (g_editTask >= 0) { RECT cr, tx; TaskRects(*n, g_editTask, cr, tx); x = tx.left; y = tx.top; w = tx.right - tx.left; h = tx.bottom - tx.top; }
    else {
        float rx, ry, rw, rh; NRect(*n, rx, ry, rw, rh);
        int hh = f2i(NODE_HEADER_H * g_zoom); if (hh < 20)hh = 20;
        x = f2i(rx) + 6; y = f2i(ry) + 2; w = f2i(rw) - 12; h = hh - 4;
    }
    SetWindowPos(g_hedit, nullptr, x, y, w, h, SWP_NOZORDER);
}
static void StartRename(int nid) {
    Node* n = FindNode(nid); if (!n) return;
    g_editNode = nid; g_editTask = -1; g_editDesc = false;
    EnsureEdit(false, 0, 0, 10, 10); RepositionEdit();
    SetWindowTextW(g_hedit, n->title.c_str());
    SetFocus(g_hedit); SendMessageW(g_hedit, EM_SETSEL, 0, -1);
}
static void StartTaskRename(int nid, int tid) {
    Node* n = FindNode(nid); if (!n || tid < 0 || tid >= (int)n->tasks.size()) return;
    g_editNode = nid; g_editTask = tid; g_editDesc = false;
    EnsureEdit(false, 0, 0, 10, 10); RepositionEdit();
    SetWindowTextW(g_hedit, n->tasks[tid].name.c_str());
    SetFocus(g_hedit); SendMessageW(g_hedit, EM_SETSEL, 0, -1);
}
static void StartDescEdit(int nid) {
    Node* n = FindNode(nid); if (!n) return;
    g_editNode = nid; g_editTask = -1; g_editDesc = true;
    EnsureEdit(true, 0, 0, 10, 10); RepositionEdit();
    SetWindowTextW(g_hedit, n->desc.c_str());
    SetFocus(g_hedit); SendMessageW(g_hedit, EM_SETSEL, 0, -1);
}

// ---------------- меню ----------------
static void ShowCanvasMenu(int x, int y) {
    HMENU hm = CreatePopupMenu();
    AppendMenuW(hm, MF_STRING, 101, L"Add Node");
    AppendMenuW(hm, MF_STRING, 102, L"Add Start");
    AppendMenuW(hm, MF_STRING, 103, L"Add Process");
    AppendMenuW(hm, MF_STRING, 104, L"Add Code");
    AppendMenuW(hm, MF_STRING, 105, L"Add Math");
    AppendMenuW(hm, MF_STRING, 106, L"Add Print");
    AppendMenuW(hm, MF_STRING, 111, L"Add Task Node");
    AppendMenuW(hm, MF_STRING, 112, L"Add Summary Node");
    AppendMenuW(hm, MF_STRING, 113, L"Add Description");
    AppendMenuW(hm, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hm, MF_STRING, 107, L"Add Group");
    AppendMenuW(hm, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hm, MF_STRING, 108, L"Save Graph");
    AppendMenuW(hm, MF_STRING, 109, L"Load Graph");
    AppendMenuW(hm, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hm, MF_STRING, 110, L"Reset View");
    POINT pt = { x,y }; ClientToScreen(g_hwnd, &pt);
    int cmd = TrackPopupMenu(hm, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, g_hwnd, nullptr);
    DestroyMenu(hm);
    float wx, wy; S2W((float)x, (float)y, wx, wy);
    switch (cmd) {
    case 101: AddNode(L"Node", wx, wy); break;
    case 102: AddNode(L"Start", wx, wy, 46, 160, 80); break;
    case 103: AddNode(L"Process", wx, wy, 140, 80, 180); break;
    case 104: AddNode(L"Code", wx, wy, 50, 130, 200); break;
    case 105: AddNode(L"Math", wx, wy, 200, 120, 50); break;
    case 106: AddNode(L"Print", wx, wy, 200, 180, 60); break;
    case 107: AddGroup(L"Group", wx, wy, 300, 200); break;
    case 108: SaveAs();   break;   // было: Save(L"graph.txt")
    case 109: LoadFrom(); break;   // было: Load(L"graph.txt")
    case 110: g_camX = 0; g_camY = 0; g_zoom = 1; break;
    case 111: {
        int id = AddNode(L"Tasks", wx, wy, 80, 120, 90);
        Node* n = FindNode(id);
        if (n) {
            n->tasks.emplace_back(L"Design", false);
            n->tasks.emplace_back(L"Code", false);
            n->tasks.emplace_back(L"Test", false);
            n->Recalc();
        } break;
    }
    case 112: {
        int id = AddNode(L"Summary", wx, wy, 80, 100, 160);
        Node* n = FindNode(id);
        if (n) { n->isSummary = true; n->Recalc(); } break;
    }
    case 113: {
        int id = AddNode(L"Description", wx, wy, 120, 120, 60, true);
        Node* n = FindNode(id);
        if (n) { n->desc = L"Double-click to edit"; n->Recalc(); } break;
    }
    }
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

static void ShowPinMenu(int x, int y, int pid) {
    Pin* p = FindPin(pid); if (!p) return;
    std::wstring label = L"Delete pin \"" + p->name + L"\"";
    HMENU hm = CreatePopupMenu();
    AppendMenuW(hm, MF_STRING, 401, label.c_str());
    POINT pt = { x,y }; ClientToScreen(g_hwnd, &pt);
    int cmd = TrackPopupMenu(hm, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, g_hwnd, nullptr);
    DestroyMenu(hm);
    if (cmd == 401) DelPin(pid);
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

static void ShowNodeMenu(int x, int y, int nid) {
    HMENU hm = CreatePopupMenu();
    AppendMenuW(hm, MF_STRING, 201, L"Rename");
    AppendMenuW(hm, MF_STRING, 202, L"Add Input Pin");
    AppendMenuW(hm, MF_STRING, 203, L"Add Output Pin");
    AppendMenuW(hm, MF_STRING, 205, L"Add Task");
    AppendMenuW(hm, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hm, MF_STRING, 204, L"Delete Node");
    POINT pt = { x,y }; ClientToScreen(g_hwnd, &pt);
    int cmd = TrackPopupMenu(hm, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, g_hwnd, nullptr);
    DestroyMenu(hm);
    Node* n = FindNode(nid); if (!n) return;
    switch (cmd) {
    case 201: StartRename(nid); break;
    case 202: n->inputs.emplace_back(g_pid++, nid, PinType::Input, L"in " + std::to_wstring(n->inputs.size()), (int)n->inputs.size()); n->Recalc(); break;
    case 203: n->outputs.emplace_back(g_pid++, nid, PinType::Output, L"out " + std::to_wstring(n->outputs.size()), (int)n->outputs.size()); n->Recalc(); break;
    case 204: DelNode(nid); break;
    case 205: n->tasks.emplace_back(L"New task", false); n->Recalc(); break;
    }
    InvalidateRect(g_hwnd, nullptr, FALSE);
}
static void ShowLinkMenu(int x, int y, int lid) {
    HMENU hm = CreatePopupMenu();
    AppendMenuW(hm, MF_STRING, 301, L"Delete Link");
    POINT pt = { x,y }; ClientToScreen(g_hwnd, &pt);
    int cmd = TrackPopupMenu(hm, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, g_hwnd, nullptr);
    DestroyMenu(hm);
    if (cmd == 301) DelLink(lid);
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

// ---------------- WndProc ----------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: Render(hWnd); return 0;
    case WM_SIZE: InvalidateRect(hWnd, nullptr, FALSE); return 0;

    case WM_MOUSEWHEEL: {
        int d = GET_WHEEL_DELTA_WPARAM(wParam);
        POINT pt = { GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam) };
        ScreenToClient(hWnd, &pt);
        float mx = (float)pt.x, my = (float)pt.y, wx, wy;
        S2W(mx, my, wx, wy);
        g_zoom += d > 0 ? 0.1f : -0.1f;
        if (g_zoom < MIN_ZOOM)g_zoom = MIN_ZOOM; if (g_zoom > MAX_ZOOM)g_zoom = MAX_ZOOM;
        g_camX = wx - mx / g_zoom; g_camY = wy - my / g_zoom;
        RepositionEdit();
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        float mx = (float)GET_X_LPARAM(lParam), my = (float)GET_Y_LPARAM(lParam);
        float wx, wy; S2W(mx, my, wx, wy);
        if (g_hedit && g_editNode >= 0) {
            RECT rc; GetWindowRect(g_hedit, &rc);
            POINT pt = { (LONG)mx,(LONG)my }; ClientToScreen(hWnd, &pt);
            if (pt.x<rc.left || pt.x>rc.right || pt.y<rc.top || pt.y>rc.bottom) EndEdit(true);
        }
        // старт линка с пина
        bool hitPin = false;
        for (auto& n : g_nodes) {
            for (auto& p : n.inputs)  if (HitPin(n, p, mx, my)) { g_linking = true; g_linkPin = p.id; g_linkEX = mx; g_linkEY = my; hitPin = true; break; }
            if (hitPin) break;
            for (auto& p : n.outputs) if (HitPin(n, p, mx, my)) { g_linking = true; g_linkPin = p.id; g_linkEX = mx; g_linkEY = my; hitPin = true; break; }
            if (hitPin) break;
        }
        if (hitPin) { SetCapture(hWnd); return 0; }
        // клик по чекбоксу задачи
        for (auto& n : g_nodes) {
            if (!n.tasks.empty() && HitNode(n, mx, my)) {
                for (size_t i = 0;i < n.tasks.size();++i) {
                    RECT cr, tx; TaskRects(n, (int)i, cr, tx);
                    if (mx >= cr.left && mx <= cr.right && my >= cr.top && my <= cr.bottom) {
                        n.tasks[i].done = !n.tasks[i].done;
                        InvalidateRect(hWnd, nullptr, FALSE);
                        return 0;
                    }
                }
            }
        }
        // нода
        for (auto it = g_nodes.rbegin();it != g_nodes.rend();++it) {
            if (HitNode(*it, mx, my)) {
                ClearSel(); g_selNode = it->id; it->selected = true;
                g_drag = ItemType::Node; g_dragId = it->id;
                g_offX = wx - it->x; g_offY = wy - it->y;
                SetCapture(hWnd); return 0;
            }
        }
        // линк
        Link* l = HitLink(mx, my);
        if (l) { ClearSel(); g_selLink = l->id; l->selected = true; return 0; }
        // группа
        for (auto it = g_groups.rbegin();it != g_groups.rend();++it) {
            if (HitGroup(*it, mx, my)) {
                ClearSel(); g_selGroup = it->id; it->selected = true;
                g_drag = ItemType::Group; g_dragId = it->id;
                g_offX = wx - it->x; g_offY = wy - it->y;
                SetCapture(hWnd); return 0;
            }
        }
        ClearSel();
        g_pan = true; g_panSX = mx; g_panSY = my; g_panCX = g_camX; g_panCY = g_camY;
        SetCapture(hWnd);
        return 0;
    }
    case WM_MOUSEMOVE: {
        float mx = (float)GET_X_LPARAM(lParam), my = (float)GET_Y_LPARAM(lParam);
        float wx, wy; S2W(mx, my, wx, wy);
        if (g_linking) { g_linkEX = mx; g_linkEY = my; InvalidateRect(hWnd, nullptr, FALSE); return 0; }
        if (g_drag == ItemType::Node && g_dragId >= 0) {
            Node* n = FindNode(g_dragId);
            if (n) {
                n->x = wx - g_offX; n->y = wy - g_offY;
                if (n->groupId >= 0) {
                    for (auto& gr : g_groups) {
                        if (gr.id == n->groupId) {
                            if (gr.x > n->x - 10)gr.x = n->x - 10;
                            if (gr.y > n->y - 10)gr.y = n->y - 10;
                            float r = n->x + n->w - gr.x + 10, b = n->y + n->h - gr.y + 10;
                            if (gr.w < (int)r)gr.w = (int)r;
                            if (gr.h < (int)b)gr.h = (int)b;
                        }
                    }
                }
                if (g_editNode == n->id) RepositionEdit();
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;
        }
        if (g_drag == ItemType::Group && g_dragId >= 0) {
            Group* gr = nullptr;
            for (auto& g : g_groups) if (g.id == g_dragId) { gr = &g; break; }
            if (gr) {
                float dx = wx - g_offX - gr->x, dy = wy - g_offY - gr->y;
                gr->x += dx; gr->y += dy;
                for (auto& n : g_nodes) if (n.groupId == gr->id) { n.x += dx; n.y += dy; }
                g_offX = wx - gr->x; g_offY = wy - gr->y;
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;
        }
        if (g_pan) {
            g_camX = g_panCX - (mx - g_panSX) / g_zoom;
            g_camY = g_panCY - (my - g_panSY) / g_zoom;
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        float mx = (float)GET_X_LPARAM(lParam), my = (float)GET_Y_LPARAM(lParam);
        if (g_linking) {
            Pin* tp = nullptr;
            for (auto& n : g_nodes) {
                for (auto& p : n.inputs)  if (HitPin(n, p, mx, my)) { tp = &p; break; }
                if (tp) break;
                for (auto& p : n.outputs) if (HitPin(n, p, mx, my)) { tp = &p; break; }
                if (tp) break;
            }
            if (tp && tp->id != g_linkPin) {
                Pin* fr = FindPin(g_linkPin);
                if (fr && fr->type != tp->type) {
                    bool ex = false;
                    for (auto& l : g_links)
                        if ((l.fromPin == g_linkPin && l.toPin == tp->id) || (l.fromPin == tp->id && l.toPin == g_linkPin)) { ex = true; break; }
                    if (!ex) {
                        int fid = (fr->type == PinType::Output) ? g_linkPin : tp->id;
                        int tid = (fr->type == PinType::Output) ? tp->id : g_linkPin;
                        g_links.emplace_back(g_lid++, fid, tid);
                    }
                }
            }
            g_linking = false; g_linkPin = -1;
            ReleaseCapture();
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        g_drag = ItemType::None; g_dragId = -1; g_pan = false;
        ReleaseCapture();
        return 0;
    }
    case WM_RBUTTONUP: {
        float mx = (float)GET_X_LPARAM(lParam), my = (float)GET_Y_LPARAM(lParam);
        POINT pt = { GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam) };
        ClientToScreen(hWnd, &pt);

        // 1) пин
        Pin* hp = nullptr;
        for (auto& n : g_nodes) {
            for (auto& p : n.inputs)  if (HitPin(n, p, mx, my)) { hp = &p; break; }
            if (hp) break;
            for (auto& p : n.outputs) if (HitPin(n, p, mx, my)) { hp = &p; break; }
            if (hp) break;
        }
        if (hp) { ShowPinMenu(pt.x, pt.y, hp->id); return 0; }

        // 2) нода / линк / канвас — как раньше
        Node* hn = nullptr;
        for (auto it = g_nodes.rbegin();it != g_nodes.rend();++it)
            if (HitNode(*it, mx, my)) { hn = &(*it); break; }
        if (hn) { ShowNodeMenu(pt.x, pt.y, hn->id); return 0; }
        Link* l = HitLink(mx, my);
        if (l) { ShowLinkMenu(pt.x, pt.y, l->id); return 0; }
        ShowCanvasMenu(pt.x, pt.y);
        return 0;
    }

    case WM_LBUTTONDBLCLK: {
        float mx = (float)GET_X_LPARAM(lParam), my = (float)GET_Y_LPARAM(lParam);
        for (auto it = g_nodes.rbegin();it != g_nodes.rend();++it) {
            if (HitNode(*it, mx, my)) {
                // даблклик по тексту задачи -> rename задачи
                if (!it->tasks.empty()) {
                    for (size_t i = 0;i < it->tasks.size();++i) {
                        RECT cr, tx; TaskRects(*it, (int)i, cr, tx);
                        if (mx >= tx.left && mx <= tx.right && my >= tx.top && my <= tx.bottom) { StartTaskRename(it->id, (int)i); return 0; }
                        if (mx >= cr.left && mx <= cr.right && my >= cr.top && my <= cr.bottom) return 0;
                    }
                }
                if (it->isNote) { StartDescEdit(it->id); return 0; }
                StartRename(it->id);
                return 0;
            }
        }
        return 0;
    }
    case WM_KEYDOWN: {
        if (wParam == VK_DELETE) {
            if (g_selNode >= 0) DelNode(g_selNode);
            else if (g_selLink >= 0) DelLink(g_selLink);
            else if (g_selGroup >= 0) DelGroup(g_selGroup);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            if (wParam == 'S') { SaveAs();   return 0; }   // Ctrl+S -> диалог сохранения
            if (wParam == 'O') { LoadFrom(); return 0; }   // Ctrl+O -> диалог загрузки
            if (wParam == 'G') {
                if (g_selNode >= 0) {
                    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
                    std::vector<int> ids;
                    for (auto& n : g_nodes) {
                        if (n.selected || n.id == g_selNode) {
                            ids.push_back(n.id);
                            if (minX > n.x)minX = n.x; if (minY > n.y)minY = n.y;
                            if (maxX < n.x + n.w)maxX = n.x + n.w; if (maxY < n.y + n.h)maxY = n.y + n.h;
                        }
                    }
                    if (ids.size() > 1) {
                        int gid = AddGroup(L"Group", minX - 20, minY - 40, maxX - minX + 40, maxY - minY + 60);
                        for (int id : ids) { Node* n = FindNode(id); if (n) n->groupId = gid; }
                        InvalidateRect(hWnd, nullptr, FALSE);
                    }
                }
                return 0;
            }
        }
        return 0;
    }
    case WM_COMMAND: {
        if (g_hedit && (HWND)lParam == g_hedit && HIWORD(wParam) == EN_KILLFOCUS) { EndEdit(true); return 0; }
        return 0;
    }
    case WM_DESTROY:
        if (g_hedit) DestroyWindow(g_hedit);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}