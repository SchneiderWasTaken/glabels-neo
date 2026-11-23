' Starts the PowerShell launcher without flashing a console window.
' The Start menu shortcut runs this file through wscript.exe.
Option Explicit

Dim sh, fso, dir, cmd, i
Set sh = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
dir = fso.GetParentFolderName(WScript.ScriptFullName)

cmd = "powershell.exe -NoProfile -ExecutionPolicy Bypass -File """ & dir & "\glabels-neo-launcher.ps1"""
For i = 0 To WScript.Arguments.Count - 1
    cmd = cmd & " """ & Replace(WScript.Arguments(i), """", "\""") & """"
Next

' Window style 0 = hidden; do not wait for the program to exit.
sh.Run cmd, 0, False
