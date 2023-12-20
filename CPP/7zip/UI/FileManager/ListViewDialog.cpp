// ListViewDialog.cpp

#include "StdAfx.h"

#include "../../../Windows/Clipboard.h"
#include "../../../Common/IntToString.h"

#include "EditDialog.h"
#include "ListViewDialog.h"
#include "RegistryUtils.h"

#ifdef Z7_LANG
#include "LangUtils.h"
#endif

using namespace NWindows;

static const unsigned kOneStringMaxSize = 1024;


static void ListView_GetSelected(NControl::CListView &listView, CUIntVector &vector)
{
  vector.Clear();
  int index = -1;
  for (;;)
  {
    index = listView.GetNextSelectedItem(index);
    if (index < 0)
      break;
    vector.Add((unsigned)index);
  }
}


bool CListViewDialog::OnInit()
{
  #ifdef Z7_LANG
  LangSetDlgItems(*this, NULL, 0);
  #endif
  _listView.Attach(GetItem(IDL_LISTVIEW));

  if (NumColumns > 1)
  {
    LONG_PTR style = _listView.GetStyle();
    style &= ~(LONG_PTR)LVS_NOCOLUMNHEADER;
    _listView.SetStyle(style);
  }

  CFmSettings st;
  st.Load();

  DWORD exStyle = 0;
  
  if (st.SingleClick)
    exStyle |= LVS_EX_ONECLICKACTIVATE | LVS_EX_TRACKSELECT;

  exStyle |= LVS_EX_FULLROWSELECT;
  if (exStyle != 0)
    _listView.SetExtendedListViewStyle(exStyle);


  SetText(Title);

  const int kWidth = 400;
  
  LVCOLUMN columnInfo;
  columnInfo.mask = LVCF_FMT | LVCF_WIDTH | LVCF_SUBITEM;
  columnInfo.fmt = LVCFMT_LEFT;
  columnInfo.iSubItem = 0;
  columnInfo.cx = kWidth;
  columnInfo.pszText = NULL; // (TCHAR *)(const TCHAR *)""; // "Property"

  if (NumColumns > 1)
  {
    columnInfo.cx = 100;
    /*
    // Windows always uses LVCFMT_LEFT for first column.
    // if we need LVCFMT_RIGHT, we can create dummy column and then remove it

    // columnInfo.mask |= LVCF_TEXT;
    _listView.InsertColumn(0, &columnInfo);
  
    columnInfo.iSubItem = 1;
    columnInfo.fmt = LVCFMT_RIGHT;
    _listView.InsertColumn(1, &columnInfo);
    _listView.DeleteColumn(0);
    */
  }
  // else
    _listView.InsertColumn(0, &columnInfo);

  if (NumColumns > 1)
  {
    // columnInfo.fmt = LVCFMT_LEFT;
    columnInfo.cx = kWidth - columnInfo.cx;
    columnInfo.iSubItem = 1;
    // columnInfo.pszText = NULL; // (TCHAR *)(const TCHAR *)""; // "Value"
    _listView.InsertColumn(1, &columnInfo);
  }


  UString s;
  
  FOR_VECTOR (i, Strings)
  {
    _listView.InsertItem(i, Strings[i]);

    if (NumColumns > 1 && i < Values.Size())
    {
      s = Values[i];
      if (s.Len() > kOneStringMaxSize)
      {
        s.DeleteFrom(kOneStringMaxSize);
        s += " ...";
      }
      s.Replace(L"\r\n", L" ");
      s.Replace(L"\n", L" ");
      _listView.SetSubItem(i, 1, s);
    }
  }

  if (SelectFirst && Strings.Size() > 0)
    _listView.SetItemState_FocusedSelected(0);

  _listView.SetColumnWidthAuto(0);
  if (NumColumns > 1)
    _listView.SetColumnWidthAuto(1);
  StringsWereChanged = false;

  NormalizeSize();
  return CModalDialog::OnInit();
}

bool CListViewDialog::OnSize(WPARAM /* wParam */, int xSize, int ySize)
{
  int mx, my;
  GetMargins(8, mx, my);
  int bx1, bx2, by;
  GetItemSizes(IDCANCEL, bx1, by);
  GetItemSizes(IDOK, bx2, by);
  int y = ySize - my - by;
  int x = xSize - mx - bx1;

  /*
  RECT rect;
  GetClientRect(&rect);
  rect.top = y - my;
  InvalidateRect(&rect);
  */
  InvalidateRect(NULL);

  MoveItem(IDCANCEL, x, y, bx1, by);
  MoveItem(IDOK, x - mx - bx2, y, bx2, by);
  /*
  if (wParam == SIZE_MAXSHOW || wParam == SIZE_MAXIMIZED || wParam == SIZE_MAXHIDE)
    mx = 0;
  */
  _listView.Move(mx, my, xSize - mx * 2, y - my * 2);
  return false;
}


extern bool g_LVN_ITEMACTIVATE_Support;

