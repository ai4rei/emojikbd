
#include <cstdlib>
#include <cstring>
#include <cwchar>

#include <windows.h>
#include <tchar.h>
#include <commctrl.h>
#include <windowsx.h>
#include <usp10.h>

#include <btypes.h>
#include <bvcstr.h>
#include <bvfont.h>
#include <memdbg.h>
#include <memtaf.h>
#include <rc_const.h>
#include <w32co.h>
#include <w32coinit.hpp>
#include <w32ex.h>
#include <w32gdi.h>
#include <w32ui.h>
#include <w32wnd.hpp>

#include "emojidat.h"
#include "emojikbd.h"

#define EMOJI_GRID_ITEMS_X 12
#define EMOJI_GRID_ITEMS_Y 10

class CEmojiKeyboard
    : public CWindowUI
    , public CCoInitialize
{
private:
    typedef CEmojiKeyboard CSelf;
    typedef CWindowUI CSuper;

protected:
    HINSTANCE const m_hInstance;
    HWND m_hWndTT;
    HANDLE m_hSideLoadFont;
    HFONT m_hUIFont;
    HFONT m_hEmojiFont;

    IEmojiData* m_pData;
    UINT* m_auDisplaySet;
    UINT m_uDisplayCount;
    TCHAR m_szSearchTerm[256];
    TCHAR m_szLastToolTip[128];

protected:
    static inline INT GetStringLength(LPCTSTR lpszString)
    {
        SIZE_T const uLength = _tcslen(lpszString);

        AssertHere(uLength==static_cast< INT >(uLength));

        return static_cast< INT >(uLength);
    }

    INT GetDeviceCapsForWindow(HWND const hWnd, INT const nIndex) const
    {
        HDC hdcWnd = GetDC(hWnd);

        if(hdcWnd!=NULL)
        {
            INT const nResult = GetDeviceCaps(hdcWnd, nIndex);

            ReleaseDC(hWnd, hdcWnd);

            return nResult;
        }

        return 0;
    }

    INT GetSystemMetricsForDpiDL(INT const nIndex, INT const nDPI) const
    {
        if(nDPI)
        {
            return GetSystemMetrics(nIndex);
        }

        return (GetSystemMetrics(nIndex)*nDPI)/96;
    }

    INT GetEmojiWidth(HWND const hWnd) const
    {
        return GetSystemMetricsForDpiDL(SM_CXICON, GetDeviceCapsForWindow(hWnd, LOGPIXELSX));
    }

    INT GetEmojiHeight(HWND const hWnd) const
    {
        return GetSystemMetricsForDpiDL(SM_CYICON, GetDeviceCapsForWindow(hWnd, LOGPIXELSY));
    }

    INT GetHScrollHeight(HWND const hWnd) const
    {
        return GetSystemMetricsForDpiDL(SM_CYHSCROLL, GetDeviceCapsForWindow(hWnd, LOGPIXELSY));
    }

    INT GetEmojiAreaWidth(HWND const hWnd) const
    {
        return GetEmojiWidth(hWnd)*EMOJI_GRID_ITEMS_X;
    }

    INT GetEmojiAreaHeight(HWND const hWnd) const
    {
        return GetEmojiHeight(hWnd)*EMOJI_GRID_ITEMS_Y+GetHScrollHeight(hWnd);
    }

    HRESULT GetEmoji(UINT const uItem, IEmoji** ppEmojiOut) const
    {
        if(uItem>=GetEmojiCount())
        {
            return E_BOUNDS;
        }

        UINT const uID = m_auDisplaySet[uItem];

        return m_pData->GetEmojiByOrder(uID, ppEmojiOut);
    }

    UINT GetEmojiCount() const
    {
        return m_uDisplayCount;
    }

    HWND GetEmojiList(HWND const hWnd) const
    {
        return GetDlgItem(hWnd, IDC_EMOJILIST);
    }

    UINT GetEmojiListItem(HWND const hWndCtl, INT const nIndex)
    {
        return static_cast< UINT >(ListBox_GetItemData(hWndCtl, nIndex));
    }

    VOID RefreshEmojiDisplaySet()
    {
        UINT uDisplayCount = 0;
        UINT* auDisplaySet = NULL;

        if(MemTAllocEx(&auDisplaySet, m_pData->GetCount()))
        {
            HRESULT hr;
            IEnumEmoji* pEnumEmoji = NULL;

            if(m_szSearchTerm[0]!=_T('\0'))
            {
                hr = m_pData->EnumEmojiByKeyword(m_szSearchTerm, &pEnumEmoji);
            }
            else
            {
                hr = m_pData->EnumEmoji(&pEnumEmoji);
            }

            if(!FAILED(hr))
            {
                HDC hdcMem = CreateCompatibleDC(NULL);

                SaveDC(hdcMem);
                {
                    SCRIPT_CACHE Sc = NULL;

                    SelectObject(hdcMem, m_hEmojiFont);

                    for(;;)
                    {
                        IEmoji* pEmoji = NULL;

                        hr = pEnumEmoji->Next(1UL, &pEmoji, NULL);
                        if(hr==S_OK)
                        {
                            LPCWSTR const lpwszCodePoints = pEmoji->GetCodePoints();
                            WORD awIndices[16], awLogClust[16];
                            SCRIPT_VISATTR Sv[__ARRAYSIZE(awIndices)];
                            SCRIPT_ANALYSIS Sa = { 0 };
                            int nGlyphs = 0;

                            hr = ScriptShape(hdcMem, &Sc, lpwszCodePoints, GetStringLength(lpwszCodePoints), __ARRAYSIZE(awIndices), &Sa, awIndices, awLogClust, Sv, &nGlyphs);

                            if(FAILED(hr) || nGlyphs!=1 || awIndices[0]==0)
                            {// does not collapse into single character on this system or font does not support glyph
                                continue;
                            }

                            auDisplaySet[uDisplayCount] = pEmoji->GetOrder();
                            uDisplayCount++;

                            CoSafeRelease(&pEmoji);
                        }
                        else
                        {
                            break;
                        }
                    }

                    ScriptFreeCache(&Sc);
                }
                RestoreDC(hdcMem, -1);

                CoSafeRelease(&pEnumEmoji);
            }

            MemTFree(&m_auDisplaySet);
            m_auDisplaySet = auDisplaySet;
            m_uDisplayCount = uDisplayCount;
        }
    }

    VOID RefreshEmojiList(HWND const hWnd)
    {
        HWND const hWndList = GetEmojiList(hWnd);

        SetWindowRedraw(hWndList, FALSE);
        {
            ListBox_ResetContent(hWndList);
            ListBox_SetColumnWidth(hWndList, GetEmojiWidth(hWnd));

            RefreshEmojiDisplaySet();

            UINT const uEmojiCount = GetEmojiCount();

            ListBox_InitStorage(hWndList, uEmojiCount, 0);

            for(UINT uItem = 0; uItem<uEmojiCount; uItem++)
            {
                ListBox_AddItemData(hWndList, uItem);
            }
        }
        SetWindowRedraw(hWndList, TRUE);
        RedrawWindow(hWndList, NULL, NULL, RDW_ERASE|RDW_FRAME|RDW_INVALIDATE|RDW_ALLCHILDREN);
        ListBox_SetCurSel(hWndList, 0);
    }

    VOID SetSearchTerm(HWND const hWnd)
    {
        GetDlgItemText(hWnd, IDC_EDIT_FIND, m_szSearchTerm, __ARRAYSIZE(m_szSearchTerm));
    }

    static VOID CALLBACK OnTimerRestoreCopyButton(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
    {
        KillTimer(hWnd, idEvent);
        SetWindowText(GetDlgItem(hWnd, IDC_BTN_COPY), _T("Copy"));
    }

    VOID CopyCurrentEmoji(HWND const hWnd)
    {
        bool bSuccess = false;
        HWND const hWndList = GetEmojiList(hWnd);
        UINT const uItem = ListBox_GetCurSel(hWndList);

        if(uItem!=LB_ERR)
        {
            HRESULT hr;
            IEmoji* pEmoji = NULL;

            hr = GetEmoji(uItem, &pEmoji);
            if(!FAILED(hr))
            {
                if(OpenClipboard(hWnd))
                {
                    if(EmptyClipboard())
                    {
                        bSuccess = SetClipboardToText(pEmoji->GetCodePoints())!=FALSE;
                    }

                    CloseClipboard();
                }

                CoSafeRelease(&pEmoji);
            }
        }

        if(bSuccess)
        {
            SetWindowText(GetDlgItem(hWnd, IDC_BTN_COPY), _T("\x221A Copied"));
            SetTimer(hWnd, reinterpret_cast< UINT_PTR >(this), 500, &OnTimerRestoreCopyButton);
        }
        else
        {
            MessageBeep(0);
        }
    }

    ATOM RegisterClassEx(WNDCLASSEX CONST* lpWcx) override
    {
        WNDCLASSEX Wcx = lpWcx[0];

        Wcx.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);

        return CSuper::RegisterClassEx(&Wcx);
    }

    VOID WndProcOnCommand(HWND hWnd, INT nId, HWND hWndCtl, UINT uCodeNotify) override
    {
        switch(nId)
        {
        case IDOK:
            SetSearchTerm(hWnd);
            RefreshEmojiList(hWnd);
            break;
        case IDC_BTN_COPY:
            CopyCurrentEmoji(hWnd);
            break;
        case IDC_EMOJILIST:
            switch(uCodeNotify)
            {
            case LBN_DBLCLK:
                CopyCurrentEmoji(hWnd);
                break;
            default:
                CSuper::WndProcOnCommand(hWnd, nId, hWndCtl, uCodeNotify);
                break;
            }
            break;
        default:
            CSuper::WndProcOnCommand(hWnd, nId, hWndCtl, uCodeNotify);
            break;
        }
    }

    BOOL WndProcOnCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct) override
    {
        RECT rcWnd;

        m_hUIFont = BvFont_GetGuiFont();

        INT const nBtnWidth = MapFontXValue(m_hUIFont, DLU_SZ_BUTTON_W);
        INT const nBtnHeight = MapFontYValue(m_hUIFont, DLU_SZ_BUTTON_H);
        INT const nMarginH = MapFontXValue(m_hUIFont, DLU_SP_DIALOGBOX_MARGIN_H);
        INT const nMarginV = MapFontYValue(m_hUIFont, DLU_SP_DIALOGBOX_MARGIN_V);
        INT const nEditHeight = MapFontYValue(m_hUIFont, DLU_SZ_EDIT_H);
        INT const nRelatedSpacingH = MapFontYValue(m_hUIFont, DLU_SP_RELATED_H);
        INT const nRelatedSpacingV = MapFontYValue(m_hUIFont, DLU_SP_RELATED_V);

        if(!CreateWindowEx(WS_EX_EDIT, WC_EDIT, _T(""), WS_VISIBLE|WS_CHILD|WS_GROUP|WS_TABSTOP|WS_EDIT, nMarginH, nMarginV, GetEmojiAreaWidth(hWnd)-nRelatedSpacingH-nBtnWidth-nRelatedSpacingH-nBtnWidth, nEditHeight, hWnd, HMENU_OR_ID(IDC_EDIT_FIND), NULL, NULL))
        {
            return FALSE;
        }

        if(!CreateWindowEx(0, WC_BUTTON, _T("Find"), WS_VISIBLE|WS_CHILD|WS_TABSTOP|BS_DEFPUSHBUTTON, nMarginH+GetEmojiAreaWidth(hWnd)-nBtnWidth-nRelatedSpacingH-nBtnWidth, nMarginV, nBtnWidth, nBtnHeight, hWnd, HMENU_OR_ID(IDOK), NULL, NULL))
        {
            return FALSE;
        }

        if(!CreateWindowEx(0, WC_BUTTON, _T("Copy"), WS_VISIBLE|WS_CHILD|WS_TABSTOP|BS_PUSHBUTTON, nMarginH+GetEmojiAreaWidth(hWnd)-nBtnWidth, nMarginV, nBtnWidth, nBtnHeight, hWnd, HMENU_OR_ID(IDC_BTN_COPY), NULL, NULL))
        {
            return FALSE;
        }

        if(!CreateWindowEx(WS_EX_NOPARENTNOTIFY, WC_LISTBOX, _T(""), WS_VISIBLE|WS_CHILD|WS_TABSTOP|WS_HSCROLL|LBS_DISABLENOSCROLL|LBS_MULTICOLUMN|LBS_NOINTEGRALHEIGHT|LBS_OWNERDRAWFIXED|LBS_NOTIFY, nMarginH, nMarginV+nEditHeight+nRelatedSpacingV, GetEmojiAreaWidth(hWnd), GetEmojiAreaHeight(hWnd), hWnd, HMENU_OR_ID(IDC_EMOJILIST), NULL, NULL))
        {
            return FALSE;
        }

        m_hWndTT = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL, TTS_NOPREFIX|TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hWnd, NULL, NULL, NULL);
        if(m_hWndTT==NULL)
        {
            return FALSE;
        }

        TCHAR szFontFile[MAX_PATH];

        if(GetModuleFileNameSpecificPath(NULL, szFontFile, __ARRAYSIZE(szFontFile), _T("NotoEmoji-Regular"), _T("ttf")))
        {
            m_hSideLoadFont = W32GdiAddFontFromFile(szFontFile);
        }

        LPCTSTR const static s_alpszEmojiFonts[] =
        {
            _T("Noto Emoji"),
            _T("Segoe UI Emoji"),
            _T("Segoe UI Symbol"),
        };
        for(UINT uIdx = 0; uIdx<__ARRAYSIZE(s_alpszEmojiFonts); uIdx++)
        {
            m_hEmojiFont = BvFont_CreateFontDefaultPrecise(-(GetEmojiHeight(hWnd)*3)/4, DEFAULT_CHARSET, s_alpszEmojiFonts[uIdx]);
            if(m_hEmojiFont!=NULL)
            {
                break;
            }
        }
        if(m_hEmojiFont==NULL)
        {
            MessageBox(NULL, _T("No Emoji font found. Exiting..."), _T("Fatal Error"), MB_ICONSTOP|MB_OK);
            return FALSE;
        }

        if(FAILED(GetEmojiDatabaseData(&m_pData)))
        {
            return FALSE;
        }

        SetWindowText(hWnd, _T("Emoji Keyboard"));
        SetWindowLargeIcon(hWnd, LoadLargeIcon(m_hInstance, MAKEINTRESOURCE(IDI_MAINICON)));
        SetWindowSmallIcon(hWnd, LoadSmallIcon(m_hInstance, MAKEINTRESOURCE(IDI_MAINICON)));
        BvFont_SetFont(hWnd, m_hUIFont);

        RefreshEmojiList(hWnd);
        SetFocus(GetEmojiList(hWnd));

        SetRect(&rcWnd, 0, 0, nMarginH*2+GetEmojiAreaWidth(hWnd), nMarginV*2+nEditHeight+nRelatedSpacingV+GetEmojiAreaHeight(hWnd));
        AdjustWindowRectEx(&rcWnd, GetWindowStyle(hWnd), GetMenu(hWnd)!=NULL, GetWindowExStyle(hWnd));
        CenterWindowInWorkAreaWithRect(hWnd, &rcWnd);

        TOOLINFO Ti = { sizeof(Ti) };

        Ti.uFlags = TTF_SUBCLASS|TTF_IDISHWND;
        Ti.hwnd = hWnd;
        Ti.uId = reinterpret_cast< UINT_PTR >(GetEmojiList(hWnd));
        Ti.lpszText = LPSTR_TEXTCALLBACK;
        Tooltip_AddTool(m_hWndTT, &Ti);
        Tooltip_SetMaxTipWidth(m_hWndTT, rcWnd.right-rcWnd.left);
        Tooltip_Activate(m_hWndTT, TRUE);

        return TRUE;
    }

    VOID WndProcOnDestroy(HWND hWnd) override
    {
        CSuper::WndProcOnDestroy(hWnd);
        m_hWndTT = NULL;
    }

    VOID WndProcOnDrawItem(HWND hWnd, DRAWITEMSTRUCT const* const lpDrawItem) override
    {
        if(lpDrawItem->CtlType!=ODT_LISTBOX || lpDrawItem->CtlID!=IDC_EMOJILIST)
        {
            CSuper::WndProcOnDrawItem(hWnd, lpDrawItem);
            return;
        }

        if(lpDrawItem->itemID==-1)
        {
            // Normally the focus rectangle would have to be drawn, but this
            // listbox is not supposed to be empty.
            return;
        }

        UINT const uItem = GetEmojiListItem(lpDrawItem->hwndItem, lpDrawItem->itemID);

        IEmoji* pEmoji = NULL;
        if(FAILED(GetEmoji(uItem, &pEmoji)))
        {
            return;
        }

        if(!!(lpDrawItem->itemAction&(ODA_DRAWENTIRE|ODA_SELECT)))
        {
            if(!!(lpDrawItem->itemState&ODS_SELECTED))
            {
                FillRect(lpDrawItem->hDC, &lpDrawItem->rcItem, GetSysColorBrush(COLOR_HIGHLIGHT));
            }
            else
            {
                FillRect(lpDrawItem->hDC, &lpDrawItem->rcItem, GetSysColorBrush(COLOR_WINDOW));
            }

            SaveDC(lpDrawItem->hDC);
            {
                SelectObject(lpDrawItem->hDC, m_hEmojiFont);

                if(!!(lpDrawItem->itemState&ODS_SELECTED))
                {
                    SetTextColor(lpDrawItem->hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
                }
                else
                {
                    SetTextColor(lpDrawItem->hDC, GetSysColor(COLOR_WINDOWTEXT));
                }

                SetBkMode(lpDrawItem->hDC, TRANSPARENT);
                TextOut(lpDrawItem->hDC, lpDrawItem->rcItem.left, lpDrawItem->rcItem.top+2, pEmoji->GetCodePoints(), GetStringLength(pEmoji->GetCodePoints()));
            }
            RestoreDC(lpDrawItem->hDC, -1);

            if(!!(lpDrawItem->itemState&ODS_FOCUS))
            {
                RECT rcFocus;
                CopyRect(&rcFocus, &lpDrawItem->rcItem);
                InflateRect(&rcFocus, -1, -1);
                DrawFocusRect(lpDrawItem->hDC, &rcFocus);
            }
        }
        if((lpDrawItem->itemAction&ODA_FOCUS)==ODA_FOCUS)
        {
            RECT rcFocus;
            CopyRect(&rcFocus, &lpDrawItem->rcItem);
            InflateRect(&rcFocus, -1, -1);
            DrawFocusRect(lpDrawItem->hDC, &rcFocus);
        }

        CoSafeRelease(&pEmoji);
    }

    VOID WndProcOnMeasureItem(HWND hWnd, MEASUREITEMSTRUCT* const lpMeasureItem) override
    {
        if(lpMeasureItem->CtlType!=ODT_LISTBOX || lpMeasureItem->CtlID!=IDC_EMOJILIST)
        {
            CSuper::WndProcOnMeasureItem(hWnd, lpMeasureItem);
            return;
        }

        lpMeasureItem->itemHeight = GetEmojiHeight(hWnd);
    }

    VOID WndProcOnMouseWheel(HWND hWnd, INT nXPos, INT nYPos, INT nZDelta, UINT uKeys) override
    {
        POINT ptMouse;

        ptMouse.x = nXPos;
        ptMouse.y = nYPos;

        HWND const hWndHit = WindowFromPoint(ptMouse);

        if(hWndHit!=GetEmojiList(hWnd))
        {
            CSuper::WndProcOnMouseWheel(hWnd, nXPos, nYPos, nZDelta, uKeys);
            return;
        }

        FORWARD_WM_MOUSEWHEEL(hWndHit, nXPos, nYPos, nZDelta, uKeys, SendMessage);
    }

    LRESULT WndProcOnNotifyToolTipGetDispInfo(HWND hWnd, INT nIdFrom, LPNMTTDISPINFO lpDispInfo)
    {
        POINT ptCursor = { 0 };
        INT nIDDlgItem;

        if(lpDispInfo->hdr.hwndFrom!=m_hWndTT)
        {
            return 0;
        }

        GetCursorPos(&ptCursor);
        lpDispInfo->hinst = NULL;
        lpDispInfo->lpszText = _T("");

        if(lpDispInfo->uFlags&TTF_IDISHWND)
        {
            nIDDlgItem = GetDlgCtrlID(reinterpret_cast< HWND >(lpDispInfo->hdr.idFrom));
        }
        else
        {
            nIDDlgItem = static_cast< INT >(lpDispInfo->hdr.idFrom);
        }

        if(nIDDlgItem!=IDC_EMOJILIST)
        {
            return 0;
        }

        HWND const hWndList = GetEmojiList(hWnd);

        if(!ScreenToClient(hWndList, &ptCursor))
        {
            return 0;
        }

        LONG const lHitTest = ListBox_ItemFromPoint(hWndList, ptCursor.x, ptCursor.y);

        if(HIWORD(lHitTest))
        {
            return 0;
        }

        HRESULT hr;
        IEmoji* pEmoji = NULL;
        UINT const uItem = GetEmojiListItem(hWndList, LOWORD(lHitTest));

        hr = GetEmoji(uItem, &pEmoji);
        if(FAILED(hr))
        {
            return 0;
        }

        BvStrNCpy(m_szLastToolTip, pEmoji->GetTTS(), __ARRAYSIZE(m_szLastToolTip));
        CoSafeRelease(&pEmoji);

        lpDispInfo->lpszText = m_szLastToolTip;
        return 0;
    }

    LRESULT WndProcOnNotify(HWND hWnd, INT nIdFrom, NMHDR* lpNmHdr) override
    {
        switch(lpNmHdr->code)
        {
        case TTN_GETDISPINFO:
            return WndProcOnNotifyToolTipGetDispInfo(hWnd, nIdFrom, reinterpret_cast< LPNMTTDISPINFO >(lpNmHdr));
        default:
            break;
        }

        return CSuper::WndProcOnNotify(hWnd, nIdFrom, lpNmHdr);
    }

public:
    ~CEmojiKeyboard()
    {
        CoSafeRelease(&m_pData);

        if(m_hEmojiFont!=NULL)
        {
            DeleteFont(m_hEmojiFont);
            m_hEmojiFont = NULL;
        }
        if(m_hUIFont!=NULL)
        {
            DeleteFont(m_hUIFont);
            m_hUIFont = NULL;
        }
        if(m_hSideLoadFont!=NULL)
        {
            W32GdiRemoveFont(&m_hSideLoadFont);
        }

        if(MemIsLeaked() || MemDbgIsBadStats())
        {
            MemDbgPrintStats();
        }
    }

    CEmojiKeyboard(HINSTANCE const hInstance)
        : CWindowUI(hInstance, 0, 0, WS_VISIBLE|WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX, WS_EX_TOPMOST)
        , m_hInstance(hInstance)
        , m_hWndTT(NULL)
        , m_hSideLoadFont(NULL)
        , m_hUIFont(NULL)
        , m_hEmojiFont(NULL)
        , m_pData(NULL)
        , m_auDisplaySet(NULL)
        , m_uDisplayCount(0U)
    {
        BvStrFastInitAW(m_szSearchTerm, __ARRAYSIZE(m_szSearchTerm));
        BvStrFastInitAW(m_szLastToolTip, __ARRAYSIZE(m_szLastToolTip));
    }
};

INT WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpszCmdLine, INT nShowCmd)
{
    InitCommonControls();

    return CEmojiKeyboard(hInstance);
}
