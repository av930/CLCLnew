# Refactoring Notes

## Scope

- Removed two unreferenced hotkey parsers from `main.c`. Active hotkeys still use `cm_hotkey_parse`.
- Reduced `CLCLSet.exe` to its existing launcher behavior: open the main application's Settings tab, or start `CLCL.exe /cmsettings` when needed.
- Removed 25 unreachable settings-only source/header files, obsolete dialogs and strings. Kept launcher icons, version information and manifest.
- Removed duplicate compilation of shared application sources from the settings launcher project.
- Removed inactive `OPTION_SET` branches from history file loading and INI persistence, retaining the existing main-application path.
- Unified history/favorites refresh handling and preserved hidden legacy control visibility during redraw.

## Preserved Behavior

Clipboard monitoring, text/image clips, history persistence, favorites, PastePlus, Settings, Alt+Space, tile sizing and plugin interfaces remain supported.

Legacy tree/list/format controls inside the main application remain because editing, synchronization, menus and plugin previews still reference them. They are not equivalent to the removed, unreachable settings UI.

User history and configuration formats are unchanged. Existing user files are not migrated or deleted by this refactoring.

## Validation

Build from the repository directory using Visual Studio 2022 Build Tools:

```powershell
MSBuild.exe CLCL.sln /t:Rebuild /p:Configuration=Release /p:Platform=x86 /m
MSBuild.exe CLCL.sln /t:Rebuild /p:Configuration=Debug /p:Platform=x86 /m
```

Run the x86 application smoke tests in a Windows PowerShell STA session with local script execution permitted:

```powershell
& .\tests\SmokeTests.ps1 -BuildDirectory .\Release
& .\tests\SmokeTests.ps1 -BuildDirectory .\Debug
```

The tests copy the executables into a temporary portable directory. They verify text/image capture, grid sizing, all four tabs, hidden legacy controls, hotkey registration, save/restart persistence and the settings launcher. History timers are driven explicitly to avoid timing-dependent assertions.

The original clipboard is restored, the temporary instance is closed and test files are deleted. A running instance from the selected build directory is closed normally and restarted afterward. Do not copy new clipboard content while the tests are running.

The tests inspect x86 tile structures and therefore must be updated if those layouts change. They do not automate external-application pasting, custom PowerShell handlers, third-party plugins or installer elevation; those remain manual integration checks.
