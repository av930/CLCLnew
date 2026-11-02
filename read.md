# ClipMaster

ClipMaster is a Windows clipboard manager created as a clone of
[CLCL](https://www.nakka.com/soft/clcl/) by Ohno Tomoaki. It is based on CLCL's
source code and extends it with tile-based clipboard history, favorites,
paste-format selection and a Symbols tab for special-character input. ClipMaster
is a separate project, not an official CLCL release.

## Download

Windows 10/11 x64 binaries are available from
[GitHub Releases](https://github.com/av930/CLCLnew/releases/latest).
Download the `ClipMaster-2026.11.01-windows-x64.zip` asset, extract the entire
archive into a folder and run `CLCL.exe`. Keep the included EXE and DLL files
together. Only x64 binaries are provided; the automatic source-code archives
are not executable packages.

## Usage

For a detailed Korean walkthrough, see the [ClipMaster user guide](guide_ko.md).

- Copy text, images or files as usual.
- Press `Alt+Space` to open clipboard history and select a clip to paste.
- Press `CapsLock+F` to open favorites.
- Press `CapsLock+V` to choose a paste format.
- Select a character in the Symbols tab to insert it.
- Use the button at the bottom of Symbols to open Windows Character Map; Wingdings is selected when available.
- Change shortcuts and history retention in the Setting tab.

Copied content may be saved in clipboard history. Disable clipboard recording
before copying passwords, tokens or other sensitive information.

## License

This project is distributed under the [MIT License](LICENSE). The original CLCL
copyright and license notices are preserved.
