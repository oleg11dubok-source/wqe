#include "editor.h"
#include <sstream>
#include <cstring>
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")

static bool WriteFileAll(const wchar_t* fn, const std::wstring& text) {
    HANDLE h = CreateFileW(fn, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    int n = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0, nullptr, nullptr);
    std::string u(n, 0);
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), &u[0], n, nullptr, nullptr);
    const char bom[3] = { char(0xEF),char(0xBB),char(0xBF) };
    DWORD wr; WriteFile(h, bom, 3, &wr, nullptr);
    if (n > 0) WriteFile(h, u.data(), (DWORD)u.size(), &wr, nullptr);
    CloseHandle(h); return true;
}
static bool ReadFileAll(const wchar_t* fn, std::wstring& out) {
    HANDLE h = CreateFileW(fn, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD sz = GetFileSize(h, nullptr);
    std::string buf(sz, 0);
    DWORD rd; if (sz > 0) ReadFile(h, &buf[0], sz, &rd, nullptr);
    CloseHandle(h);
    if (sz >= 3 && std::memcmp(buf.data(), "\xEF\xBB\xBF", 3) == 0) buf.erase(0, 3);
    if (buf.empty()) { out.clear(); return true; }
    int n = MultiByteToWideChar(CP_UTF8, 0, buf.data(), (int)buf.size(), nullptr, 0);
    out.assign(n, 0);
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, buf.data(), (int)buf.size(), &out[0], n);
    return true;
}

static std::wstring TrimLead(const std::wstring& s) { size_t i = 0; while (i < s.size() && s[i] == L' ') ++i; return s.substr(i); }
static bool StartsWith(const std::wstring& s, const wchar_t* p) { return s.rfind(p, 0) == 0; }
static std::wstring Escape(const std::wstring& s) {
    std::wstring r; r.reserve(s.size());
    for (wchar_t c : s) {
        if (c == L'\\') r += L"\\\\";
        else if (c == L'\n') r += L"\\n";
        else if (c == L'\r') r += L"\\r";
        else if (c == L'\t') r += L"\\t";
        else r += c;
    }
    return r;
}
static std::wstring Unescape(const std::wstring& s) {
    std::wstring r; r.reserve(s.size());
    for (size_t i = 0;i < s.size();++i) {
        if (s[i] == L'\\' && i + 1 < s.size()) {
            wchar_t n = s[++i];
            if (n == L'n') r += '\n'; else if (n == L'r') r += '\r';
            else if (n == L't') r += '\t'; else r += n;
        }
        else r += s[i];
    }
    return r;
}
static std::vector<std::wstring> SplitLines(const std::wstring& text) {
    std::vector<std::wstring> out; size_t start = 0;
    while (start <= text.size()) {
        size_t p = text.find(L'\n', start);
        if (p == std::wstring::npos) { out.push_back(text.substr(start)); break; }
        std::wstring l = text.substr(start, p - start);
        if (!l.empty() && l.back() == L'\r') l.pop_back();
        out.push_back(l); start = p + 1;
    }
    return out;
}
struct LineReader {
    std::vector<std::wstring> L; size_t i = 0;
    bool has() const { return i < L.size(); }
    const std::wstring& peek() const { static std::wstring e; return i < L.size() ? L[i] : e; }
    std::wstring next() { return i < L.size() ? L[i++] : std::wstring(); }
};

// ---------- SAVE (#v3, desc экранируется) ----------
void Save(const wchar_t* fn) {
    std::wstringstream f;
    f << L"#v3\n";
    f << L"NODES " << g_nodes.size() << L"\n";
    for (auto& n : g_nodes) {
        f << L"NODE " << n.id << L' ' << n.x << L' ' << n.y << L' ' << n.cr << L' ' << n.cg << L' ' << n.cb << L' '
            << (n.isSummary ? 1 : 0) << L' ' << (n.isNote ? 1 : 0) << L"\n";
        f << n.title << L"\n";
        f << L"PINS " << n.inputs.size() << L' ' << n.outputs.size() << L"\n";
        for (auto& p : n.inputs)  f << L"IN " << p.id << L' ' << p.idx << L' ' << p.name << L"\n";
        for (auto& p : n.outputs) f << L"OUT " << p.id << L' ' << p.idx << L' ' << p.name << L"\n";
        f << L"TASKS " << n.tasks.size() << L"\n";
        for (auto& t : n.tasks) f << L"TASK " << (t.done ? 1 : 0) << L' ' << t.name << L"\n";
        f << L"DESC\n" << Escape(n.desc) << L"\n";
    }
    f << L"LINKS " << g_links.size() << L"\n";
    for (auto& l : g_links) f << L"LINK " << l.id << L' ' << l.fromPin << L' ' << l.toPin << L"\n";
    f << L"GROUPS " << g_groups.size() << L"\n";
    for (auto& g : g_groups) {
        f << L"GROUP " << g.id << L' ' << g.x << L' ' << g.y << L' ' << g.w << L' ' << g.h << L"\n";
        f << g.title << L"\n";
    }
    WriteFileAll(fn, f.str());
}

