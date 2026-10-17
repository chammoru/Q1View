#pragma once

#include "Q1UiAppearanceWin.h"

namespace q1view {

// Incremental policy for existing commands, not a replacement command router.
enum class WindowsUiCommand { RawInput, CopyPixels, PasteImage, CompareFile, ClearSelection };
struct WindowsUiCommandContext {
	bool rawInput = true; // An empty session may preconfigure RAW interpretation.
	bool pixels = false, clipboardImage = false, file = false, selection = false;
};
struct WindowsUiCommandState { bool enabled; const wchar_t* unavailable; };
inline WindowsUiCommandState WindowsUiResolveCommand(WindowsUiCommand command, const WindowsUiCommandContext& context)
{
	switch (command) {
	case WindowsUiCommand::RawInput: return {context.rawInput, L"Raw input only"};
	case WindowsUiCommand::CopyPixels: return {context.pixels, L"No image pixels available"};
	case WindowsUiCommand::PasteImage: return {context.clipboardImage, L"No image on the clipboard"};
	case WindowsUiCommand::CompareFile: return {context.file, L"Open a file first"};
	case WindowsUiCommand::ClearSelection: return {context.selection, L"No selection to clear"};
	}
	return {false, L"Unavailable"};
}
inline WindowsUiColor WindowsUiCommandBackground(bool enabled, bool hot, bool pressed,
	WindowsUiColor normal = WindowsUiColor::SurfaceAlt)
{ return !enabled ? normal : pressed ? WindowsUiColor::Pressed : hot ? WindowsUiColor::Hover : normal; }
} // namespace q1view