void CListViewDialog::CopyToClipboard()
{
  CUIntVector indexes;
  ListView_GetSelected(_listView, indexes);
  UString s;
  
  FOR_VECTOR (i, indexes)
  {
    unsigned index = indexes[i];
    s += Strings[index];
    if (NumColumns > 1 && index < Values.Size())
    {
      const UString &v = Values[index];
      // if (!v.IsEmpty())
      {
        s += ": ";
        s += v;
      }
    }
    // if (indexes.Size() > 1)
    {
      s +=
        #ifdef _WIN32
          "\r\n"
        #else
          "\n"
        #endif
        ;
    }
  }
  
  ClipboardSetText(*this, s);
}


void CListViewDialog::ShowItemInfo()
{
  CUIntVector indexes;
  ListView_GetSelected(_listView, indexes);
  if (indexes.Size() != 1)
    return;
  unsigned index = indexes[0];

  CEditDialog dlg;
  if (NumColumns == 1)
    dlg.Text = Strings[index];
  else
  {
    dlg.Title = Strings[index];
    if (index < Values.Size())
      dlg.Text = Values[index];
  }
  
  #ifdef _WIN32
  if (dlg.Text.Find(L'\r') < 0)
    dlg.Text.Replace(L"\n", L"\r\n");
  #endif

  dlg.Create(*this);
}

static FString HrToString(HRESULT hr) {
  wchar_t ii[100];
  ConvertInt64ToString(hr, ii);
  FString hrs = FString(ii);
  return hrs;
}

bool GetWindowsOptionalFeatureInfo_Cleanup(DismSession* pSession, DismFeatureInfo** ppFeatureInfo, DismString* pErrorString);

bool GetWindowsOptionalFeatureInfo_NT(DismSession* pSession, DismFeatureInfo** ppFeatureInfo, UString mountPath, UString featureName)
{
  HRESULT hr = S_OK;
  HRESULT hrLocal = S_OK;
  DismString* pErrorString = NULL;

  // Initialize the API
  hr = DismInitialize(DismLogErrorsWarningsInfo, L"C:\\MyLogFile.txt", NULL);
  if (FAILED(hr))
  {
    FString message = L"DismInitialize Failed: ";
    OutputDebugStringW(message + HrToString(hr));

    return GetWindowsOptionalFeatureInfo_Cleanup(pSession, ppFeatureInfo, pErrorString);
  }

  // Open a session against the mounted image
  hr = DismOpenSession(mountPath,
    NULL,
    NULL,
    pSession);
  if (FAILED(hr))
  {
    OutputDebugStringW(L"DismOpenSession Failed: " + HrToString(hr));
    return GetWindowsOptionalFeatureInfo_Cleanup(pSession, ppFeatureInfo, pErrorString);
  }

  // Get the feature info for a non-existent feature to demonstrate error
  // functionality
  hr = DismGetFeatureInfo(*pSession,
    featureName,
    NULL,
    DismPackageNone,
    ppFeatureInfo);

  if (FAILED(hr))
  {
    OutputDebugStringW(L"DismGetFeatureInfo Failed: " + HrToString(hr));

    hrLocal = DismGetLastErrorMessage(&pErrorString);
    if (FAILED(hrLocal))
    {
      OutputDebugStringW(L"DismGetLastErrorMessage Failed: " + HrToString(hr));
      return GetWindowsOptionalFeatureInfo_Cleanup(pSession, ppFeatureInfo, pErrorString);
    }
    else
    {
      OutputDebugStringW(L"The last error string was: " + UString(pErrorString->Value));
    }
  }

  return true;
}

bool GetWindowsOptionalFeatureInfo_Cleanup(DismSession *pSession, DismFeatureInfo** ppFeatureInfo, DismString* pErrorString)
{
  HRESULT hrLocal = S_OK;

  // Delete the memory associated with the objects that were returned.
  // pFeatureInfo should be NULL due to the expected failure above, but the
  // DismDelete function will still return success in this case.
  hrLocal = DismDelete(*ppFeatureInfo);
  if (FAILED(hrLocal))
  {
    OutputDebugStringW(L"DismDelete Failed: " + HrToString(hrLocal));
  }

  hrLocal = DismDelete(pErrorString);
  if (FAILED(hrLocal))
  {
    OutputDebugStringW(L"DismDelete Failed: " + HrToString(hrLocal));
  }

  // Close the DismSession to free up resources tied to the offline session
  hrLocal = DismCloseSession(*pSession);
  if (FAILED(hrLocal))
  {
    OutputDebugStringW(L"DismCloseSession Failed: " + HrToString(hrLocal));
  }

  // Shutdown the DISM API to free up remaining resources
  hrLocal = DismShutdown();
  if (FAILED(hrLocal))
  {
    FString message = L"DismShutdown Failed: ";
    OutputDebugStringW(message + HrToString(hrLocal));
  }

  FString message = L"Return code is: ";
  OutputDebugStringW(message + HrToString(hrLocal));

  if (hrLocal == S_OK) {
    return true;
  }
  else {
    return false;
  }
}

