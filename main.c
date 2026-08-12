#include ".\resource.h"
#include <windows.h>
#include <tchar.h>
#include <wingdi.h>
#define BUFSIZE 65535
typedef struct WindowParam {
    BOOL bTmInit;
    UINT cxClient;
    UINT cyClient;
    UINT nCh;
    UINT dwLines;
    UINT nCurChar;
    BOOL isFirstPos;
    HFONT hFont;
    PTCHAR pChInputBuf;
    POINT cursorPos;
    LPUINT pLineHeads;
    INT uiScrollLines;
}WindowParam, * LPWindowParam;
static HINSTANCE g_hInstance;
void static calCaretPos(HWND hwnd,LPWindowParam p,LPCTSTR pChInputBuf,UINT nCurChar) {
    HDC hdc = GetDC(hwnd);
    SelectObject(hdc,p->hFont);
    TEXTMETRIC tm;
    GetTextMetrics(hdc, &tm);
    UINT mydwlines = 0;
    UINT myWidth = 0;
    UINT cur_width = 0;
    UINT f_pos = 0;
    UINT uncounted = 0;
    if (p->isFirstPos) {
        p->cursorPos.x = 0;
        p->cursorPos.y = 0;
        ReleaseDC(hwnd, hdc);
        return;
    }
    for (UINT i = 0; i < p->dwLines; i++) {
        if (p->pLineHeads[i] <= nCurChar + pChInputBuf - p->pChInputBuf) {
            if (p->pLineHeads[i] >= pChInputBuf - p->pChInputBuf)
                mydwlines++;
            else
                uncounted++;
        }
        else {
            break;
        }
    }
    f_pos = (uncounted + mydwlines) == 0 ? 0 : (p->pLineHeads[uncounted + mydwlines - 1] + 1);
    for (UINT j = f_pos - (pChInputBuf - p->pChInputBuf); j <= nCurChar; j++) {
        GetCharWidth32(hdc, pChInputBuf[j], pChInputBuf[j], &cur_width);
        myWidth += cur_width;
    }
    p->cursorPos.x = myWidth;
    p->cursorPos.y = mydwlines * tm.tmHeight;
    ReleaseDC(hwnd,hdc);
}
LPCTSTR static skipLines(UINT toSkip, LPCTSTR inputBuffer, size_t bufLen, LPWindowParam p) {
    UINT curPos = inputBuffer - p->pChInputBuf;
    UINT total_skips = 0;
    for (UINT i = 0; i < p->dwLines; i++) {
        if (p->pLineHeads[i] <= curPos) {
            total_skips++;
        }
    }
    total_skips += toSkip;
    if (total_skips == 0) {
        return inputBuffer;
    }
    if (total_skips > p->dwLines) {
        return inputBuffer + bufLen;
    }
    return p->pChInputBuf + 1 + p->pLineHeads[total_skips - 1];
}
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,LPARAM lParam);
int WINAPI _tWinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int CmdShow){
  WNDCLASSEX wndClass;
  g_hInstance = hInstance;
  TCHAR szClassName[] = TEXT("MyWindow");
  TCHAR szAppName[] = TEXT("Edit");
  HWND hwnd;
  MSG msg;
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
  wndClass.cbSize = sizeof(WNDCLASSEX);
  wndClass.style = CS_HREDRAW | CS_VREDRAW;
  wndClass.lpfnWndProc = WindowProc;
  wndClass.cbClsExtra = 0;
  wndClass.cbWndExtra = 0;
  wndClass.hInstance = hInstance;
  wndClass.hIcon = LoadIcon(hInstance,MAKEINTRESOURCE(IDI_ICON2));
  wndClass.hCursor = LoadCursor(NULL,IDC_ARROW);
  wndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
  wndClass.lpszMenuName = NULL;
  wndClass.lpszClassName = szClassName;
  wndClass.hIconSm = LoadIcon(hInstance,MAKEINTRESOURCE(IDI_ICON2));
  RegisterClassEx(&wndClass);
  hwnd = CreateWindowEx(0,szClassName,szAppName,WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_HSCROLL,CW_USEDEFAULT,CW_USEDEFAULT,400,300,NULL,NULL,hInstance,NULL);
  ShowWindow(hwnd,CmdShow);
  UpdateWindow(hwnd);
  HACCEL hAccel = LoadAccelerators(hInstance,MAKEINTRESOURCE(IDR_ACCELERATOR1));
  if (hAccel == NULL) {
      DWORD dwErr = GetLastError();
      TCHAR szBuf[256];
      wsprintf(szBuf, TEXT("加载加速键失败，错误码: %d"), dwErr);
      MessageBox(hwnd, szBuf, TEXT("错误"), MB_OK);
      return 0;
  }
  while(GetMessage(&msg,NULL,0,0) != 0){
      if (!TranslateAccelerator(hwnd, hAccel, &msg)) {
          TranslateMessage(&msg);
          DispatchMessage(&msg);
      }
  }
  return msg.wParam;
}



