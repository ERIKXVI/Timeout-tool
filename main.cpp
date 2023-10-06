#include <Windows.h>
#include <CommCtrl.h>
#include <iostream>
#include <string>
#include <Psapi.h>
#include <vector>
#include <map>

#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "Comctl32.lib")

int selectedProcessIndex = -1;

// Declare these variables globally
HWND processListView;
HWND searchBox;
HWND blockButton;
HWND unblockButton;
HWND blockedListView;

LVITEM lvItem;
DWORD processCount;
DWORD processes[1024];
WCHAR processName[MAX_PATH];

std::map<DWORD, std::wstring> blockedProcesses;

// Function declarations
void PopulateListView();
DWORD GetProcessIdByName(const std::wstring& processName);
bool HasVisibleWindow(DWORD processId);
bool BlockInternetAccess(DWORD processId);
bool UnblockInternetAccess(DWORD processId);
void UpdateBlockedListView();
void HandleSearch(const std::wstring& searchText);
DWORD GetProcessIdByIndex(int index);

// Custom ListView drawing function (implement this with your custom drawing logic)
void DrawListViewItem(LPDRAWITEMSTRUCT lpDrawItem) {
    // Implement the drawing logic for your ListView items here
    // You can use lvItem and the processName to draw the items
    // ...
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        // Create ListView
        processListView = CreateWindowW(WC_LISTVIEW, L"",
            WS_VISIBLE | WS_CHILD | LVS_REPORT | LVS_SINGLESEL,
            10, 90, 400, 200, hwnd, NULL, NULL, NULL);

        // Create columns for the ListView
        LVCOLUMN lvc = {};
        lvc.mask = LVCF_TEXT | LVCF_WIDTH;
        lvc.cx = 100;
        lvc.pszText = const_cast<LPWSTR>(L"Process Name");
        ListView_InsertColumn(processListView, 0, &lvc);

        // Enable custom drawing for the ListView
        ListView_SetExtendedListViewStyle(processListView, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);

        // Create TextBox
        searchBox = CreateWindowW(L"Edit", L"",
            WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL,
            10, 10, 200, 30, hwnd, NULL, NULL, NULL);

        // Create Block Button
        blockButton = CreateWindowW(L"Button", L"Block Internet Access",
            WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            220, 10, 200, 30, hwnd, (HMENU)1, NULL, NULL);

        // Create Unblock Button
        unblockButton = CreateWindowW(L"Button", L"Unblock Internet Access",
            WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            220, 50, 200, 30, hwnd, (HMENU)2, NULL, NULL);

        // Create Blocked ListView
        blockedListView = CreateWindowW(WC_LISTVIEW, L"",
            WS_VISIBLE | WS_CHILD | LVS_REPORT,
            10, 300, 400, 200, hwnd, NULL, NULL, NULL);

        // Create columns for the Blocked ListView
        lvc.cx = 200;
        lvc.pszText = const_cast<LPWSTR>(L"Blocked Process");
        ListView_InsertColumn(blockedListView, 0, &lvc);

        // Populate the ListView with processes
        PopulateListView();

        break;
    }
    case WM_COMMAND: {
        if (HIWORD(wParam) == BN_CLICKED) {
            if (LOWORD(wParam) == 1) {
                // Handle block button click
                if (selectedProcessIndex != -1) {
                    DWORD selectedProcessId = GetProcessIdByIndex(selectedProcessIndex);
                    std::wcout << "Block Internet Access button clicked for Process ID: " << selectedProcessId << std::endl;
                    if (BlockInternetAccess(selectedProcessId)) {
                        blockedProcesses[selectedProcessId] = std::wstring();
                        UpdateBlockedListView();
                    }
                }
            }
            else if (LOWORD(wParam) == 2) {
                // Handle unblock button click
                if (selectedProcessIndex != -1) {
                    DWORD selectedProcessId = GetProcessIdByIndex(selectedProcessIndex);
                    std::wcout << "Unblock Internet Access button clicked for Process ID: " << selectedProcessId << std::endl;
                    if (UnblockInternetAccess(selectedProcessId)) {
                        blockedProcesses.erase(selectedProcessId);
                        UpdateBlockedListView();
                    }
                }
            }
        }
        break;
    }
    case WM_NOTIFY: {
        LPNMHDR pnmh = (LPNMHDR)lParam;
        if (pnmh->code == NM_CUSTOMDRAW && pnmh->hwndFrom == processListView) {
            LPNMLVCUSTOMDRAW lplvcd = (LPNMLVCUSTOMDRAW)lParam;
            if (lplvcd->nmcd.dwDrawStage == CDDS_PREPAINT) {
                // Request item-specific notifications for custom drawing
                return CDRF_NOTIFYITEMDRAW;
            }
            else if (lplvcd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                // Handle custom drawing for individual items
                if (lplvcd->nmcd.uItemState & CDIS_SELECTED) {
                    // Item is selected, set the background color
                    lplvcd->clrTextBk = RGB(173, 216, 230); // Light blue
                    return CDRF_NEWFONT;
                }
            }
        }
        if (pnmh->code == LVN_ITEMCHANGED && pnmh->hwndFrom == processListView) {
            LPNMLISTVIEW pNMLV = (LPNMLISTVIEW)lParam;
            if ((pNMLV->uChanged & LVIF_STATE) && (pNMLV->uNewState & LVIS_SELECTED)) {
                // Update the selected process index when a new item is selected in the ListView
                selectedProcessIndex = pNMLV->iItem;
            }
        }
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}

