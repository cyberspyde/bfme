#include "connect.h"
#include "bfmeGUI.h"
#include "MemoryScanner.h"
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <iomanip>
#include "modify.h"
#include "other.h"

// Global variables
HINSTANCE hInst;
std::vector<std::pair<DWORD_PTR, DWORD>> guiResults;
std::string processName;
std::map<int, DWORD_PTR> buttonToAddressMap; // Map button IDs to addresses
std::map<DWORD_PTR, HWND> addressToValueLabelMap; // Map addresses to their value display labels
int selectedAddressIndex = -1; // Index of the selected address
HWND hEditValue = NULL; // Handle to the edit control for value modification
HWND hModifyButton = NULL; // Handle to the modify button
HWND hStatusBar = NULL; // Handle to the status bar
bool guiInitialized = false; // Flag to track if GUI has been initialized

// Forward declarations
void CreateButtons(HWND hwnd);
void UpdateValueLabels(HWND hwnd);

// Function to convert results to a string
std::string GetResultsAsString() {
    std::ostringstream oss;
    for (size_t i = 0; i < guiResults.size(); ++i) {
        const auto& [address, value] = guiResults[i];
        oss << "Address: 0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << address 
            << " -> Value: " << std::dec << value << "\r\n";
    }
    return oss.str();
}

// Function to update only the value displays
void UpdateValueLabels(HWND hwnd) {
    // Store currently selected address if any
    DWORD_PTR selectedAddress = (selectedAddressIndex >= 0 && selectedAddressIndex < guiResults.size()) ? 
                               guiResults[selectedAddressIndex].first : 0;
    
    // Get the latest memory values
    std::vector<std::pair<DWORD_PTR, DWORD>> newResults = MemoryScanner::ScanMemory(processName);
    
    // Check if addresses changed (added or removed)
    bool addressesChanged = (newResults.size() != guiResults.size());
    if (!addressesChanged) {
        for (size_t i = 0; i < newResults.size(); i++) {
            if (newResults[i].first != guiResults[i].first) {
                addressesChanged = true;
                break;
            }
        }
    }
    
    // If addresses changed, recreate the entire UI
    if (addressesChanged) {

        // Reset the selected address index
        selectedAddressIndex = -1;

        // Disable the Modify Value field and button
        EnableWindow(hEditValue, FALSE);
        EnableWindow(hModifyButton, FALSE);

        // Clear the edit field
        SetWindowTextA(hEditValue, ""); 

        // Clear button mapping and destroy old controls
        for (const auto& pair : buttonToAddressMap) {
            DestroyWindow(GetDlgItem(hwnd, pair.first));
        }
        buttonToAddressMap.clear();
        addressToValueLabelMap.clear();
        
        // Update results and recreate UI
        guiResults = newResults;
        CreateButtons(hwnd);
        
        // Update status bar with new count
        std::ostringstream status;
        status << "Monitoring process: " << processName << " | Addresses found: " << guiResults.size();
        SetWindowTextA(hStatusBar, status.str().c_str());
        
        // Force full repaint
        InvalidateRect(hwnd, NULL, TRUE);
        
        // Try to restore selection
        selectedAddressIndex = -1;
        for (size_t i = 0; i < guiResults.size(); ++i) {
            if (guiResults[i].first == selectedAddress) {
                selectedAddressIndex = i;
                break;
            }
        }
        
        // Update UI for selection state
        if (selectedAddressIndex >= 0) {
            EnableWindow(hEditValue, TRUE);
            EnableWindow(hModifyButton, TRUE);
            
            // Update status bar
            status.str("");
            status << "Selected address: 0x" << std::hex << std::uppercase << std::setw(8) 
                  << std::setfill('0') << guiResults[selectedAddressIndex].first;
            SetWindowTextA(hStatusBar, status.str().c_str());
        } else {
            EnableWindow(hEditValue, FALSE);
            EnableWindow(hModifyButton, FALSE);
        }

        // Update the status bar with the number of connected clients
        int connectedClients = getConnectedClientsCount();
        status.str(""); // Clear the status stream
        status << "Monitoring process: " << processName 
            << " | Addresses found: " << guiResults.size()
            << " | Clients connected: " << connectedClients;
        SetWindowTextA(hStatusBar, status.str().c_str());
    } else {
        // Update only the value labels
        for (size_t i = 0; i < newResults.size(); ++i) {
            const auto& [address, newValue] = newResults[i];
            DWORD oldValue = guiResults[i].second;
            
            // Update the actual value in our data
            guiResults[i].second = newValue;
            
            // Only update the display if the value changed
            if (newValue != oldValue && addressToValueLabelMap.find(address) != addressToValueLabelMap.end()) {
                HWND valueLabel = addressToValueLabelMap[address];
                std::ostringstream valStr;
                valStr << std::dec << newValue;
                SetWindowTextA(valueLabel, valStr.str().c_str());
            }
        }

        // Update the status bar with the number of connected clients
        int connectedClients = getConnectedClientsCount();
        std::ostringstream status;
        status << "Monitoring process: " << processName 
            << " | Addresses found: " << guiResults.size()
            << " | Clients connected: " << connectedClients;
        SetWindowTextA(hStatusBar, status.str().c_str());
    }
}