BOOL static InitWindow(HWND hwnd,LPWindowParam params) {
    HDC hdc = GetDC(hwnd);
    params->hFont = CreateFont(25, 0, 0, 0, 0, 0, 0, 0, GB2312_CHARSET, 0, 0, 0, 0, TEXT("宋体"));
    params->bTmInit = TRUE;
    params->dwLines = 0;
    ReleaseDC(hwnd, hdc);
    params->pChInputBuf = (LPTSTR)malloc(sizeof(TCHAR)*BUFSIZE);
    params->pLineHeads = (LPUINT)malloc(sizeof(UINT) * BUFSIZE);
    if (!params->pChInputBuf || !params->pLineHeads) {
        return FALSE;
    }
    SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &params->uiScrollLines, 0);
    params->uiScrollLines = WHEEL_DELTA / params->uiScrollLines;
    ZeroMemory(params->pChInputBuf,BUFSIZE);
    ZeroMemory(params->pLineHeads, BUFSIZE);
    params->nCh = 0;
    params->nCurChar = 0;
    params->isFirstPos = TRUE;
    HMENU hMenu = LoadMenu(g_hInstance, MAKEINTRESOURCE(IDR_MENU1));
    SetMenu(hwnd,hMenu);
    return TRUE;
}
void static ResizeWindow(HWND hwnd,LPWindowParam params,LPARAM lParam) {
    SCROLLINFO si = { 0 };
    TEXTMETRIC tm;
    HDC hdc = GetDC(hwnd);
    SelectObject(hdc, params->hFont);
    GetTextMetrics(hdc, &tm);
    params->cxClient = LOWORD(lParam);
    params->cyClient = HIWORD(lParam);
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_PAGE;
    si.nPage = params->cyClient / tm.tmHeight;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
    ReleaseDC(hwnd, hdc);
}
void static FocusWindow(HWND hwnd, LPWindowParam params) {
    HDC hdc = GetDC(hwnd);
    SelectObject(hdc, params->hFont);
    TEXTMETRIC tm;
    GetTextMetricsW(hdc, &tm);
    CreateCaret(hwnd, (HBITMAP)1, tm.tmMaxCharWidth, tm.tmHeight);
    SetCaretPos(0, 0);
    ShowCaret(hwnd);
}
void static CharInputWindow(HWND hwnd,LPWindowParam params,WPARAM wParam) {
    switch (wParam) {
    case '\b': {
        if (params->nCh == 0)return;
        UINT target = 0;
        for (UINT i = 0; i <= params->dwLines; i++) {
            target = i;
            if (i < params->dwLines && params->pLineHeads[i] > params->nCurChar) {
                break;
            }
        }
        for (; target < params->dwLines; target++) {
            params->pLineHeads[target]--;
        }
        if (params->pChInputBuf[params->nCurChar] == '\r') {
            UINT target = 0;
            for (UINT i = 0; i < params->dwLines; i++) {
                if (params->pLineHeads[i] == params->nCurChar) {
                    target = i;
                    break;
                }
            }
            memmove(params->pLineHeads + target, params->pLineHeads + target + 1, params->dwLines - target);
            params->dwLines--;
        }

        if (params->nCurChar > 0)
        {
            params->nCurChar--;
        }
        else
        {
            params->isFirstPos = TRUE;
        }
        params->nCh--;

        if (params->isFirstPos) {
            memmove(params->pChInputBuf, params->pChInputBuf + 1, (params->nCh) * sizeof(TCHAR));
        }
        else {
            memmove(params->pChInputBuf + params->nCurChar + 1, params->pChInputBuf + params->nCurChar + 2, (params->nCh - params->nCurChar - 1) * sizeof(TCHAR));
        }
        params->pChInputBuf[params->nCh] = 0;
        InvalidateRect(hwnd, NULL, TRUE);
        UpdateWindow(hwnd);
    }
    case 0x1B:
        return;
    case '\t':
        for (int i = 0; i < 4; i++) {
            SendMessage(hwnd, WM_CHAR, ' ', 0);
        }
        return;
    default: {
        UINT target = params->dwLines;
        for (UINT i = 0; i <= params->dwLines; i++) {
            target = i;
            if (i < params->dwLines && params->pLineHeads[i] > params->nCurChar) {
                break;
            }
        }
        for (UINT i = target; i < params->dwLines; i++) {
                params->pLineHeads[i]++;
        }
        if ((TCHAR)'\r' == (TCHAR)wParam) {
            memmove(params->pLineHeads + target + 1, params->pLineHeads + target, (params->dwLines - target)*sizeof(UINT));
            params->pLineHeads[target] = params->nCurChar + 1;
            params->dwLines++;
        }
           HideCaret(hwnd);
           SCROLLINFO si = { 0 };
           if (params->isFirstPos) {
               int nCurChar = -1;
               memmove(params->pChInputBuf + nCurChar + 2, params->pChInputBuf + nCurChar + 1, (params->nCh - nCurChar - 1) * sizeof(TCHAR));
               params->pChInputBuf[params->nCurChar] = (TCHAR)wParam;
               params->isFirstPos = FALSE;
           }
           else {
               memmove(params->pChInputBuf + params->nCurChar + 2, params->pChInputBuf + params->nCurChar + 1, (params->nCh - params->nCurChar - 1) * sizeof(TCHAR));
               params->pChInputBuf[++params->nCurChar] = (TCHAR)wParam;
           }
           params->nCh++;
           si.cbSize = sizeof(SCROLLINFO);
           si.fMask = SIF_RANGE | SIF_PAGE;
           si.nMin = 0;
           si.nMax = params->dwLines + 10;
           HDC hdc = GetDC(hwnd);
           SelectObject(hdc, params->hFont);
           TEXTMETRIC tm;
           GetTextMetrics(hdc, &tm);
           si.nPage = params->cyClient / tm.tmHeight;
           ReleaseDC(hwnd, hdc);
           SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
           si.cbSize = sizeof(SCROLLINFO);
           si.fMask = SIF_ALL;
           GetScrollInfo(hwnd, SB_VERT, &si);
           InvalidateRect(hwnd, NULL, TRUE);
           UpdateWindow(hwnd);
           ShowCaret(hwnd);
           return;
    }
    }
}
void static ScrollBarWindow(HWND hwnd,LPWindowParam p,WPARAM wParam,int direction) {
    SCROLLINFO si = { 0 };
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_ALL;
    GetScrollInfo(hwnd, direction, &si);
    INT iPos = si.nPos;
    if (direction == SB_VERT) {
        switch (LOWORD(wParam)) {
        case SB_LINEUP:
            si.nPos -= 1;
            break;
        case SB_LINEDOWN:
            si.nPos += 1;
            break;
        case SB_PAGEUP:
            si.nPos -= si.nPage;
            break;
        case SB_PAGEDOWN:
            si.nPos += si.nPage;
            break;
        case SB_THUMBTRACK:
            si.nPos = si.nTrackPos;
            break;
        default:
            return 0;
        }
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_POS;
        SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
        GetScrollInfo(hwnd, SB_VERT, &si);

        if (iPos != si.nPos) {
            HDC hdc = GetDC(hwnd);
            SelectObject(hdc, p->hFont);
            TEXTMETRIC tm;
            GetTextMetrics(hdc, &tm);
            ScrollWindow(hwnd, 0, tm.tmHeight * (iPos - si.nPos), NULL, NULL);
            ReleaseDC(hwnd,hdc);
            UpdateWindow(hwnd);
        }
    }
    else {
        switch (LOWORD(wParam))
        {
        case SB_LINELEFT:
            si.nPos -= 1;
            break;
        case SB_LINERIGHT:
            si.nPos += 1;
            break;
        case SB_PAGELEFT:
            si.nPos -= si.nPage;
            break;
        case SB_PAGERIGHT:
            si.nPos += si.nPage;
            break;
        case SB_THUMBTRACK:
            si.nPos = si.nTrackPos;
            break;
        }
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_POS;
        SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
        GetScrollInfo(hwnd, SB_HORZ, &si);
        if (iPos != si.nPos) {
            HDC hdc = GetDC(hwnd);
            SelectObject(hdc, p->hFont);
            TEXTMETRIC tm;
            GetTextMetrics(hdc, &tm);
            /*ScrollWindow(hwnd, tm.tmAveCharWidth*(iPos - si.nPos), 0, NULL, NULL);*/
            InvalidateRect(hwnd, NULL, TRUE);
            UpdateWindow(hwnd);
        }
    }
}
void static KeyInputWindow(HWND hwnd,LPWindowParam p,WPARAM wParam) {
    HDC hdc = GetDC(hwnd);
    switch (wParam) {
    case VK_LEFT:
        if (p->nCurChar > 0)
            p->nCurChar--;
        else if (p->nCurChar == 0) {
            p->isFirstPos = TRUE;
        }
        break;
    case VK_RIGHT:
        if (p->isFirstPos == TRUE) {
            p->isFirstPos = FALSE;
        }
        else {
            p->nCurChar++;
        }
        p->nCurChar = min(p->nCh - 1, p->nCurChar);
        break;
    case VK_UP:
        SendMessage(hwnd, WM_VSCROLL, SB_LINEUP, 0);
        break;
    case VK_DOWN:
        SendMessage(hwnd, WM_VSCROLL, SB_LINEDOWN, 0);
        break;
    default:
        break;
    }
    ReleaseDC(hwnd, hdc);
    SendMessage(hwnd, WM_PAINT, 0, 0);
}
void static PaintWindow(HWND hwnd, LPWindowParam p, WPARAM wParam) {
    PAINTSTRUCT ps;
    SCROLLINFO si;
    INT iVertPos;
    INT iHorzPos;
    RECT rect;
    HDC hdc = BeginPaint(hwnd, &ps);
    TEXTMETRIC tm;
    GetTextMetrics(hdc, &tm);
    SelectObject(hdc, p->hFont);
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_POS | SIF_PAGE;
    GetScrollInfo(hwnd, SB_VERT, &si);
    iVertPos = si.nPos;
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_POS;
    GetScrollInfo(hwnd, SB_HORZ, &si);
    iHorzPos = si.nPos;
    LPCTSTR half_dest = skipLines(iVertPos, p->pChInputBuf, p->nCh,p);
    LPCTSTR dest = skipLines((ps.rcPaint.top + tm.tmHeight - 1) / tm.tmHeight, half_dest, p->nCh - (half_dest - p->pChInputBuf),p);
    HideCaret(hwnd);
    SetRect(&rect, -iHorzPos * tm.tmAveCharWidth, ps.rcPaint.top, p->cxClient, p->cyClient);
    if (dest - p->pChInputBuf <= p->nCh)
        DrawText(hdc, dest, -1, &rect, DT_LEFT);
    if (p->nCurChar >= half_dest - p->pChInputBuf) {
        calCaretPos(hwnd, p, half_dest, p->nCurChar - (half_dest - p->pChInputBuf));
    }
    else {
        p->cursorPos.x = p->cursorPos.y = 0;
    }
    SetCaretPos(p->cursorPos.x - tm.tmAveCharWidth*iHorzPos, p->cursorPos.y);
    ShowCaret(hwnd);
    EndPaint(hwnd, &ps);
}
void static MouseEventWindow(HWND hwnd,LPWindowParam p,WPARAM wParam) {
    if (p->uiScrollLines == 0)return;
    int dist = GET_WHEEL_DELTA_WPARAM(wParam);
    dist = dist / p->uiScrollLines;
    while (dist > 0) {
        dist--;
        SendMessage(hwnd, WM_VSCROLL, SB_LINEUP, 0);
    }
    while (dist < 0) {
        dist++;
        SendMessage(hwnd, WM_VSCROLL, SB_LINEDOWN, 0);
    }
}
void static MenuCommandWindow(HWND hwnd, LPWindowParam p, WPARAM wParam) {
    WCHAR szBuffer[50] = { 0 };
    LPWSTR choice = NULL;
    switch (LOWORD(wParam)) {
    case ID_FILE_NEW: {
        choice = L"新建";
        break;
    }
    case ID_FILE_OPEN: {
        choice = L"打开";
        break;
    }
    case ID_FILE_SAVE: {
        choice = L"保存";
        break;
    }
    case ID_FILE_SAVEAS: {
        choice = L"另存";
        break;
    }
    case ID_FILE_EXIT: {
        choice = L"退出";
        break;
    }
    case ID_HELP_ABOUT: {
        choice = L"关于";
        break;
    }
    case ID_EDIT_CUT: {
        choice = L"剪切";
        break;
    }
    case ID_EDIT_COPY: {
        choice = L"复制";
        break;
    }
    case ID_EDIT_PASTE: {
        choice = TEXT("粘贴");
        break;
    }
    default:
        break;
    }
    wsprintfW(szBuffer, TEXT("你点击了 %s"), choice);
    MessageBoxW(hwnd,szBuffer,TEXT("提示"),MB_OK);
}
LRESULT CALLBACK WindowProc(HWND hwnd,UINT umsg,WPARAM wParam,LPARAM lParam){
  static WindowParam params;
  switch (umsg) {
  case WM_CREATE:
  {
      BOOL bInitSucceed = InitWindow(hwnd, &params);
      if (!bInitSucceed) {
          PostMessage(hwnd, WM_DESTROY, 0, 0);
      }
      return 0;
  }
  case WM_SIZE:
  {
      ResizeWindow(hwnd, &params, lParam);
      return 0;
  }
  case WM_SETFOCUS:
  {
      FocusWindow(hwnd, &params);
      return 0;
  }
  case WM_KILLFOCUS:
  {
      HideCaret(hwnd);
      DestroyCaret();
      return 0;
  }
  case WM_CHAR: {
      CharInputWindow(hwnd, &params, wParam);
      return 0;
  }
  case WM_KEYDOWN:
  {
      KeyInputWindow(hwnd, &params, wParam);
      return 0;
  }
  case WM_PAINT: {
      PaintWindow(hwnd, &params, wParam);
      return 0;
  }
  case WM_DESTROY:
  {
      free(params.pChInputBuf);
      free(params.pLineHeads);
      DeleteObject(params.hFont);
      PostQuitMessage(0);
      return 0;
  }
  case WM_VSCROLL:
      ScrollBarWindow(hwnd, &params, wParam, SB_VERT);
      return 0;
  case WM_HSCROLL:
      ScrollBarWindow(hwnd, &params, wParam, SB_HORZ);
      return 0;
  case WM_MOUSEWHEEL:
      MouseEventWindow(hwnd, &params, wParam);
      return 0;
  case WM_COMMAND: {
      MenuCommandWindow(hwnd, &params, wParam);
      return 0;
  }
  }
  return DefWindowProc(hwnd,umsg,wParam,lParam);
}