// Function to get the process ID by index
DWORD GetProcessIdByIndex(int index) {
    lvItem.mask = LVIF_PARAM;
    lvItem.iItem = index;
    lvItem.iSubItem = 0;
    ListView_GetItem(processListView, &lvItem);
    return static_cast<DWORD>(lvItem.lParam);
}

// Function to populate the ListView with processes
void PopulateListView() {
    DWORD cbNeeded;
    if (EnumProcesses(processes, sizeof(processes), &cbNeeded)) {
        processCount = cbNeeded / sizeof(DWORD);

        lvItem.mask = LVIF_TEXT | LVIF_PARAM; // Add LVIF_PARAM to store process ID
        lvItem.cchTextMax = 256;

        for (DWORD i = 0; i < processCount; i++) {
            DWORD processId = processes[i];
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
            if (hProcess) {
                if (GetModuleBaseNameW(hProcess, NULL, processName, sizeof(processName) / sizeof(WCHAR))) {
                    lvItem.iItem = i;
                    lvItem.iSubItem = 0;
                    lvItem.pszText = processName;
                    lvItem.lParam = static_cast<LPARAM>(processId); // Store the process ID as lParam
                    ListView_InsertItem(processListView, &lvItem);
                }
                CloseHandle(hProcess);
            }
        }
    }
}

// Function to handle the search functionality
void HandleSearch(const std::wstring& searchText) {
    // Clear the selection when searching
    selectedProcessIndex = -1;

    // Clear the ListView
    ListView_DeleteAllItems(processListView);

    lvItem.mask = LVIF_TEXT | LVIF_PARAM; // Add LVIF_PARAM to store process ID
    lvItem.cchTextMax = 256;

    for (DWORD i = 0; i < processCount; i++) {
        DWORD processId = processes[i];
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
        if (hProcess) {
            if (GetModuleBaseNameW(hProcess, NULL, processName, sizeof(processName) / sizeof(WCHAR))) {
                // Check if the process name contains the search text
                if (searchText.empty() || wcsstr(processName, searchText.c_str()) != nullptr) {
                    lvItem.iItem = i;
                    lvItem.iSubItem = 0;
                    lvItem.pszText = processName;
                    lvItem.lParam = static_cast<LPARAM>(processId); // Store the process ID as lParam
                    ListView_InsertItem(processListView, &lvItem);
                }
            }
            CloseHandle(hProcess);
        }
    }
}

// Function to update the Blocked ListView
void UpdateBlockedListView() {
    // Clear the Blocked ListView
    ListView_DeleteAllItems(blockedListView);

    lvItem.mask = LVIF_TEXT;

    for (const auto& kvp : blockedProcesses) {
        lvItem.iItem = 0; // You can adjust the index as needed
        lvItem.iSubItem = 0;
        lvItem.pszText = const_cast<LPWSTR>(kvp.second.c_str());
        ListView_InsertItem(blockedListView, &lvItem);
    }
}

// Function to check if a process has a visible window (user interface)
bool HasVisibleWindow(DWORD processId) {
    HWND hwnd = FindWindowExW(nullptr, nullptr, nullptr, nullptr);
    while (hwnd) {
        DWORD windowProcessId;
        GetWindowThreadProcessId(hwnd, &windowProcessId);
        if (windowProcessId == processId) {
            if (IsWindowVisible(hwnd)) {
                return true;
            }
        }
        hwnd = FindWindowExW(nullptr, hwnd, nullptr, nullptr);
    }
    return false;
}

// Function to block internet access using Windows Firewall for a specified process ID
bool BlockInternetAccess(DWORD processId) {
    if (HasVisibleWindow(processId)) {
        std::wstring powershellCmd = L"powershell -Command \"";
        powershellCmd += L"$processId = ";
        powershellCmd += std::to_wstring(processId);
        powershellCmd += L"; ";
        powershellCmd += L"try { New-NetFirewallRule -DisplayName 'Block Outgoing' -Direction Outbound -Action Block -Program (Get-Process -Id $processId).Path -ErrorAction Stop }";
        powershellCmd += L"catch { exit 1 }\"";

        int result = _wsystem(powershellCmd.c_str());

        return (result == 0);
    }
    else {
        std::wcerr << "Skipping background process with ID: " << processId << std::endl;
        return false;
    }
}

// Function to unblock internet access for a specified process ID
bool UnblockInternetAccess(DWORD processId) {
    std::wstring powershellCmd = L"powershell -Command \"";
    powershellCmd += L"$processId = ";
    powershellCmd += std::to_wstring(processId);
    powershellCmd += L"; ";
    powershellCmd += L"Get-NetFirewallRule -DisplayName 'Block Outgoing' | Where-Object { $_.Program -eq (Get-Process -Id $processId).Path } | Remove-NetFirewallRule -ErrorAction Stop\"";

    int result = _wsystem(powershellCmd.c_str());

    return (result == 0);
}

// Function to get the process ID by process name
DWORD GetProcessIdByName(const std::wstring& processName) {
    DWORD processes[1024];
    DWORD cbNeeded;
    if (EnumProcesses(processes, sizeof(processes), &cbNeeded)) {
        DWORD processCount = cbNeeded / sizeof(DWORD);

        for (DWORD i = 0; i < processCount; i++) {
            DWORD processId = processes[i];
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
            if (hProcess) {
                WCHAR buffer[MAX_PATH];
                if (GetModuleBaseNameW(hProcess, NULL, buffer, sizeof(buffer) / sizeof(WCHAR))) {
                    if (processName == buffer) {
                        CloseHandle(hProcess);
                        return processId;
                    }
                }
                CloseHandle(hProcess);
            }
        }
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"My Window Class";

    WNDCLASS wc = {};

    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Process Control Window",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 600,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (hwnd == NULL) {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