// Function to update the results by scanning memory
void UpdateResults(HWND hwnd) {
    if (!guiInitialized) {
        // First time initialization
        guiResults = MemoryScanner::ScanMemory(processName);
        CreateButtons(hwnd);
        guiInitialized = true;
    } else {
        // Just update the values
        UpdateValueLabels(hwnd);
    }
}

// Function to create buttons dynamically
void CreateButtons(HWND hwnd) {
    // Clear existing maps
    buttonToAddressMap.clear();
    addressToValueLabelMap.clear();
    
    // Get client area dimensions
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    int clientWidth = clientRect.right - clientRect.left;
    int clientHeight = clientRect.bottom - clientRect.top;
    
    // Calculate dimensions for results area and controls
    int listWidth = clientWidth - 40;  // 20px padding on each side
    int rowHeight = 40;
    int addressWidth = listWidth * 0.6;
    int valueWidth = listWidth * 0.2;
    int buttonWidth = listWidth * 0.15;
    
    // Create header labels
    HWND hAddressLabel = CreateWindowA(
        "STATIC", "Memory Address",
        WS_VISIBLE | WS_CHILD | SS_CENTER,
        20, 20, addressWidth, 30,
        hwnd, NULL, hInst, NULL);
    
    HWND hValueLabel = CreateWindowA(
        "STATIC", "Value",
        WS_VISIBLE | WS_CHILD | SS_CENTER,
        20 + addressWidth + 10, 20, valueWidth, 30,
        hwnd, NULL, hInst, NULL);
    
    HWND hActionLabel = CreateWindowA(
        "STATIC", "Action",
        WS_VISIBLE | WS_CHILD | SS_CENTER,
        20 + addressWidth + valueWidth + 20, 20, buttonWidth, 30,
        hwnd, NULL, hInst, NULL);
    
    // Set font for headers
    HFONT hHeaderFont = CreateFont(
        18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    
    SendMessage(hAddressLabel, WM_SETFONT, (WPARAM)hHeaderFont, TRUE);
    SendMessage(hValueLabel, WM_SETFONT, (WPARAM)hHeaderFont, TRUE);
    SendMessage(hActionLabel, WM_SETFONT, (WPARAM)hHeaderFont, TRUE);
    
    // Create list items
    int buttonId = 1000; // Start button IDs from 1000
    int yPos = 60;       // Start position after headers
    
    HFONT hItemFont = CreateFont(
        16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    
    for (size_t i = 0; i < guiResults.size(); ++i) {
        const auto& [address, value] = guiResults[i];
        
        // Address display
        std::ostringstream addrStr;
        addrStr << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << address;
        
        HWND hAddr = CreateWindowA(
            "STATIC", addrStr.str().c_str(),
            WS_VISIBLE | WS_CHILD | SS_LEFT,
            20, yPos, addressWidth, rowHeight,
            hwnd, NULL, hInst, NULL);
        SendMessage(hAddr, WM_SETFONT, (WPARAM)hItemFont, TRUE);
        
        // Value display
        std::ostringstream valStr;
        valStr << std::dec << value;
        
        HWND hVal = CreateWindowA(
            "STATIC", valStr.str().c_str(),
            WS_VISIBLE | WS_CHILD | SS_LEFT,
            20 + addressWidth + 10, yPos, valueWidth, rowHeight,
            hwnd, (HMENU)(2000 + i), hInst, NULL);  // Use unique IDs for value labels
        SendMessage(hVal, WM_SETFONT, (WPARAM)hItemFont, TRUE);
        
        // Store the handle to the value label for future updates
        addressToValueLabelMap[address] = hVal;
        
        // Select button
        HWND hButton = CreateWindowA(
            "BUTTON", "Select",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            20 + addressWidth + valueWidth + 20, yPos, buttonWidth, rowHeight - 10,
            hwnd, (HMENU)(LONG_PTR)buttonId, hInst, NULL);
        SendMessage(hButton, WM_SETFONT, (WPARAM)hItemFont, TRUE);
        
        // Map the button ID to the address
        buttonToAddressMap[buttonId] = address;
        
        yPos += rowHeight;
        buttonId++;
    }
    
    // Create bottom panel for value modification
    int bottomPanelY = clientHeight - 120;
    
    // Separator line
    HWND hSeparator = CreateWindowA(
        "STATIC", "",
        WS_VISIBLE | WS_CHILD | SS_ETCHEDHORZ,
        10, bottomPanelY, clientWidth - 20, 2,
        hwnd, NULL, hInst, NULL);
    
    // "New Value" label
    HWND hNewValueLabel = CreateWindowA(
        "STATIC", "New Value:",
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        20, bottomPanelY + 20, 100, 30,
        hwnd, NULL, hInst, NULL);
    SendMessage(hNewValueLabel, WM_SETFONT, (WPARAM)hItemFont, TRUE);
    
    // Edit control for new value
    hEditValue = CreateWindowA(
        "EDIT", "",
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
        130, bottomPanelY + 20, 150, 30,
        hwnd, (HMENU)9998, hInst, NULL);
    SendMessage(hEditValue, WM_SETFONT, (WPARAM)hItemFont, TRUE);
    
    // Modify button
    hModifyButton = CreateWindowA(
        "BUTTON", "Modify Value",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        300, bottomPanelY + 20, 150, 30,
        hwnd, (HMENU)9999, hInst, NULL);
    SendMessage(hModifyButton, WM_SETFONT, (WPARAM)hItemFont, TRUE);
    
    // Reconnect button
    HWND hReconnectButton = CreateWindowA(
        "BUTTON", "Reconnect",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        470, bottomPanelY + 20, 150, 30,
        hwnd, (HMENU)10000, hInst, NULL);
    SendMessage(hReconnectButton, WM_SETFONT, (WPARAM)hItemFont, TRUE);
    
    // Other button
    HWND hOtherButton = CreateWindowA(
        "BUTTON", "Other",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        640, bottomPanelY + 20, 150, 30,
        hwnd, (HMENU)10001, hInst, NULL);
    SendMessage(hOtherButton, WM_SETFONT, (WPARAM)hItemFont, TRUE);
    
    // Disable edit and modify button initially
    EnableWindow(hEditValue, FALSE);
    EnableWindow(hModifyButton, FALSE);
    
    // Create status bar
    hStatusBar = CreateWindowA(
        "STATIC", "",
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        10, clientHeight - 30, clientWidth - 20, 20,
        hwnd, NULL, hInst, NULL);
    
    // Set initial status
    std::ostringstream status;
    status << "Monitoring process: " << processName << " | Addresses found: " << guiResults.size();
    SetWindowTextA(hStatusBar, status.str().c_str());
}

// Window procedure for handling messages
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        // Set a timer to update results every 2 seconds (2000 ms)
        SetTimer(hwnd, 1, 2000, NULL);
    
        // Update the status bar with the initial connected clients count
        int connectedClients = getConnectedClientsCount();
        std::ostringstream status;
        status << "Monitoring process: " << processName 
                << " | Addresses found: " << guiResults.size()
                << " | Clients connected: " << connectedClients;
        SetWindowTextA(hStatusBar, status.str().c_str());
        break;
    }
    case WM_SIZE: {
        // Window has been resized, recreate the UI
        if (wParam != SIZE_MINIMIZED) {
            // Reset GUI initialized flag to force full recreation
            guiInitialized = false;
            UpdateResults(hwnd);
        }
        break;
    }
    case WM_TIMER: {
        // Timer event: Update the results
        UpdateResults(hwnd);
        break;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = (HDC)wParam;
        SetTextColor(hdcStatic, RGB(220, 220, 220));
        SetBkColor(hdcStatic, RGB(40, 40, 40));
        static HBRUSH hBrush = CreateSolidBrush(RGB(40, 40, 40));
        return (LRESULT)hBrush;
    }
    case WM_CTLCOLOREDIT: {
        HDC hdcEdit = (HDC)wParam;
        SetTextColor(hdcEdit, RGB(220, 220, 220));
        SetBkColor(hdcEdit, RGB(50, 50, 50));
        static HBRUSH hBrush = CreateSolidBrush(RGB(50, 50, 50));
        return (LRESULT)hBrush;
    }
    case WM_CTLCOLORBTN: {
        HDC hdcBtn = (HDC)wParam;
        SetTextColor(hdcBtn, RGB(220, 220, 220));
        SetBkColor(hdcBtn, RGB(60, 60, 60));
        static HBRUSH hBrush = CreateSolidBrush(RGB(60, 60, 60));
        return (LRESULT)hBrush;
    }
    case WM_COMMAND: {
        int buttonId = LOWORD(wParam);

        // Handle "Select" button clicks
        if (buttonToAddressMap.find(buttonId) != buttonToAddressMap.end()) {
            // Deselect previous button if any
            if (selectedAddressIndex >= 0 && selectedAddressIndex < guiResults.size()) {
                SendMessage(GetDlgItem(hwnd, 1000 + selectedAddressIndex), BM_SETSTATE, FALSE, 0);
            }
            
            // Set new selection
            selectedAddressIndex = buttonId - 1000;
            
            // Update UI for selection
            if (selectedAddressIndex >= 0 && selectedAddressIndex < guiResults.size()) {
                // Enable edit control and modify button
                EnableWindow(hEditValue, TRUE);
                EnableWindow(hModifyButton, TRUE);
                
                // Set the current value in the edit control
                std::ostringstream oss;
                oss << guiResults[selectedAddressIndex].second;
                SetWindowTextA(hEditValue, oss.str().c_str());
                
                // Update status bar
                std::ostringstream status;
                status << "Selected address: 0x" << std::hex << std::uppercase << std::setw(8) 
                        << std::setfill('0') << guiResults[selectedAddressIndex].first;
                SetWindowTextA(hStatusBar, status.str().c_str());

                std::ostringstream command;
                command << "SELECT " << selectedAddressIndex;
                std::string commandStr = command.str();
                sendCommandToClients(commandStr);
            } else {
                EnableWindow(hEditValue, FALSE);
                EnableWindow(hModifyButton, FALSE);

                SetWindowTextA(hEditValue, "");
            }

        }

        if (buttonId == 10000) {
            issueReconnect();

            // Update the results to refresh the GUI
            UpdateResults(hwnd);
        
            // Reset the selected address index
            selectedAddressIndex = -1;
        
            // Disable the Modify Value field and button
            EnableWindow(hEditValue, FALSE);
            EnableWindow(hModifyButton, FALSE);
        
            // Clear the edit field
            SetWindowTextA(hEditValue, "");
        
            // Update the status bar
            std::ostringstream status;
            status << "Monitoring process: " << processName 
                   << " | Addresses found: " << guiResults.size()
                   << " | Clients connected: " << getConnectedClientsCount();
            SetWindowTextA(hStatusBar, status.str().c_str());
        }

        if (buttonId == 10001) {
            other();
        }

        if (buttonId == 9999) {
            // Get the new value from the edit control
            char newValueStr[32] = {0};
            GetWindowTextA(hEditValue, newValueStr, sizeof(newValueStr));
            DWORD newValue = std::atoi(newValueStr);
            
            // Modify the value locally
            ModifyValue(hwnd, selectedAddressIndex, guiResults, hEditValue);
            
            // Send command to clients with the new value
            std::ostringstream command;
            command << "MODIFY " << selectedAddressIndex << " " << newValue;
            std::string commandStr = command.str();
            sendCommandToClients(commandStr);
        }
        break;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Set background color
        HBRUSH brush = CreateSolidBrush(RGB(40, 40, 40)); // Dark gray background
        FillRect(hdc, &ps.rcPaint, brush);
        DeleteObject(brush);

        EndPaint(hwnd, &ps);
        break;
    }
    case WM_DESTROY:
        // Kill the timer when the window is destroyed
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// Function to initialize and run the GUI
void bfmeGUI::Run(const std::string& title, const std::vector<std::pair<DWORD_PTR, DWORD>>& initialResults) {
    guiResults = initialResults;

    // Store the process name globally for real-time updates
    processName = "lotrbfme.exe";
    
    hInst = GetModuleHandle(NULL);
    if (!hInst) {
        MessageBoxA(NULL, "Failed to get module handle!", "Error", MB_ICONERROR | MB_OK);
        return;
    }

    // Define the window class
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // Default background
    wc.lpszClassName = "bfmeGUIWindow";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassA(&wc)) {
        MessageBoxA(NULL, "Failed to register window class!", "Error", MB_ICONERROR | MB_OK);
        return;
    }
    // Create the window
    HWND hwnd = CreateWindowExA(
        0,
        "bfmeGUIWindow",
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, // Larger window size
        NULL, NULL, hInst, NULL);

    if (hwnd == NULL) {
        MessageBoxA(NULL, "Failed to create window!", "Error", MB_ICONERROR | MB_OK);
        return;
    }

    // Show the window
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // Run the message loop
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (msg.message == -1) {
        MessageBoxA(NULL, "Error in message loop!", "Error", MB_ICONERROR | MB_OK);
    }
}