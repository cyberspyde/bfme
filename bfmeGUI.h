#ifndef BFME_GUI_H
#define BFME_GUI_H

#include <windows.h>
#include <string>
#include <vector>
#include <utility>

class bfmeGUI {
public:
    // Function to initialize and run the GUI
    static void Run(const std::string& title, const std::vector<std::pair<DWORD_PTR, DWORD>>& results);
};

#endif // SIMPLE_GUI_H