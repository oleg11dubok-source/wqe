#include "editor.h"
#include <cmath>
#include <cstdio>

static HFONT MakeFont(int size, int weight) {
    return CreateFontW(size, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

static void DrawGrid(HDC hdc, int W, int H) {
    HBRUSH bb = CreateSolidBrush(RGB(28, 28, 32)); RECT r = { 0,0,W,H }; FillRect(hdc, &r, bb); DeleteObject(bb);
    HPEN gp = CreatePen(PS_SOLID, 1, RGB(45, 45, 55)); HPEN op = (HPEN)SelectObject(hdc, gp);
    float step = (float)GRID_SIZE * g_zoom;
    float sx = fmodf(-g_camX * g_zoom, step), sy = fmodf(-g_camY * g_zoom, step);
    if (sx < 0)sx += step; if (sy < 0)sy += step;
    for (float x = sx;x < W;x += step) { MoveToEx(hdc, f2i(x), 0, nullptr); LineTo(hdc, f2i(x), H); }
    for (float y = sy;y < H;y += step) { MoveToEx(hdc, 0, f2i(y), nullptr); LineTo(hdc, W, f2i(y)); }
    SelectObject(hdc, op); DeleteObject(gp);
}
static void DrawArrow(HDC hdc, float x1, float y1, float x2, float y2, float sz, COLORREF c) {
    float dx = x2 - x1, dy = y2 - y1, len = sqrtf(dx * dx + dy * dy); if (len < 1)return;
    dx /= len; dy /= len; float nx = -dy, ny = dx;
    POINT pts[3];
    pts[0].x = f2i(x2); pts[0].y = f2i(y2);
    pts[1].x = f2i(x2 - sz * (dx + nx * 0.5f)); pts[1].y = f2i(y2 - sz * (dy + ny * 0.5f));
    pts[2].x = f2i(x2 - sz * (dx - nx * 0.5f)); pts[2].y = f2i(y2 - sz * (dy - ny * 0.5f));
    HBRUSH br = CreateSolidBrush(c); HBRUSH ob = (HBRUSH)SelectObject(hdc, br);
    HPEN pn = CreatePen(PS_SOLID, 1, c); HPEN op = (HPEN)SelectObject(hdc, pn);
    Polygon(hdc, pts, 3);
    SelectObject(hdc, ob); SelectObject(hdc, op); DeleteObject(br); DeleteObject(pn);
}
static void DrawBezier(HDC hdc, float x1, float y1, float x2, float y2, bool sel) {
    float c1x = x1 + 80 * g_zoom, c1y = y1, c2x = x2 - 80 * g_zoom, c2y = y2;
    COLORREF col = sel ? RGB(100, 200, 255) : RGB(160, 160, 170);
    HPEN pn = CreatePen(PS_SOLID, sel ? 3 : 2, col); HPEN op = (HPEN)SelectObject(hdc, pn);
    POINT pts[4];
    pts[0].x = f2i(x1); pts[0].y = f2i(y1); pts[1].x = f2i(c1x); pts[1].y = f2i(c1y);
    pts[2].x = f2i(c2x); pts[2].y = f2i(c2y); pts[3].x = f2i(x2); pts[3].y = f2i(y2);
    PolyBezier(hdc, pts, 4);
    SelectObject(hdc, op); DeleteObject(pn);
    DrawArrow(hdc, c2x, c2y, x2, y2, 10, col);
}
static void DrawLinks(HDC hdc) {
    for (auto& l : g_links) {
        Pin* a = FindPin(l.fromPin); Pin* b = FindPin(l.toPin); if (!a || !b) continue;
        Node* na = NodeByPin(l.fromPin); Node* nb = NodeByPin(l.toPin); if (!na || !nb) continue;
        float p1x, p1y, p2x, p2y; PPos(*na, *a, p1x, p1y); PPos(*nb, *b, p2x, p2y);
        DrawBezier(hdc, p1x, p1y, p2x, p2y, l.selected);
    }
    if (g_linking) {
        Pin* a = FindPin(g_linkPin);
        if (a) {
            Node* na = NodeByPin(g_linkPin);
            if (na) { float p1x, p1y; PPos(*na, *a, p1x, p1y); DrawBezier(hdc, p1x, p1y, g_linkEX, g_linkEY, true); }
        }
    }
}
static void DrawPie(HDC hdc, float cx, float cy, float r, float pct, COLORREF done, COLORREF bg, COLORREF txt) {
    int icx = f2i(cx), icy = f2i(cy), ir = f2i(r); if (ir < 4)ir = 4;
    // фон = невыполнено (серый)
    {
        HBRUSH bb = CreateSolidBrush(bg); HPEN pb = CreatePen(PS_SOLID, 1, bg);
        HBRUSH ob = (HBRUSH)SelectObject(hdc, bb); HPEN op = (HPEN)SelectObject(hdc, pb);
        Ellipse(hdc, icx - ir, icy - ir, icx + ir, icy + ir);
        SelectObject(hdc, ob); SelectObject(hdc, op); DeleteObject(bb); DeleteObject(pb);
    }
    if (pct > 0.5f) {
        if (pct >= 99.5f) {
            HBRUSH fb = CreateSolidBrush(done); HPEN pf = CreatePen(PS_SOLID, 1, done);
            HBRUSH ob = (HBRUSH)SelectObject(hdc, fb); HPEN op = (HPEN)SelectObject(hdc, pf);
            Ellipse(hdc, icx - ir, icy - ir, icx + ir, icy + ir);
            SelectObject(hdc, ob); SelectObject(hdc, op); DeleteObject(fb); DeleteObject(pf);
        }
        else {
            // конечная точка сектора: pct% по часовой от 12:00
            float ang = (-90.0f + pct * 3.6f) * 3.14159265f / 180.0f;
            int xe = icx + (int)(ir * cosf(ang)), ye = icy + (int)(ir * sinf(ang));
            HBRUSH fb = CreateSolidBrush(done); HPEN pf = CreatePen(PS_SOLID, 1, done);
            HBRUSH ob = (HBRUSH)SelectObject(hdc, fb); HPEN op = (HPEN)SelectObject(hdc, pf);
            // GDI метёт дугу ПРОТИВ часовой от 1-й точки ко 2-й,
            // поэтому передаём (xe,ye) первой, а 12:00 второй —
            // получаем сектор 12:00 -> P по часовой (заполнение справа налево)
            Pie(hdc, icx - ir, icy - ir, icx + ir, icy + ir, xe, ye, icx, icy - ir);
            SelectObject(hdc, ob); SelectObject(hdc, op); DeleteObject(fb); DeleteObject(pf);
        }
    }
    wchar_t buf[16]; swprintf_s(buf, L"%.0f%%", pct);
    int fh = f2i(14 * g_zoom); if (fh < 9)fh = 9;
    HFONT fn = CreateFontW(fh, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT of = (HFONT)SelectObject(hdc, fn);
    SetTextColor(hdc, txt); SetBkMode(hdc, TRANSPARENT);
    RECT rc = { icx - ir,icy - 10,icx + ir,icy + 10 };
    DrawTextW(hdc, buf, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, of); DeleteObject(fn);
}

static void DrawNodes(HDC hdc) {
    float z = g_zoom;
    HFONT f = MakeFont(14, FW_NORMAL);
    HFONT fb = MakeFont(14, FW_BOLD);
    HFONT of = (HFONT)SelectObject(hdc, f);
    SetBkMode(hdc, TRANSPARENT);
    for (auto& n : g_nodes) {
        float frx, fry, frw, frh; NRect(n, frx, fry, frw, frh);
        int rx = f2i(frx), ry = f2i(fry), rw = f2i(frw), rh = f2i(frh);
        bool sel = (n.id == g_selNode);
        int crn = f2i(8 * z); if (crn < 2)crn = 2;
        // тень
        HBRUSH sh = CreateSolidBrush(RGB(15, 15, 15)); RECT sr = { rx + 3,ry + 4,rx + 3 + rw,ry + 4 + rh }; FillRect(hdc, &sr, sh); DeleteObject(sh);
        // тело
        COLORREF bgc = sel ? RGB(55, 55, 68) : RGB(42, 42, 52);
        HBRUSH bgb = CreateSolidBrush(bgc); HBRUSH ob = (HBRUSH)SelectObject(hdc, bgb);
        HPEN bp0 = CreatePen(PS_SOLID, 1, bgc); HPEN op0 = (HPEN)SelectObject(hdc, bp0);
        RoundRect(hdc, rx, ry, rx + rw, ry + rh, crn * 2, crn * 2);
        SelectObject(hdc, ob); SelectObject(hdc, op0); DeleteObject(bgb); DeleteObject(bp0);
        // шапка
        int hh = f2i(NODE_HEADER_H * z);
        HBRUSH hb = CreateSolidBrush(RGB(n.cr, n.cg, n.cb));
        HPEN hp = CreatePen(PS_SOLID, 1, RGB(n.cr, n.cg, n.cb));
        ob = (HBRUSH)SelectObject(hdc, hb); op0 = (HPEN)SelectObject(hdc, hp);
        RoundRect(hdc, rx, ry, rx + rw, ry + hh, crn * 2, crn * 2);
        SelectObject(hdc, ob); SelectObject(hdc, op0); DeleteObject(hb); DeleteObject(hp);
        // рамка
        COLORREF bc = sel ? RGB(100, 200, 255) : RGB(70, 70, 85);
        HPEN bp = CreatePen(PS_SOLID, sel ? 2 : 1, bc); HPEN op = (HPEN)SelectObject(hdc, bp);
        HBRUSH nb = (HBRUSH)GetStockObject(NULL_BRUSH); ob = (HBRUSH)SelectObject(hdc, nb);
        RoundRect(hdc, rx, ry, rx + rw, ry + rh, crn * 2, crn * 2);
        SelectObject(hdc, op); SelectObject(hdc, ob); DeleteObject(bp);
        // разделитель
        HPEN sp = CreatePen(PS_SOLID, 1, RGB(50, 50, 60)); op = (HPEN)SelectObject(hdc, sp);
        MoveToEx(hdc, rx + crn, ry + hh, nullptr); LineTo(hdc, rx + rw - crn, ry + hh);
        SelectObject(hdc, op); DeleteObject(sp);
        // заголовок
        SetTextColor(hdc, RGB(235, 235, 235)); SelectObject(hdc, fb);
        RECT tr = { rx + 10,ry,rx + rw - 10,ry + hh };
        DrawTextW(hdc, n.title.c_str(), -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        // ---- задачи: диаграмма по центру + масштабируемый список ----
        if (!n.tasks.empty()) {
            float pcx, pcy, pr; TaskPie(n, pcx, pcy, pr);
            DrawPie(hdc, pcx, pcy, pr, n.Progress(), RGB(100, 220, 120), RGB(70, 70, 75), RGB(255, 255, 255)); 
            
            for (size_t i = 0;i < n.tasks.size();++i) {
                RECT cr, txr; TaskRects(n, (int)i, cr, txr);
                HBRUSH cb = CreateSolidBrush(n.tasks[i].done ? RGB(100, 220, 120) : RGB(60, 60, 70));
                FillRect(hdc, &cr, cb); DeleteObject(cb);
                HPEN cp = CreatePen(PS_SOLID, 1, RGB(180, 180, 190)); op = (HPEN)SelectObject(hdc, cp);
                HBRUSH nul = (HBRUSH)GetStockObject(NULL_BRUSH); ob = (HBRUSH)SelectObject(hdc, nul);
                Rectangle(hdc, cr.left, cr.top, cr.right, cr.bottom);
                SelectObject(hdc, op); SelectObject(hdc, ob); DeleteObject(cp);
                SetTextColor(hdc, n.tasks[i].done ? RGB(140, 140, 150) : RGB(220, 220, 230));
                DrawTextW(hdc, n.tasks[i].name.c_str(), -1, &txr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }
        }
        // ---- summary ----
        if (n.isSummary) {
            std::vector<const Node*> src;
            for (auto& l : g_links) {
                Pin* to = FindPin(l.toPin);
                if (to && to->nodeId == n.id) {
                    Pin* fr = FindPin(l.fromPin);
                    if (fr) {
                        Node* s = FindNode(fr->nodeId);
                        if (s && (!s->tasks.empty() || s->isSummary)) src.push_back(s);
                    }
                }
            }
            int nh = NODE_HEADER_H + 140 + (int)src.size() * 22 + 10;
            if (n.h < nh) n.h = nh;
            float tot = 0; for (auto s : src) tot += s->Progress();
            float avg = src.empty() ? 0.0f : tot / (float)src.size();
            DrawPie(hdc, rx + rw / 2.0f, ry + hh + 55 * z, 36 * z, avg, RGB(100, 200, 255), RGB(70, 70, 75), RGB(255, 255, 255)); // su
            wchar_t buf[32];
            swprintf_s(buf, L"Total: %.0f%%", avg);
            SetTextColor(hdc, RGB(200, 200, 210));
            RECT tr3 = { rx,f2i(ry + hh + 100 * z),rx + rw,f2i(ry + hh + 120 * z) };
            DrawTextW(hdc, buf, -1, &tr3, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            int ly = f2i(ry + hh + 130 * z);
            for (auto s : src) {
                swprintf_s(buf, L"%.0f%%", s->Progress());
                SetTextColor(hdc, RGB(180, 180, 190));
                RECT nr = { rx + 10,ly,rx + rw / 2,ly + 18 };
                DrawTextW(hdc, s->title.c_str(), -1, &nr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                SetTextColor(hdc, RGB(140, 200, 255));
                RECT pr = { rx + rw / 2,ly,rx + rw - 10,ly + 18 };
                DrawTextW(hdc, buf, -1, &pr, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
                ly += f2i(20 * z);
            }
        }
        // ---- description ----
        if (n.isNote) {
            RECT br = NoteBodyRect(n);
            SetTextColor(hdc, RGB(200, 205, 215));
            SelectObject(hdc, f);
            RECT m = br; DrawTextW(hdc, n.desc.c_str(), -1, &m, DT_CALCRECT | DT_WORDBREAK);
            int needH = m.bottom - m.top + 8;
            int haveH = br.bottom - br.top;
            if (needH > haveH) n.h += (needH - haveH);   // нода растёт под текст
            DrawTextW(hdc, n.desc.c_str(), -1, &br, DT_LEFT | DT_TOP | DT_WORDBREAK);
        }
        // ---- пины ----
        SelectObject(hdc, f);
        for (auto& p : n.inputs) {
            float px, py; PPos(n, p, px, py);
            int x = f2i(px), y = f2i(py), r = f2i(PIN_RADIUS * z); if (r < 3)r = 3;
            HBRUSH pbr = CreateSolidBrush(RGB(100, 220, 120)); ob = (HBRUSH)SelectObject(hdc, pbr);
            Ellipse(hdc, x - r, y - r, x + r, y + r); SelectObject(hdc, ob); DeleteObject(pbr);
            HPEN ppn = CreatePen(PS_SOLID, 1, RGB(40, 40, 40)); op = (HPEN)SelectObject(hdc, ppn);
            Ellipse(hdc, x - r, y - r, x + r, y + r); SelectObject(hdc, op); DeleteObject(ppn);
            SetTextColor(hdc, RGB(200, 200, 210));
            RECT nr = { x + r + 5,y - 10,rx + rw / 2,y + 10 };
            DrawTextW(hdc, p.name.c_str(), -1, &nr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
        for (auto& p : n.outputs) {
            float px, py; PPos(n, p, px, py);
            int x = f2i(px), y = f2i(py), r = f2i(PIN_RADIUS * z); if (r < 3)r = 3;
            HBRUSH pbr = CreateSolidBrush(RGB(100, 160, 255)); ob = (HBRUSH)SelectObject(hdc, pbr);
            Ellipse(hdc, x - r, y - r, x + r, y + r); SelectObject(hdc, ob); DeleteObject(pbr);
            HPEN ppn = CreatePen(PS_SOLID, 1, RGB(40, 40, 40)); op = (HPEN)SelectObject(hdc, ppn);
            Ellipse(hdc, x - r, y - r, x + r, y + r); SelectObject(hdc, op); DeleteObject(ppn);
            SetTextColor(hdc, RGB(200, 200, 210));
            RECT nr = { rx + rw / 2,y - 10,x - r - 5,y + 10 };
            DrawTextW(hdc, p.name.c_str(), -1, &nr, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        }
    }
    SelectObject(hdc, of); DeleteObject(f); DeleteObject(fb);
}
static void DrawGroups(HDC hdc) {
    HFONT f = MakeFont(13, FW_BOLD); HFONT of = (HFONT)SelectObject(hdc, f); SetBkMode(hdc, TRANSPARENT);
    for (auto& g : g_groups) {
        float frx, fry, frw, frh; GRect(g, frx, fry, frw, frh);
        int rx = f2i(frx), ry = f2i(fry), rw = f2i(frw), rh = f2i(frh);
        bool sel = (g.id == g_selGroup);
        HBRUSH b = CreateSolidBrush(sel ? RGB(70, 90, 140) : RGB(50, 70, 110));
        RECT r = { rx,ry,rx + rw,ry + rh }; FillRect(hdc, &r, b); DeleteObject(b);
        HPEN p = CreatePen(PS_DASH, 1, sel ? RGB(100, 170, 255) : RGB(70, 110, 170));
        HPEN op = (HPEN)SelectObject(hdc, p);
        HBRUSH nb = (HBRUSH)GetStockObject(NULL_BRUSH); HBRUSH ob = (HBRUSH)SelectObject(hdc, nb);
        Rectangle(hdc, rx, ry, rx + rw, ry + rh);
        SelectObject(hdc, op); SelectObject(hdc, ob); DeleteObject(p);
        SetTextColor(hdc, RGB(170, 190, 220));
        RECT tr = { rx + 10,ry + 6,rx + rw - 10,ry + f2i(GROUP_HEADER_H * g_zoom) };
        DrawTextW(hdc, g.title.c_str(), -1, &tr, DT_LEFT | DT_TOP | DT_SINGLELINE);
    }
    SelectObject(hdc, of); DeleteObject(f);
}

void Render(HWND hWnd) {
    PAINTSTRUCT ps; HDC hdc = BeginPaint(hWnd, &ps);
    RECT rc; GetClientRect(hWnd, &rc);
    int W = rc.right - rc.left, H = rc.bottom - rc.top;
    HDC mdc = CreateCompatibleDC(hdc); HBITMAP mb = CreateCompatibleBitmap(hdc, W, H);
    HBITMAP omb = (HBITMAP)SelectObject(mdc, mb);
    DrawGrid(mdc, W, H); DrawGroups(mdc); DrawLinks(mdc); DrawNodes(mdc);
    HFONT hf = MakeFont(13, FW_NORMAL); HFONT of = (HFONT)SelectObject(mdc, hf);
    SetBkMode(mdc, TRANSPARENT); SetTextColor(mdc, RGB(150, 150, 160));
    wchar_t buf[512];
    swprintf_s(buf, L"Nodes:%d Links:%d Groups:%d Zoom:%.0f%% | Ctrl+S/O=Save/Load Ctrl+G=Group",
        (int)g_nodes.size(), (int)g_links.size(), (int)g_groups.size(), g_zoom * 100);
    TextOutW(mdc, 12, 10, buf, (int)wcslen(buf));
    swprintf_s(buf, L"Wheel=Zoom RMB=Menu DragPin=Link Del=Delete DblClick=Rename / task rename / description");
    TextOutW(mdc, 12, H - 22, buf, (int)wcslen(buf));
    SelectObject(mdc, of); DeleteObject(hf);
    BitBlt(hdc, 0, 0, W, H, mdc, 0, 0, SRCCOPY);
    SelectObject(mdc, omb); DeleteObject(mb); DeleteDC(mdc);
    EndPaint(hWnd, &ps);
}