// ---------- LOAD ----------
static void AfterLoad() {
    g_nid = 1; g_pid = 1000; g_lid = 5000; g_gid = 100;
    for (auto& n : g_nodes) {
        if (n.id >= g_nid)g_nid = n.id + 1;
        for (auto& p : n.inputs)  if (p.id >= g_pid)g_pid = p.id + 1;
        for (auto& p : n.outputs) if (p.id >= g_pid)g_pid = p.id + 1;
    }
    for (auto& l : g_links)  if (l.id >= g_lid)g_lid = l.id + 1;
    for (auto& g : g_groups) if (g.id >= g_gid)g_gid = g.id + 1;
    ClearSel();
}

static bool LoadV3(LineReader& lr) {
    { std::wstringstream ss(lr.next()); std::wstring t; int nc; ss >> t >> nc; if (t != L"NODES") return false; }
    while (lr.has() && !StartsWith(lr.peek(), L"LINKS")) {
        if (!StartsWith(lr.peek(), L"NODE ")) return false;
        std::wstringstream ns(lr.next());
        std::wstring t; int id, cr, cg, cb, sm, nt; float x, y;
        ns >> t >> id >> x >> y >> cr >> cg >> cb >> sm >> nt;
        Node n(id, lr.next(), x, y, cr, cg, cb); n.isSummary = (sm != 0); n.isNote = (nt != 0);
        {
            std::wstringstream ps(lr.next()); std::wstring pt; int ic, oc; ps >> pt >> ic >> oc; if (pt != L"PINS") return false;
            for (int j = 0;j < ic;j++) {
                std::wstringstream qs(lr.next()); std::wstring kt; int pid, idx; qs >> kt >> pid >> idx;
                std::wstring rest; std::getline(qs, rest); n.inputs.emplace_back(pid, n.id, PinType::Input, TrimLead(rest), idx);
            }
            for (int j = 0;j < oc;j++) {
                std::wstringstream qs(lr.next()); std::wstring kt; int pid, idx; qs >> kt >> pid >> idx;
                std::wstring rest; std::getline(qs, rest); n.outputs.emplace_back(pid, n.id, PinType::Output, TrimLead(rest), idx);
            }
        }
        {
            std::wstringstream ts(lr.next()); std::wstring tt; int tc; ts >> tt >> tc; if (tt != L"TASKS") return false;
            for (int j = 0;j < tc;j++) {
                std::wstringstream qs(lr.next()); std::wstring kt; int d; qs >> kt >> d;
                std::wstring rest; std::getline(qs, rest); n.tasks.emplace_back(TrimLead(rest), d != 0);
            }
        }
        if (lr.next() != L"DESC") return false;
        std::wstring d = lr.has() ? lr.next() : L"";
        // спасение старых битых файлов: сырые строки desc до следующего NODE/LINKS
        while (lr.has() && !StartsWith(lr.peek(), L"NODE ") && !StartsWith(lr.peek(), L"LINKS"))
            d += L"\n" + lr.next();
        n.desc = Unescape(d);
        n.Recalc(); g_nodes.push_back(n);
    }
    {
        std::wstringstream ss(lr.next()); std::wstring t; int lc; ss >> t >> lc; if (t != L"LINKS") return false;
        for (int i = 0;i < lc;i++) {
            std::wstringstream ls(lr.next()); std::wstring lt; int id, fr, to; ls >> lt >> id >> fr >> to;
            if (lt != L"LINK") return false; g_links.emplace_back(id, fr, to);
        }
    }
    {
        std::wstringstream ss(lr.next()); std::wstring t; int gc; ss >> t >> gc; if (t != L"GROUPS") return false;
        for (int i = 0;i < gc;i++) {
            std::wstringstream gs(lr.next()); std::wstring gt; int id, w, h; float gx, gy;
            gs >> gt >> id >> gx >> gy >> w >> h; if (gt != L"GROUP") return false;
            g_groups.emplace_back(id, lr.next(), gx, gy, w, h);
        }
    }
    return true;
}

