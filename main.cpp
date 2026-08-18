#include "editor.h"

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nCmd) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"NodeEditorClass";
    wc.style = CS_DBLCLKS; 
    RegisterClassExW(&wc);

    g_hwnd = CreateWindowExW(0, L"NodeEditorClass", L"Node Editor",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 1400, 900,
        nullptr, nullptr, hInst, nullptr);

    // стартовая сцена
    int id1 = AddNode(L"Start", 100, 200, 46, 160, 80);
    int id2 = AddNode(L"Process", 400, 150, 140, 80, 180);
    int id3 = AddNode(L"Code", 400, 380, 50, 130, 200);
    int id4 = AddNode(L"Output", 720, 260, 200, 180, 60);
    int id5 = AddNode(L"Description", 100, 470, 120, 120, 60, true);
    Node* n5 = FindNode(id5);
    if (n5) { n5->desc = L"Double-click me\nto edit description"; n5->Recalc(); }

    Node* n1 = FindNode(id1), * n2 = FindNode(id2), * n3 = FindNode(id3), * n4 = FindNode(id4);
    if (n1 && n2 && n3 && n4) {
        g_links.emplace_back(g_lid++, n1->outputs[0].id, n2->inputs[0].id);
        g_links.emplace_back(g_lid++, n2->outputs[0].id, n4->inputs[0].id);
        g_links.emplace_back(g_lid++, n1->outputs[0].id, n3->inputs[0].id);
        g_links.emplace_back(g_lid++, n3->outputs[0].id, n4->inputs[0].id);
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return (int)msg.wParam;
}