#include <iostream>
#include <string>
#include <Windows.h>
#include <Psapi.h>
#include <conio.h>
#include <fstream>
#include <vector>

#pragma comment(lib, "Psapi.lib")

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
    } else {
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

// Function to list and display running user-level processes (apps) with visible windows
void ListRunningUserProcessesWithWindows() {
    std::wcout << "======================================" << std::endl;
    std::wcout << "Running User-Level Processes with Windows (Apps):" << std::endl;
    std::wcout << "======================================" << std::endl;

    DWORD processes[1024];
    DWORD needed;
    if (EnumProcesses(processes, sizeof(processes), &needed)) {
        const DWORD count = needed / sizeof(DWORD);
        for (DWORD i = 0; i < count; ++i) {
            const DWORD processId = processes[i];
            if (HasVisibleWindow(processId)) {
                HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
                if (hProcess != nullptr) {
                    WCHAR processPath[MAX_PATH];
                    if (K32GetModuleFileNameExW(hProcess, nullptr, processPath, MAX_PATH)) {
                        std::wcout << i + 1 << ". Process ID: " << processId << std::endl;
                        std::wcout << "Path: " << processPath << std::endl;
                        std::wcout << "--------------------------------------" << std::endl;
                    }
                    CloseHandle(hProcess);
                }
            }
        }
    }
}

// Function to list blocked processes and their firewall rules
void ListBlockedProcesses() {
    std::wcout << "======================================" << std::endl;
    std::wcout << "Blocked Processes:" << std::endl;
    std::wcout << "======================================" << std::endl;

    std::wstring powershellCmd = L"powershell -Command \"";
    powershellCmd += L"Get-NetFirewallRule -DisplayName 'Block Outgoing' | Select-Object -Property DisplayName, Description, Program | Format-Table -AutoSize\"";

    FILE* pipe = _wpopen(powershellCmd.c_str(), L"rt");
    if (pipe) {
        wchar_t buffer[128];
        while (fgetws(buffer, sizeof(buffer) / sizeof(wchar_t), pipe)) {
            std::wcout << buffer;
        }
        _pclose(pipe);
    } else {
        std::wcerr << "Failed to list blocked processes." << std::endl;
    }
}

int main() {
    while (true) {
        system("cls");

        std::wcout << "Options:" << std::endl;
        std::wcout << "1. List running user-level processes (apps) with windows and block internet access for one" << std::endl;
        std::wcout << "2. List blocked processes" << std::endl;
        std::wcout << "3. Unblock internet access for a process" << std::endl;
        std::wcout << "4. Exit" << std::endl;
        std::wcout << "Enter your choice: ";

        int choice;
        std::wcin >> choice;

        switch (choice) {
            case 1: {
                ListRunningUserProcessesWithWindows();
                std::wcout << "Enter the number of the process to block internet access for: ";
                int processNumber;
                std::wcin >> processNumber;
                if (processNumber >= 1) {
                    DWORD processes[1024];
                    DWORD needed;
                    if (EnumProcesses(processes, sizeof(processes), &needed)) {
                        const DWORD count = needed / sizeof(DWORD);
                        if (processNumber <= static_cast<int>(count)) {
                            if (BlockInternetAccess(processes[processNumber - 1])) {
                                std::wcout << "Internet access blocked for the selected process." << std::endl;
                            } else {
                                std::wcerr << "Failed to block internet access for the selected process." << std::endl;
                            }
                        } else {
                            std::wcerr << "Invalid process number." << std::endl;
                        }
                    }
                } else {
                    std::wcerr << "Invalid input." << std::endl;
                }
                break;
            }
            case 2:
                ListBlockedProcesses();
                break;
            case 3: {
                std::wcout << "Enter the Process ID to unblock internet access for: ";
                DWORD processId;
                std::wcin >> processId;
                if (UnblockInternetAccess(processId)) {
                    std::wcout << "Internet access unblocked for the selected process." << std::endl;
                } else {
                    std::wcerr << "Failed to unblock internet access for the selected process." << std::endl;
                }
                break;
            }
            case 4:
                return 0;
            default:
                std::wcerr << "Invalid choice. Please enter a valid option." << std::endl;
                break;
        }

        std::wcout << "Press any key to continue...";
        _getch();
    }

    return 0;
}
