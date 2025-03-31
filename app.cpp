#include "MemoryScanner.h"
#include "bfmeGUI.h"
#include <windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    std::string processName = "lotrbfme.exe";
    auto results = MemoryScanner::ScanMemory(processName);

    // Run the GUI to display the results
    bfmeGUI::Run("Memory Scanner", results);

    return 0;
}