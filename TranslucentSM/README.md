# TranslucentSM

A lightweight utility that makes Windows UI panels translucent using acrylic brush effects via XAML Diagnostics injection.

This is a fork of [rounk-ctrl/TranslucentSM](https://github.com/rounk-ctrl/TranslucentSM) with extended panel coverage and additional features.

## Features

### Acrylic transparency for all major panels

| Panel | Process | Status |
|-------|---------|--------|
| Start Menu | `StartMenuExperienceHost.exe` | Original |
| Search Menu | `SearchHost.exe` | Added |
| Notification Center & Calendar | `ShellExperienceHost.exe` | Added |
| System Tray Overflow Panel (`^`) | `explorer.exe` | Added |

### In-menu settings panel

Click the gear icon in the Start Menu to access:

- **TintOpacity** slider — controls main acrylic brush opacity
- **TintLuminosityOpacity** slider — controls secondary luminosity brush opacity
- **Hide search box** checkbox
- **Hide white border** checkbox
- **Hide recommended** checkbox

Settings are persisted to `HKCU\SOFTWARE\TranslucentSM` and apply immediately.

## Usage

Run `start.exe` — it injects `StartTAP.dll` into all target processes automatically.

```
Initializing...
--------------------
- Opened HKCU\SOFTWARE\TranslucentSM registry key.
- StartMenuExperienceHost.exe PID: 12345
- Injected ...\StartTAP.dll into StartMenuExperienceHost.exe
- SearchHost.exe PID: 12346
- Injected ...\StartTAP.dll into SearchHost.exe
- ShellExperienceHost.exe PID: 12347
- Injected ...\StartTAP.dll into ShellExperienceHost.exe
- explorer.exe PID: 12348
- Injecting ...\StartTAP.dll into explorer.exe via SetWindowsHookEx...
- DLL initialized successfully!
```

## How it works

The app uses `InitializeXamlDiagnosticsEx` to load `StartTAP.dll` into target processes. The DLL registers an `IVisualTreeServiceCallback2` listener and applies acrylic brush modifications when matching XAML elements appear in the visual tree.

For `explorer.exe` (which does not support cross-process injection via `InitializeXamlDiagnosticsEx`), the DLL is injected via `SetWindowsHookEx` with a `WH_CALLWNDPROC` hook. The DLL then self-initializes from within the target process by calling `InitializeXamlDiagnosticsEx` locally.

## Registry reference

`HKCU\SOFTWARE\TranslucentSM`

| Value | Default | Description |
|-------|---------|-------------|
| `TintOpacity` | 30 | Main acrylic brush opacity (0–100) |
| `TintLuminosityOpacity` | 30 | Secondary luminosity brush opacity (0–100) |
| `HideSearch` | 0 | Hide the Start Menu search box |
| `HideBorder` | 0 | Hide the white acrylic border |
| `HideRecommended` | 0 | Hide recommended section in Start Menu |
| `EditButton` | 1 | Show settings gear icon in Start Menu |

## Building

Open `start.sln` in Visual Studio 2022 and build for **x64 Release**. NuGet packages (VC-LTL, WebView2) will restore automatically.

## Credits

- Original project by [rounk-ctrl](https://github.com/rounk-ctrl/TranslucentSM)
