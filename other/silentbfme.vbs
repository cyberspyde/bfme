Option Explicit

Dim objShell, strCommand

' Path to your executable - update this path to wherever client.exe is located
strCommand = """C:\Path\To\client.exe"""

' Create Shell object
Set objShell = CreateObject("WScript.Shell")

' Run the application with window hidden (0)
objShell.Run strCommand, 0, True

' Clean up
Set objShell = Nothing