void CListViewDialog::ShowDismItemInfo()
{
  CUIntVector indexes;
  ListView_GetSelected(_listView, indexes);
  if (indexes.Size() != 1)
    return;
  unsigned index = indexes[0];

  CEditDialog dlg;
  if (NumColumns == 1)
    dlg.Text = Strings[index];
  else
  {
    dlg.Title = Strings[index];
    DismFeatureInfo* pFeatureInfo = NULL;
    DismSession session = DISM_SESSION_DEFAULT;

    GetWindowsOptionalFeatureInfo_NT(&session, &pFeatureInfo, mountPath, dlg.Title);

    if (index < Values.Size()) {
      UString featureName = Strings[index];
      UString featureState = Values[index];

      UString text;
      
      text += L"FeatureName: " + featureName + L"\r\n";
      text += L"FeatureState: " + featureState + L"\r\n";
      text += L"DisplayName: " + UString(pFeatureInfo->DisplayName) + L"\r\n";
      text += L"Description: " + UString(pFeatureInfo->Description) + L"\r\n";

      UString restartRequired = L"No";
      switch (pFeatureInfo->RestartRequired) {
        case DismRestartNo:
          restartRequired = L"No";
          break;
        case DismRestartPossible:
          restartRequired = L"Possible";
          break;
        case DismRestartRequired:
          restartRequired = L"Required";
          break;
        default:
          restartRequired = L"No";
          break;
      }
      text += L"RestartRequired: " + restartRequired + L"\r\n";

      text += L"CustomProperty:\r\n";
      for (UINT i = 0; i < pFeatureInfo->CustomPropertyCount; ++i)
      {
        wchar_t ii[10];
        ConvertUInt32ToString(i, ii);
        text += L"CustomProperty[" + UString(ii) + L"]:\r\n";
        text += L"Name: " + UString(pFeatureInfo->CustomProperty[i].Name) + L"\r\n";
        text += L"Value: " + UString(pFeatureInfo->CustomProperty[i].Value) + L"\r\n";
        text += L"Path: " + UString(pFeatureInfo->CustomProperty[i].Path) + L"\r\n";
      }

      GetWindowsOptionalFeatureInfo_Cleanup(&session, &pFeatureInfo, NULL);

      dlg.Text = text;
    }
  }

#ifdef _WIN32
  if (dlg.Text.Find(L'\r') < 0)
    dlg.Text.Replace(L"\n", L"\r\n");
#endif

  dlg.Create(*this);
}

void CListViewDialog::DeleteItems()
{
  for (;;)
  {
    const int index = _listView.GetNextSelectedItem(-1);
    if (index < 0)
      break;
    StringsWereChanged = true;
    _listView.DeleteItem((unsigned)index);
    if ((unsigned)index < Strings.Size())
      Strings.Delete((unsigned)index);
    if ((unsigned)index < Values.Size())
      Values.Delete((unsigned)index);
  }
  const int focusedIndex = _listView.GetFocusedItem();
  if (focusedIndex >= 0)
    _listView.SetItemState_FocusedSelected(focusedIndex);
  _listView.SetColumnWidthAuto(0);
}


void CListViewDialog::OnEnter()
{
  if (IsKeyDown(VK_MENU)
      || NumColumns > 1)
  {
    if (ShowDism) {
      ShowDismItemInfo();
    }
    else {
      ShowItemInfo();
    }
    return;
  }
  OnOK();
}

bool CListViewDialog::OnNotify(UINT /* controlID */, LPNMHDR header)
{
  if (header->hwndFrom != _listView)
    return false;
  switch (header->code)
  {
    case LVN_ITEMACTIVATE:
      if (g_LVN_ITEMACTIVATE_Support)
      {
        OnEnter();
        return true;
      }
      break;
    case NM_DBLCLK:
    case NM_RETURN: // probabably it's unused
      if (!g_LVN_ITEMACTIVATE_Support)
      {
        OnEnter();
        return true;
      }
      break;

    case LVN_KEYDOWN:
    {
      LPNMLVKEYDOWN keyDownInfo = LPNMLVKEYDOWN(header);
      switch (keyDownInfo->wVKey)
      {
        case VK_DELETE:
        {
          if (!DeleteIsAllowed)
            return false;
          DeleteItems();
          return true;
        }
        case 'A':
        {
          if (IsKeyDown(VK_CONTROL))
          {
            _listView.SelectAll();
            return true;
          }
          break;
        }
        case VK_INSERT:
        case 'C':
        {
          if (IsKeyDown(VK_CONTROL))
          {
            CopyToClipboard();
            return true;
          }
          break;
        }
      }
    }
  }
  return false;
}

void CListViewDialog::OnOK()
{
  FocusedItemIndex = _listView.GetFocusedItem();
  CModalDialog::OnOK();
}