static bool LoadV2(LineReader& lr) {
    std::wstringstream hs(lr.next()); std::wstring t; int nc; hs >> t >> nc; if (t != L"NODES") return false;
    for (int i = 0;i < nc;i++) {
        std::wstringstream ns(lr.next());
        int id, cr, cg, cb, sm; float x, y;
        ns >> id >> x >> y >> cr >> cg >> cb >> sm;
        std::wstring title; std::getline(ns, title);
        Node n(id, TrimLead(title), x, y, cr, cg, cb); n.isSummary = (sm != 0);
        std::wstringstream ps(lr.next()); std::wstring pt; int ic, oc; ps >> pt >> ic >> oc; if (pt != L"PINS") return false;
        for (int j = 0;j < ic;j++) {
            std::wstringstream qs(lr.next()); int pid, idx; qs >> pid >> idx;
            std::wstring rest; std::getline(qs, rest); n.inputs.emplace_back(pid, n.id, PinType::Input, TrimLead(rest), idx);
        }
        for (int j = 0;j < oc;j++) {
            std::wstringstream qs(lr.next()); int pid, idx; qs >> pid >> idx;
            std::wstring rest; std::getline(qs, rest); n.outputs.emplace_back(pid, n.id, PinType::Output, TrimLead(rest), idx);
        }
        std::wstringstream ts(lr.next()); std::wstring tt; int tc; ts >> tt >> tc; if (tt != L"TASKS") return false;
        for (int j = 0;j < tc;j++) {
            std::wstringstream qs(lr.next()); int d; qs >> d;
            std::wstring rest; std::getline(qs, rest); n.tasks.emplace_back(TrimLead(rest), d != 0);
        }
        n.Recalc(); g_nodes.push_back(n);
    }
    {
        std::wstringstream ss(lr.next()); std::wstring t; int lc; ss >> t >> lc; if (t != L"LINKS") return false;
        for (int i = 0;i < lc;i++) { std::wstringstream ls(lr.next()); int id, fr, to; ls >> id >> fr >> to; g_links.emplace_back(id, fr, to); }
    }
    {
        std::wstringstream ss(lr.next()); std::wstring t; int gc; ss >> t >> gc; if (t != L"GROUPS") return false;
        for (int i = 0;i < gc;i++) {
            std::wstringstream gs(lr.next()); int id, w, h; float gx, gy; gs >> id >> gx >> gy >> w >> h;
            std::wstring rest; std::getline(gs, rest); g_groups.emplace_back(id, TrimLead(rest), gx, gy, w, h);
        }
    }
    return true;
}

static std::wstring s_err;

bool Load(const wchar_t* fn) {
    std::wstring text;
    if (!ReadFileAll(fn, text)) { s_err = L"file not readable"; return false; }
    auto lines = SplitLines(text);
    if (lines.empty() || lines[0].empty()) { s_err = L"file is empty"; return false; }
    std::wstring ver = lines[0];
    LineReader lr; lr.L.assign(lines.begin() + 1, lines.end());
    auto bakN = g_nodes;
    auto bakL = g_links;
    auto bakG = g_groups;
    g_nodes.clear(); g_links.clear(); g_groups.clear();
    if (g_hedit) ShowWindow(g_hedit, SW_HIDE);
    g_editNode = -1; g_editTask = -1; g_editDesc = false;
    bool ok = (ver == L"#v3") ? LoadV3(lr) : (ver == L"#v2") ? LoadV2(lr) : false;
    if (!ok) {
        g_nodes = bakN; g_links = bakL; g_groups = bakG;
        s_err = (ver == L"#v3" || ver == L"#v2") ? L"parse error" : L"unknown format";
        return false;
    }
    AfterLoad(); s_err.clear();
    return true;
}

// ---------- диалоги ----------
static bool PickFile(bool open, wchar_t* buf, DWORD cch) {
    ZeroMemory(buf, cch * sizeof(wchar_t));
    OPENFILENAMEW ofn; ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFilter = L"Graph files (*.txt)\0*.txt\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = cch;
    ofn.lpstrDefExt = L"txt";
    ofn.lpstrTitle = open ? L"Load graph" : L"Save graph";
    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR |
        (open ? OFN_FILEMUSTEXIST : OFN_OVERWRITEPROMPT);
    return open ? (GetOpenFileNameW(&ofn) == TRUE) : (GetSaveFileNameW(&ofn) == TRUE);
}
void SaveAs() {
    wchar_t buf[MAX_PATH];
    if (!PickFile(false, buf, MAX_PATH)) return;
    Save(buf);
}
void LoadFrom() {
    wchar_t buf[MAX_PATH];
    if (!PickFile(true, buf, MAX_PATH)) return;
    if (!Load(buf)) {
        std::wstring msg = L"Load failed: " + s_err;
        MessageBoxW(g_hwnd, msg.c_str(), L"Load", MB_OK | MB_ICONWARNING);
    }
    InvalidateRect(g_hwnd, nullptr, FALSE);
}