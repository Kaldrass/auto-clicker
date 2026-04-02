
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>

#include <atomic>
#include <chrono>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{
constexpr int kWindowWidth = 1040;
constexpr int kWindowHeight = 1040;

constexpr int kModeSingle = 1001;
constexpr int kModeMacro = 1002;
constexpr int kEditSingleX = 1101;
constexpr int kEditSingleY = 1102;
constexpr int kComboSingleButton = 1103;
constexpr int kEditSingleRepeats = 1104;
constexpr int kEditSingleInterval = 1105;
constexpr int kRadioOutputCursor = 1151;
constexpr int kRadioOutputBackground = 1152;
constexpr int kRadioOutputBackgroundPlus = 1156;
constexpr int kCheckBackgroundFocus = 1157;
constexpr int kButtonCaptureTargetWindow = 1153;
constexpr int kEditTargetWindow = 1154;
constexpr int kButtonTestClick = 1155;
constexpr int kButtonCaptureSingle = 1201;
constexpr int kButtonStart = 1202;
constexpr int kButtonStop = 1203;
constexpr int kButtonLoadExample = 1204;
constexpr int kButtonCaptureMacro = 1205;
constexpr int kButtonSavePoint = 1206;
constexpr int kButtonInsertSavedPoint = 1207;
constexpr int kButtonSaveProfile = 1208;
constexpr int kButtonLoadProfile = 1209;
constexpr int kEditMacro = 1301;
constexpr int kLabelStatus = 1302;
constexpr int kComboMacroButton = 1303;
constexpr int kEditMacroCount = 1304;
constexpr int kEditMacroDelay = 1305;
constexpr int kListSavedPoints = 1306;
constexpr int kEditScreenBounds = 1307;
constexpr int kEditLog = 1308;
constexpr int kButtonClearLog = 1309;
constexpr int kHotkeyToggle = 1;
constexpr int kHotkeyCaptureSingle = 2;
constexpr int kHotkeyCaptureMacro = 3;
constexpr int kHotkeyCaptureTarget = 4;

enum class ClickButton { Left, Right, Middle };
enum class OutputMode { Cursor, BackgroundWindow, BackgroundWindowPlus };

struct ClickStep
{
    LONG x = 0;
    LONG y = 0;
    ClickButton button = ClickButton::Left;
    int count = 1;
    int delayMs = 50;
};

struct TargetWindow
{
    HWND handle = nullptr;
    std::wstring title;
    std::wstring className;
};

struct RunConfig
{
    OutputMode outputMode = OutputMode::Cursor;
    TargetWindow targetWindow;
};

struct SingleClickConfig
{
    LONG x = 0;
    LONG y = 0;
    ClickButton button = ClickButton::Left;
    int repeats = 0;
    int intervalMs = 10;
    RunConfig runConfig;
};

struct MacroConfig
{
    std::vector<ClickStep> steps;
    RunConfig runConfig;
};

struct SavedPoint
{
    std::wstring name;
    LONG x = 0;
    LONG y = 0;
};

struct AppState
{
    HWND hwnd = nullptr;
    HWND statusLabel = nullptr;
    HWND radioSingle = nullptr;
    HWND radioMacro = nullptr;
    HWND radioOutputCursor = nullptr;
    HWND radioOutputBackground = nullptr;
    HWND radioOutputBackgroundPlus = nullptr;
    HWND checkBackgroundFocus = nullptr;
    HWND buttonCaptureTargetWindow = nullptr;
    HWND buttonTestClick = nullptr;
    HWND buttonCaptureSingle = nullptr;
    HWND buttonLoadExample = nullptr;
    HWND buttonCaptureMacro = nullptr;
    HWND buttonSavePoint = nullptr;
    HWND buttonInsertSavedPoint = nullptr;
    HWND buttonSaveProfile = nullptr;
    HWND buttonLoadProfile = nullptr;
    HWND editTargetWindow = nullptr;
    HWND editSingleX = nullptr;
    HWND editSingleY = nullptr;
    HWND comboSingleButton = nullptr;
    HWND editSingleRepeats = nullptr;
    HWND editSingleInterval = nullptr;
    HWND editMacro = nullptr;
    HWND comboMacroButton = nullptr;
    HWND editMacroCount = nullptr;
    HWND editMacroDelay = nullptr;
    HWND listSavedPoints = nullptr;
    HWND editScreenBounds = nullptr;
    HWND editLog = nullptr;
    HWND buttonClearLog = nullptr;
    std::atomic<bool> running = false;
    std::jthread worker;
    std::vector<SavedPoint> savedPoints;
    int savedPointCounter = 1;
    TargetWindow selectedTargetWindow;
};

AppState gApp;

std::wstring Trim(const std::wstring& value)
{
    const auto start = value.find_first_not_of(L" \t\r\n");
    if (start == std::wstring::npos)
    {
        return L"";
    }
    const auto end = value.find_last_not_of(L" \t\r\n");
    return value.substr(start, end - start + 1);
}

std::wstring ClickButtonToToken(ClickButton button)
{
    switch (button)
    {
    case ClickButton::Right:
        return L"right";
    case ClickButton::Middle:
        return L"middle";
    case ClickButton::Left:
    default:
        return L"left";
    }
}

std::wstring OutputModeToToken(OutputMode mode)
{
    switch (mode)
    {
    case OutputMode::BackgroundWindow:
        return L"background";
    case OutputMode::BackgroundWindowPlus:
        return L"background_plus";
    case OutputMode::Cursor:
    default:
        return L"cursor";
    }
}

std::wstring OutputModeToLabel(OutputMode mode)
{
    switch (mode)
    {
    case OutputMode::BackgroundWindow:
        return L"Fenetre ciblee";
    case OutputMode::BackgroundWindowPlus:
        return L"Fenetre ciblee +";
    case OutputMode::Cursor:
    default:
        return L"Curseur reel";
    }
}

std::wstring EscapeText(const std::wstring& text)
{
    std::wstring result;
    for (wchar_t ch : text)
    {
        switch (ch)
        {
        case L'\\': result += L"\\\\"; break;
        case L'\n': result += L"\\n"; break;
        case L'\r': result += L"\\r"; break;
        default: result += ch; break;
        }
    }
    return result;
}

std::wstring UnescapeText(const std::wstring& text)
{
    std::wstring result;
    for (size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == L'\\' && i + 1 < text.size())
        {
            switch (text[i + 1])
            {
            case L'n': result += L'\n'; ++i; continue;
            case L'r': result += L'\r'; ++i; continue;
            case L'\\': result += L'\\'; ++i; continue;
            default: break;
            }
        }
        result += text[i];
    }
    return result;
}

std::wstring GetWindowTextString(HWND hwnd)
{
    const int length = GetWindowTextLengthW(hwnd);
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(hwnd, text.data(), length + 1);
    text.resize(length);
    return text;
}

std::wstring GetClassNameString(HWND hwnd)
{
    wchar_t buffer[256] = {};
    GetClassNameW(hwnd, buffer, static_cast<int>(std::size(buffer)));
    return buffer;
}

std::wstring MakeTimestamp()
{
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t buffer[32] = {};
    swprintf_s(buffer, L"[%02u:%02u:%02u]", st.wHour, st.wMinute, st.wSecond);
    return buffer;
}

void AppendLogLine(const std::wstring& text)
{
    if (gApp.editLog == nullptr)
    {
        return;
    }

    std::wstring current = GetWindowTextString(gApp.editLog);
    if (!current.empty())
    {
        current += L"\r\n";
    }
    current += MakeTimestamp() + L" " + text;
    SetWindowTextW(gApp.editLog, current.c_str());
    SendMessageW(gApp.editLog, EM_SETSEL, static_cast<WPARAM>(current.size()), static_cast<LPARAM>(current.size()));
    SendMessageW(gApp.editLog, EM_SCROLLCARET, 0, 0);
}

void PostLogLine(const std::wstring& text)
{
    auto* heapText = new std::wstring(text);
    PostMessageW(gApp.hwnd, WM_APP + 10, reinterpret_cast<WPARAM>(heapText), 0);
}

void SetStatus(const std::wstring& text)
{
    if (gApp.statusLabel != nullptr)
    {
        SetWindowTextW(gApp.statusLabel, text.c_str());
    }
}

void SetEnabled(HWND hwnd, bool enabled)
{
    if (hwnd != nullptr)
    {
        EnableWindow(hwnd, enabled ? TRUE : FALSE);
    }
}

bool TryParseInt(HWND hwnd, int minValue, int maxValue, int& output)
{
    const std::wstring text = Trim(GetWindowTextString(hwnd));
    if (text.empty())
    {
        return false;
    }
    try
    {
        size_t consumed = 0;
        const long long value = std::stoll(text, &consumed, 10);
        if (consumed != text.size() || value < minValue || value > maxValue)
        {
            return false;
        }
        output = static_cast<int>(value);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool GetCursorScreenPoint(POINT& point)
{
    return GetCursorPos(&point) == TRUE;
}

ClickButton GetSelectedButton(HWND combo)
{
    switch (static_cast<int>(SendMessageW(combo, CB_GETCURSEL, 0, 0)))
    {
    case 1: return ClickButton::Right;
    case 2: return ClickButton::Middle;
    default: return ClickButton::Left;
    }
}

OutputMode GetSelectedOutputMode()
{
    return SendMessageW(gApp.radioOutputBackground, BM_GETCHECK, 0, 0) == BST_CHECKED
        ? OutputMode::BackgroundWindow
        : OutputMode::Cursor;
}

bool IsSingleModeSelected()
{
    return SendMessageW(gApp.radioSingle, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

void RefreshSavedPointsList()
{
    SendMessageW(gApp.listSavedPoints, LB_RESETCONTENT, 0, 0);
    for (const SavedPoint& point : gApp.savedPoints)
    {
        const std::wstring label = point.name + L" : (" + std::to_wstring(point.x) + L", " + std::to_wstring(point.y) + L")";
        SendMessageW(gApp.listSavedPoints, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
    }
}

void RefreshTargetWindowDisplay()
{
    std::wstring text;
    if (gApp.selectedTargetWindow.handle == nullptr)
    {
        if (gApp.selectedTargetWindow.title.empty() && gApp.selectedTargetWindow.className.empty())
        {
            text = L"Aucune fenetre cible. Capture avec F9 ou le bouton.";
        }
        else
        {
            text = L"Fenetre du profil chargee, mais handle non restaurable. Recapture avec F9.\r\n";
            text += L"Titre=" + (gApp.selectedTargetWindow.title.empty() ? L"<sans titre>" : gApp.selectedTargetWindow.title);
            text += L"\r\nClasse=" + gApp.selectedTargetWindow.className;
        }
    }
    else
    {
        text = L"Handle=" + std::to_wstring(reinterpret_cast<UINT_PTR>(gApp.selectedTargetWindow.handle));
        text += L"\r\nTitre=" + (gApp.selectedTargetWindow.title.empty() ? L"<sans titre>" : gApp.selectedTargetWindow.title);
        text += L"\r\nClasse=" + gApp.selectedTargetWindow.className;
    }
    SetWindowTextW(gApp.editTargetWindow, text.c_str());
}

bool IsUsableTargetWindow(const TargetWindow& target)
{
    return target.handle != nullptr && IsWindow(target.handle) == TRUE;
}

void UpdateUiState()
{
    const bool singleMode = IsSingleModeSelected();
    const bool backgroundMode = GetSelectedOutputMode() != OutputMode::Cursor;
    SetEnabled(gApp.editSingleX, singleMode);
    SetEnabled(gApp.editSingleY, singleMode);
    SetEnabled(gApp.comboSingleButton, singleMode);
    SetEnabled(gApp.editSingleRepeats, singleMode);
    SetEnabled(gApp.editSingleInterval, singleMode);
    SetEnabled(gApp.buttonCaptureSingle, singleMode);
    SetEnabled(gApp.comboMacroButton, !singleMode);
    SetEnabled(gApp.editMacroCount, !singleMode);
    SetEnabled(gApp.editMacroDelay, !singleMode);
    SetEnabled(gApp.buttonCaptureMacro, !singleMode);
    SetEnabled(gApp.buttonLoadExample, !singleMode);
    SetEnabled(gApp.buttonInsertSavedPoint, !singleMode);
    SetEnabled(gApp.editMacro, !singleMode);
    SetEnabled(gApp.buttonCaptureTargetWindow, backgroundMode);
    SetEnabled(gApp.buttonTestClick, backgroundMode);
}

std::optional<RunConfig> ReadRunConfig(std::wstring& error)
{
    RunConfig config;
    config.outputMode = GetSelectedOutputMode();
    config.targetWindow = gApp.selectedTargetWindow;
    if (config.outputMode != OutputMode::Cursor && !IsUsableTargetWindow(config.targetWindow))
    {
        error = L"Capture d'abord une fenetre cible pour le mode arriere-plan.";
        return std::nullopt;
    }
    return config;
}

std::optional<SingleClickConfig> ReadSingleConfig(std::wstring& error)
{
    SingleClickConfig config;
    int value = 0;
    if (!TryParseInt(gApp.editSingleX, -100000, 100000, value))
    {
        error = L"Coordonnee X invalide.";
        return std::nullopt;
    }
    config.x = value;
    if (!TryParseInt(gApp.editSingleY, -100000, 100000, value))
    {
        error = L"Coordonnee Y invalide.";
        return std::nullopt;
    }
    config.y = value;
    config.button = GetSelectedButton(gApp.comboSingleButton);
    if (!TryParseInt(gApp.editSingleRepeats, 0, 1000000, value))
    {
        error = L"Repetitions invalides. Utilise 0 pour infini.";
        return std::nullopt;
    }
    config.repeats = value;
    if (!TryParseInt(gApp.editSingleInterval, 1, 60000, value))
    {
        error = L"Intervalle invalide.";
        return std::nullopt;
    }
    config.intervalMs = value;
    const auto runConfig = ReadRunConfig(error);
    if (!runConfig.has_value())
    {
        return std::nullopt;
    }
    config.runConfig = *runConfig;
    return config;
}

std::optional<ClickButton> ParseButton(const std::wstring& value)
{
    if (value == L"left" || value == L"gauche") return ClickButton::Left;
    if (value == L"right" || value == L"droit") return ClickButton::Right;
    if (value == L"middle" || value == L"milieu") return ClickButton::Middle;
    return std::nullopt;
}

std::optional<ClickStep> ReadMacroDefaults(std::wstring& error)
{
    ClickStep step;
    int value = 0;
    step.button = GetSelectedButton(gApp.comboMacroButton);
    if (!TryParseInt(gApp.editMacroCount, 1, 1000000, value))
    {
        error = L"Count macro invalide.";
        return std::nullopt;
    }
    step.count = value;
    if (!TryParseInt(gApp.editMacroDelay, 1, 60000, value))
    {
        error = L"Delay macro invalide.";
        return std::nullopt;
    }
    step.delayMs = value;
    return step;
}

std::optional<MacroConfig> ReadMacroConfig(std::wstring& error)
{
    MacroConfig config;
    std::wistringstream allLines(GetWindowTextString(gApp.editMacro));
    std::wstring line;
    int lineNumber = 0;
    while (std::getline(allLines, line))
    {
        ++lineNumber;
        const std::wstring trimmed = Trim(line);
        if (trimmed.empty())
        {
            continue;
        }
        std::wstringstream parser(trimmed);
        std::wstring parts[5];
        for (int i = 0; i < 5; ++i)
        {
            if (!std::getline(parser, parts[i], L','))
            {
                error = L"Ligne " + std::to_wstring(lineNumber) + L" incomplete. Format: x,y,button,count,delayMs";
                return std::nullopt;
            }
            parts[i] = Trim(parts[i]);
        }
        ClickStep step;
        try
        {
            step.x = std::stoi(parts[0]);
            step.y = std::stoi(parts[1]);
            const auto button = ParseButton(parts[2]);
            if (!button.has_value())
            {
                error = L"Bouton invalide a la ligne " + std::to_wstring(lineNumber) + L".";
                return std::nullopt;
            }
            step.button = *button;
            step.count = std::stoi(parts[3]);
            step.delayMs = std::stoi(parts[4]);
        }
        catch (...)
        {
            error = L"Valeur invalide a la ligne " + std::to_wstring(lineNumber) + L".";
            return std::nullopt;
        }
        if (step.count < 1 || step.delayMs < 1)
        {
            error = L"Valeurs hors plage a la ligne " + std::to_wstring(lineNumber) + L".";
            return std::nullopt;
        }
        config.steps.push_back(step);
    }
    if (config.steps.empty())
    {
        error = L"La macro est vide.";
        return std::nullopt;
    }
    const auto runConfig = ReadRunConfig(error);
    if (!runConfig.has_value())
    {
        return std::nullopt;
    }
    config.runConfig = *runConfig;
    return config;
}
void SendForegroundClick(LONG x, LONG y, ClickButton button)
{
    SetCursorPos(x, y);
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_MOUSE;
    inputs[1].type = INPUT_MOUSE;
    switch (button)
    {
    case ClickButton::Right:
        inputs[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
        inputs[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
        break;
    case ClickButton::Middle:
        inputs[0].mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
        inputs[1].mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
        break;
    case ClickButton::Left:
    default:
        inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        break;
    }
    SendInput(2, inputs, sizeof(INPUT));
}

HWND ResolveBestChildWindow(HWND targetRoot, POINT screenPoint)
{
    if (!IsWindow(targetRoot))
    {
        return nullptr;
    }
    POINT clientPoint = screenPoint;
    if (!ScreenToClient(targetRoot, &clientPoint))
    {
        return targetRoot;
    }
    HWND child = RealChildWindowFromPoint(targetRoot, clientPoint);
    return child != nullptr ? child : targetRoot;
}

void BuildMouseMessages(ClickButton button, UINT& downMessage, UINT& upMessage, WPARAM& downFlags)
{
    downMessage = WM_LBUTTONDOWN;
    upMessage = WM_LBUTTONUP;
    downFlags = MK_LBUTTON;
    switch (button)
    {
    case ClickButton::Right:
        downMessage = WM_RBUTTONDOWN;
        upMessage = WM_RBUTTONUP;
        downFlags = MK_RBUTTON;
        break;
    case ClickButton::Middle:
        downMessage = WM_MBUTTONDOWN;
        upMessage = WM_MBUTTONUP;
        downFlags = MK_MBUTTON;
        break;
    case ClickButton::Left:
    default:
        break;
    }
}

bool SendBackgroundClick(const TargetWindow& target, LONG x, LONG y, ClickButton button)
{
    if (!IsUsableTargetWindow(target))
    {
        return false;
    }
    POINT screenPoint{ x, y };
    HWND recipient = ResolveBestChildWindow(target.handle, screenPoint);
    if (recipient == nullptr || !IsWindow(recipient))
    {
        recipient = target.handle;
    }
    POINT clientPoint = screenPoint;
    if (!ScreenToClient(recipient, &clientPoint))
    {
        return false;
    }
    const LPARAM lParam = MAKELPARAM(static_cast<short>(clientPoint.x), static_cast<short>(clientPoint.y));
    UINT downMessage = 0;
    UINT upMessage = 0;
    WPARAM downFlags = 0;
    BuildMouseMessages(button, downMessage, upMessage, downFlags);
    PostMessageW(recipient, WM_MOUSEMOVE, 0, lParam);
    PostMessageW(recipient, downMessage, downFlags, lParam);
    PostMessageW(recipient, upMessage, 0, lParam);
    return true;
}

bool SendBackgroundClickEnhanced(const TargetWindow& target, LONG x, LONG y, ClickButton button, bool focusTemporarily)
{
    if (!IsUsableTargetWindow(target))
    {
        return false;
    }
    POINT screenPoint{ x, y };
    HWND recipient = ResolveBestChildWindow(target.handle, screenPoint);
    if (recipient == nullptr || !IsWindow(recipient))
    {
        recipient = target.handle;
    }
    POINT clientPoint = screenPoint;
    if (!ScreenToClient(recipient, &clientPoint))
    {
        return false;
    }
    const LPARAM lParam = MAKELPARAM(static_cast<short>(clientPoint.x), static_cast<short>(clientPoint.y));
    UINT downMessage = 0;
    UINT upMessage = 0;
    WPARAM downFlags = 0;
    BuildMouseMessages(button, downMessage, upMessage, downFlags);

    if (focusTemporarily)
    {
        AllowSetForegroundWindow(ASFW_ANY);
        SetForegroundWindow(target.handle);
        SetActiveWindow(target.handle);
        SetFocus(target.handle);
    }

    SendMessageW(recipient, WM_MOUSEACTIVATE, reinterpret_cast<WPARAM>(target.handle), MAKELPARAM(HTCLIENT, downMessage));
    SendMessageW(recipient, WM_SETCURSOR, reinterpret_cast<WPARAM>(recipient), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
    SendMessageW(recipient, WM_MOUSEMOVE, 0, lParam);
    SendMessageW(recipient, downMessage, downFlags, lParam);
    SendMessageW(recipient, upMessage, 0, lParam);
    return true;
}

bool ExecuteClick(const RunConfig& runConfig, LONG x, LONG y, ClickButton button)
{
    switch (runConfig.outputMode)
    {
    case OutputMode::BackgroundWindow:
        return SendBackgroundClick(runConfig.targetWindow, x, y, button);
    case OutputMode::BackgroundWindowPlus:
        return SendBackgroundClickEnhanced(runConfig.targetWindow, x, y, button,
            SendMessageW(gApp.checkBackgroundFocus, BM_GETCHECK, 0, 0) == BST_CHECKED);
    case OutputMode::Cursor:
    default:
        SendForegroundClick(x, y, button);
        return true;
    }
}

void CaptureCurrentCursorToSingleFields()
{
    POINT point{};
    if (GetCursorScreenPoint(point))
    {
        SetWindowTextW(gApp.editSingleX, std::to_wstring(point.x).c_str());
        SetWindowTextW(gApp.editSingleY, std::to_wstring(point.y).c_str());
        SetStatus(L"Position capturee pour le point fixe.");
        AppendLogLine(L"Capture point fixe : (" + std::to_wstring(point.x) + L", " + std::to_wstring(point.y) + L")");
    }
}

void AppendMacroStep(const ClickStep& step)
{
    std::wstring macroText = GetWindowTextString(gApp.editMacro);
    if (!macroText.empty() && macroText.back() != L'\n')
    {
        macroText += L"\r\n";
    }
    macroText += std::to_wstring(step.x) + L"," + std::to_wstring(step.y) + L"," + ClickButtonToToken(step.button) +
        L"," + std::to_wstring(step.count) + L"," + std::to_wstring(step.delayMs);
    SetWindowTextW(gApp.editMacro, macroText.c_str());
}

void CaptureCurrentCursorToMacro()
{
    std::wstring error;
    const auto defaults = ReadMacroDefaults(error);
    if (!defaults.has_value())
    {
        SetStatus(error);
        return;
    }
    POINT point{};
    if (!GetCursorScreenPoint(point))
    {
        SetStatus(L"Impossible de capturer la souris.");
        return;
    }
    ClickStep step = *defaults;
    step.x = point.x;
    step.y = point.y;
    AppendMacroStep(step);
    SetStatus(L"Etape macro ajoutee depuis la souris.");
    AppendLogLine(L"Ajout macro : (" + std::to_wstring(step.x) + L", " + std::to_wstring(step.y) + L") " + ClickButtonToToken(step.button) + L", count=" + std::to_wstring(step.count) + L", delay=" + std::to_wstring(step.delayMs));
}

void SaveCurrentCursorPoint()
{
    POINT point{};
    if (!GetCursorScreenPoint(point))
    {
        SetStatus(L"Impossible de capturer la souris.");
        return;
    }
    SavedPoint savedPoint;
    savedPoint.name = L"Point " + std::to_wstring(gApp.savedPointCounter++);
    savedPoint.x = point.x;
    savedPoint.y = point.y;
    gApp.savedPoints.push_back(savedPoint);
    RefreshSavedPointsList();
    SendMessageW(gApp.listSavedPoints, LB_SETCURSEL, static_cast<WPARAM>(gApp.savedPoints.size() - 1), 0);
    SetStatus(L"Point sauvegarde depuis la position actuelle.");
    AppendLogLine(L"Point sauvegarde : " + savedPoint.name + L" -> (" + std::to_wstring(savedPoint.x) + L", " + std::to_wstring(savedPoint.y) + L")");
}

void InsertSelectedSavedPointIntoMacro()
{
    const int selectedIndex = static_cast<int>(SendMessageW(gApp.listSavedPoints, LB_GETCURSEL, 0, 0));
    if (selectedIndex == LB_ERR || selectedIndex < 0 || selectedIndex >= static_cast<int>(gApp.savedPoints.size()))
    {
        SetStatus(L"Selectionne d'abord un point sauvegarde.");
        return;
    }
    std::wstring error;
    const auto defaults = ReadMacroDefaults(error);
    if (!defaults.has_value())
    {
        SetStatus(error);
        return;
    }
    ClickStep step = *defaults;
    step.x = gApp.savedPoints[selectedIndex].x;
    step.y = gApp.savedPoints[selectedIndex].y;
    AppendMacroStep(step);
    SetStatus(L"Point sauvegarde insere dans la macro.");
    AppendLogLine(L"Insertion dans macro depuis point sauvegarde : " + gApp.savedPoints[selectedIndex].name);
}

void CaptureTargetWindowFromCursor()
{
    POINT point{};
    if (!GetCursorScreenPoint(point))
    {
        SetStatus(L"Impossible de lire la position de la souris.");
        return;
    }
    HWND hovered = WindowFromPoint(point);
    if (hovered == nullptr)
    {
        SetStatus(L"Aucune fenetre detectee sous la souris.");
        return;
    }
    HWND root = GetAncestor(hovered, GA_ROOT);
    if (root == nullptr)
    {
        root = hovered;
    }
    gApp.selectedTargetWindow.handle = root;
    gApp.selectedTargetWindow.title = GetWindowTextString(root);
    gApp.selectedTargetWindow.className = GetClassNameString(root);
    RefreshTargetWindowDisplay();
    SetStatus(L"Fenetre cible capturee. Le mode arriere-plan va essayer de cliquer dedans sans bouger la souris.");
    AppendLogLine(L"Fenetre cible capturee : " + (gApp.selectedTargetWindow.title.empty() ? gApp.selectedTargetWindow.className : gApp.selectedTargetWindow.title));
}

BOOL CALLBACK MonitorEnumProc(HMONITOR monitor, HDC, LPRECT, LPARAM lParam)
{
    auto* text = reinterpret_cast<std::wstring*>(lParam);
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info))
    {
        return TRUE;
    }
    const RECT& area = info.rcMonitor;
    const RECT& work = info.rcWork;
    *text += info.szDevice;
    *text += (info.dwFlags & MONITORINFOF_PRIMARY) ? L" [principal]" : L"";
    *text += L"\r\n";
    *text += L"  Monitor : left=" + std::to_wstring(area.left) + L", top=" + std::to_wstring(area.top) +
        L", right=" + std::to_wstring(area.right) + L", bottom=" + std::to_wstring(area.bottom) + L"\r\n";
    *text += L"  Work    : left=" + std::to_wstring(work.left) + L", top=" + std::to_wstring(work.top) +
        L", right=" + std::to_wstring(work.right) + L", bottom=" + std::to_wstring(work.bottom) + L"\r\n\r\n";
    return TRUE;
}

void RefreshScreenBounds()
{
    std::wstring bounds;
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, reinterpret_cast<LPARAM>(&bounds));
    if (bounds.empty())
    {
        bounds = L"Impossible de lire les ecrans.";
    }
    SetWindowTextW(gApp.editScreenBounds, bounds.c_str());
}

std::optional<std::wstring> ChooseProfilePath(bool saveMode)
{
    wchar_t fileBuffer[MAX_PATH] = L"autoclicker-profile.ini";
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = gApp.hwnd;
    dialog.lpstrFilter = L"Profil AutoClicker (*.ini)\0*.ini\0Tous les fichiers (*.*)\0*.*\0";
    dialog.lpstrFile = fileBuffer;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrDefExt = L"ini";
    dialog.Flags = OFN_PATHMUSTEXIST | OFN_EXPLORER;
    if (saveMode)
    {
        dialog.Flags |= OFN_OVERWRITEPROMPT;
        if (!GetSaveFileNameW(&dialog)) return std::nullopt;
    }
    else
    {
        dialog.Flags |= OFN_FILEMUSTEXIST;
        if (!GetOpenFileNameW(&dialog)) return std::nullopt;
    }
    return std::wstring(fileBuffer);
}

void WriteProfileValue(const std::wstring& section, const std::wstring& key, const std::wstring& value, const std::wstring& path)
{
    WritePrivateProfileStringW(section.c_str(), key.c_str(), value.c_str(), path.c_str());
}

std::wstring ReadProfileValue(const std::wstring& section, const std::wstring& key, const std::wstring& fallback, const std::wstring& path)
{
    std::wstring buffer(65535, L'\0');
    GetPrivateProfileStringW(section.c_str(), key.c_str(), fallback.c_str(), buffer.data(), static_cast<DWORD>(buffer.size()), path.c_str());
    buffer.resize(wcslen(buffer.c_str()));
    return buffer;
}
void SaveProfile()
{
    const auto path = ChooseProfilePath(true);
    if (!path.has_value())
    {
        SetStatus(L"Sauvegarde du profil annulee.");
        return;
    }
    WriteProfileValue(L"General", L"ActionMode", IsSingleModeSelected() ? L"single" : L"macro", *path);
    WriteProfileValue(L"General", L"OutputMode", OutputModeToToken(GetSelectedOutputMode()), *path);
    WriteProfileValue(L"General", L"TargetTitle", EscapeText(gApp.selectedTargetWindow.title), *path);
    WriteProfileValue(L"General", L"TargetClass", EscapeText(gApp.selectedTargetWindow.className), *path);
    WriteProfileValue(L"Single", L"X", GetWindowTextString(gApp.editSingleX), *path);
    WriteProfileValue(L"Single", L"Y", GetWindowTextString(gApp.editSingleY), *path);
    WriteProfileValue(L"Single", L"ButtonIndex", std::to_wstring(static_cast<int>(SendMessageW(gApp.comboSingleButton, CB_GETCURSEL, 0, 0))), *path);
    WriteProfileValue(L"Single", L"Repeats", GetWindowTextString(gApp.editSingleRepeats), *path);
    WriteProfileValue(L"Single", L"IntervalMs", GetWindowTextString(gApp.editSingleInterval), *path);
    WriteProfileValue(L"Macro", L"DefaultButtonIndex", std::to_wstring(static_cast<int>(SendMessageW(gApp.comboMacroButton, CB_GETCURSEL, 0, 0))), *path);
    WriteProfileValue(L"Macro", L"DefaultCount", GetWindowTextString(gApp.editMacroCount), *path);
    WriteProfileValue(L"Macro", L"DefaultDelay", GetWindowTextString(gApp.editMacroDelay), *path);
    WriteProfileValue(L"Macro", L"Steps", EscapeText(GetWindowTextString(gApp.editMacro)), *path);
    WriteProfileValue(L"Points", L"Count", std::to_wstring(gApp.savedPoints.size()), *path);
    for (size_t i = 0; i < gApp.savedPoints.size(); ++i)
    {
        const std::wstring section = L"Point" + std::to_wstring(i + 1);
        WriteProfileValue(section, L"Name", EscapeText(gApp.savedPoints[i].name), *path);
        WriteProfileValue(section, L"X", std::to_wstring(gApp.savedPoints[i].x), *path);
        WriteProfileValue(section, L"Y", std::to_wstring(gApp.savedPoints[i].y), *path);
    }
    SetStatus(L"Profil sauvegarde.");
    AppendLogLine(L"Profil sauvegarde.");
}

void LoadProfile()
{
    const auto path = ChooseProfilePath(false);
    if (!path.has_value())
    {
        SetStatus(L"Chargement du profil annule.");
        return;
    }
    const std::wstring actionMode = ReadProfileValue(L"General", L"ActionMode", L"single", *path);
    const std::wstring outputMode = ReadProfileValue(L"General", L"OutputMode", L"cursor", *path);
    SendMessageW(gApp.radioSingle, BM_SETCHECK, actionMode == L"single" ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(gApp.radioMacro, BM_SETCHECK, actionMode == L"macro" ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(gApp.radioOutputCursor, BM_SETCHECK, outputMode == L"cursor" ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(gApp.radioOutputBackground, BM_SETCHECK, outputMode == L"background" ? BST_CHECKED : BST_UNCHECKED, 0);
    SetWindowTextW(gApp.editSingleX, ReadProfileValue(L"Single", L"X", L"0", *path).c_str());
    SetWindowTextW(gApp.editSingleY, ReadProfileValue(L"Single", L"Y", L"0", *path).c_str());
    SetWindowTextW(gApp.editSingleRepeats, ReadProfileValue(L"Single", L"Repeats", L"0", *path).c_str());
    SetWindowTextW(gApp.editSingleInterval, ReadProfileValue(L"Single", L"IntervalMs", L"10", *path).c_str());
    int comboIndex = _wtoi(ReadProfileValue(L"Single", L"ButtonIndex", L"0", *path).c_str());
    SendMessageW(gApp.comboSingleButton, CB_SETCURSEL, comboIndex, 0);
    comboIndex = _wtoi(ReadProfileValue(L"Macro", L"DefaultButtonIndex", L"0", *path).c_str());
    SendMessageW(gApp.comboMacroButton, CB_SETCURSEL, comboIndex, 0);
    SetWindowTextW(gApp.editMacroCount, ReadProfileValue(L"Macro", L"DefaultCount", L"1", *path).c_str());
    SetWindowTextW(gApp.editMacroDelay, ReadProfileValue(L"Macro", L"DefaultDelay", L"50", *path).c_str());
    SetWindowTextW(gApp.editMacro, UnescapeText(ReadProfileValue(L"Macro", L"Steps", L"", *path)).c_str());
    gApp.savedPoints.clear();
    const int pointsCount = _wtoi(ReadProfileValue(L"Points", L"Count", L"0", *path).c_str());
    for (int i = 0; i < pointsCount; ++i)
    {
        const std::wstring section = L"Point" + std::to_wstring(i + 1);
        SavedPoint point;
        point.name = UnescapeText(ReadProfileValue(section, L"Name", L"Point " + std::to_wstring(i + 1), *path));
        point.x = _wtoi(ReadProfileValue(section, L"X", L"0", *path).c_str());
        point.y = _wtoi(ReadProfileValue(section, L"Y", L"0", *path).c_str());
        gApp.savedPoints.push_back(point);
    }
    gApp.savedPointCounter = static_cast<int>(gApp.savedPoints.size()) + 1;
    RefreshSavedPointsList();
    gApp.selectedTargetWindow.handle = nullptr;
    gApp.selectedTargetWindow.title = UnescapeText(ReadProfileValue(L"General", L"TargetTitle", L"", *path));
    gApp.selectedTargetWindow.className = UnescapeText(ReadProfileValue(L"General", L"TargetClass", L"", *path));
    RefreshTargetWindowDisplay();
    UpdateUiState();
    SetStatus(L"Profil charge. Si tu utilises un mode fenetre ciblee, recapture la fenetre avec F9.");
    AppendLogLine(L"Profil charge.");
}

void TestCurrentClick()
{
    if (GetSelectedOutputMode() == OutputMode::Cursor)
    {
        SetStatus(L"Le test rapide sert au mode fenetre ciblee. Selectionne-le d'abord.");
        return;
    }
    std::wstring error;
    if (IsSingleModeSelected())
    {
        const auto config = ReadSingleConfig(error);
        if (!config.has_value())
        {
            SetStatus(error);
            return;
        }
        SetStatus(ExecuteClick(config->runConfig, config->x, config->y, config->button)
            ? L"Test d'un clic envoye sur la fenetre ciblee."
            : L"Echec du clic test sur la fenetre ciblee.");
        return;
    }
    const auto config = ReadMacroConfig(error);
    if (!config.has_value())
    {
        SetStatus(error);
        return;
    }
    const ClickStep& first = config->steps.front();
    SetStatus(ExecuteClick(config->runConfig, first.x, first.y, first.button)
        ? L"Test du premier clic de la macro envoye sur la fenetre ciblee."
        : L"Echec du clic test sur la fenetre ciblee.");
}

void StopClicking()
{
    if (!gApp.running.exchange(false))
    {
        SetStatus(L"Arret: aucun clic en cours.");
        return;
    }
    if (gApp.worker.joinable())
    {
        gApp.worker.request_stop();
        gApp.worker.join();
    }
    SetStatus(L"Autoclicker arrete.");
    AppendLogLine(L"Autoclicker arrete.");
}

void StartSingle(const SingleClickConfig& config)
{
    gApp.running = true;
    gApp.worker = std::jthread([config](std::stop_token stopToken) {
        int executed = 0;
        while (!stopToken.stop_requested() && gApp.running.load())
        {
            if (!ExecuteClick(config.runConfig, config.x, config.y, config.button))
            {
                gApp.running = false;
                PostMessageW(gApp.hwnd, WM_APP + 2, 0, 0);
                return;
            }
            ++executed;
            if (config.repeats > 0 && executed >= config.repeats)
            {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(config.intervalMs));
        }
        gApp.running = false;
        PostMessageW(gApp.hwnd, WM_APP + 1, 0, 0);
    });
}

void StartMacro(const MacroConfig& config)
{
    gApp.running = true;
    gApp.worker = std::jthread([config](std::stop_token stopToken) {
        while (!stopToken.stop_requested() && gApp.running.load())
        {
            for (const ClickStep& step : config.steps)
            {
                for (int i = 0; i < step.count; ++i)
                {
                    if (stopToken.stop_requested() || !gApp.running.load())
                    {
                        gApp.running = false;
                        PostMessageW(gApp.hwnd, WM_APP + 1, 0, 0);
                        return;
                    }
                    if (!ExecuteClick(config.runConfig, step.x, step.y, step.button))
                    {
                        gApp.running = false;
                        PostMessageW(gApp.hwnd, WM_APP + 2, 0, 0);
                        return;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(step.delayMs));
                }
            }
        }
        gApp.running = false;
        PostMessageW(gApp.hwnd, WM_APP + 1, 0, 0);
    });
}

void StartClicking()
{
    if (gApp.running.load())
    {
        SetStatus(L"L'autoclicker tourne deja.");
        AppendLogLine(L"Demarrage ignore : deja en cours.");
        return;
    }
    std::wstring error;
    if (IsSingleModeSelected())
    {
        const auto config = ReadSingleConfig(error);
        if (!config.has_value())
        {
            SetStatus(error);
            return;
        }
        StartSingle(*config);
        AppendLogLine(L"Demarrage point fixe en mode " + OutputModeToLabel(config->runConfig.outputMode) + L" sur (" + std::to_wstring(config->x) + L", " + std::to_wstring(config->y) + L")");
        SetStatus(config->runConfig.outputMode == OutputMode::BackgroundWindow
            ? L"Mode point fixe demarre en arriere-plan. Compatibilite selon l'application cible."
            : L"Mode point fixe demarre. F6 pour arreter.");
        return;
    }
    const auto macro = ReadMacroConfig(error);
    if (!macro.has_value())
    {
        SetStatus(error);
        return;
    }
    StartMacro(*macro);
    AppendLogLine(L"Demarrage macro en mode " + OutputModeToLabel(macro->runConfig.outputMode) + L" avec " + std::to_wstring(macro->steps.size()) + L" etapes");
    SetStatus(macro->runConfig.outputMode == OutputMode::BackgroundWindow
        ? L"Mode macro demarre en arriere-plan. Compatibilite selon l'application cible."
        : L"Mode macro demarre. F6 pour arreter.");
}

void FillMacroExample()
{
    const wchar_t* example =
        L"-1200,500,left,10,10\r\n"
        L"-900,500,left,3,50\r\n"
        L"-900,620,right,1,200\r\n";
    SetWindowTextW(gApp.editMacro, example);
    SetStatus(L"Exemple de macro charge.");
}
void AddLabel(HWND parent, const wchar_t* text, int x, int y, int w, int h)
{
    CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, w, h, parent, nullptr, nullptr, nullptr);
}

HWND AddEdit(HWND parent, int id, const wchar_t* text, DWORD style, int x, int y, int w, int h)
{
    return CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
}

HWND AddButton(HWND parent, int id, const wchar_t* text, DWORD style, int x, int y, int w, int h)
{
    return CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
}

void AddGroupBox(HWND parent, const wchar_t* text, int x, int y, int w, int h)
{
    CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_GROUPBOX, x, y, w, h, parent, nullptr, nullptr, nullptr);
}

void BuildUi(HWND hwnd)
{
    HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    AddGroupBox(hwnd, L"Mode d'action", 12, 10, 260, 64);
    gApp.radioSingle = AddButton(hwnd, kModeSingle, L"Point fixe", BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP, 28, 36, 110, 24);
    gApp.radioMacro = AddButton(hwnd, kModeMacro, L"Macro", BS_AUTORADIOBUTTON, 150, 36, 100, 24);
    SendMessageW(gApp.radioSingle, BM_SETCHECK, BST_CHECKED, 0);
    AddGroupBox(hwnd, L"Mode d'envoi des clics", 286, 10, 304, 86);
    gApp.radioOutputCursor = AddButton(hwnd, kRadioOutputCursor, L"Curseur reel", BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP, 302, 32, 120, 24);
    gApp.radioOutputBackground = AddButton(hwnd, kRadioOutputBackground, L"Fenetre ciblee", BS_AUTORADIOBUTTON, 426, 32, 130, 24);
    gApp.radioOutputBackgroundPlus = AddButton(hwnd, kRadioOutputBackgroundPlus, L"Fenetre ciblee +", BS_AUTORADIOBUTTON, 302, 58, 145, 24);
    gApp.checkBackgroundFocus = AddButton(hwnd, kCheckBackgroundFocus, L"Focus temporaire", BS_AUTOCHECKBOX, 456, 58, 125, 24);
    SendMessageW(gApp.radioOutputCursor, BM_SETCHECK, BST_CHECKED, 0);
    AddGroupBox(hwnd, L"Fenetre cible capturee", 604, 10, 406, 132);
    gApp.buttonCaptureTargetWindow = AddButton(hwnd, kButtonCaptureTargetWindow, L"Capturer fenetre sous la souris (F9)", BS_PUSHBUTTON, 620, 34, 240, 30);
    gApp.buttonTestClick = AddButton(hwnd, kButtonTestClick, L"Tester un clic", BS_PUSHBUTTON, 870, 34, 120, 30);
    gApp.editTargetWindow = AddEdit(hwnd, kEditTargetWindow, L"", ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL, 620, 70, 370, 54);
    AddGroupBox(hwnd, L"Profils", 12, 84, 578, 58);
    gApp.buttonSaveProfile = AddButton(hwnd, kButtonSaveProfile, L"Sauvegarder un profil", BS_PUSHBUTTON, 28, 106, 180, 28);
    gApp.buttonLoadProfile = AddButton(hwnd, kButtonLoadProfile, L"Charger un profil", BS_PUSHBUTTON, 220, 106, 170, 28);
    AddLabel(hwnd, L"Les profils restaurent l'UI, la macro et les points. La fenetre cible doit etre recapturee apres chargement.", 404, 110, 180, 20);
    AddLabel(hwnd, L"Configuration point fixe", 20, 158, 220, 20);
    AddLabel(hwnd, L"X", 20, 188, 20, 20);
    gApp.editSingleX = AddEdit(hwnd, kEditSingleX, L"0", ES_AUTOHSCROLL, 45, 184, 80, 24);
    AddLabel(hwnd, L"Y", 140, 188, 20, 20);
    gApp.editSingleY = AddEdit(hwnd, kEditSingleY, L"0", ES_AUTOHSCROLL, 165, 184, 80, 24);
    AddLabel(hwnd, L"Bouton", 260, 188, 60, 20);
    gApp.comboSingleButton = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 320, 184, 120, 160, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kComboSingleButton)), nullptr, nullptr);
    SendMessageW(gApp.comboSingleButton, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Gauche"));
    SendMessageW(gApp.comboSingleButton, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Droit"));
    SendMessageW(gApp.comboSingleButton, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Milieu"));
    SendMessageW(gApp.comboSingleButton, CB_SETCURSEL, 0, 0);
    AddLabel(hwnd, L"Repetitions", 20, 222, 80, 20);
    gApp.editSingleRepeats = AddEdit(hwnd, kEditSingleRepeats, L"0", ES_AUTOHSCROLL, 105, 218, 80, 24);
    AddLabel(hwnd, L"Intervalle ms", 200, 222, 85, 20);
    gApp.editSingleInterval = AddEdit(hwnd, kEditSingleInterval, L"10", ES_AUTOHSCROLL, 290, 218, 80, 24);
    gApp.buttonCaptureSingle = AddButton(hwnd, kButtonCaptureSingle, L"Capturer position souris (F7)", BS_PUSHBUTTON, 390, 216, 240, 28);
    AddLabel(hwnd, L"Bornes des ecrans detectes", 20, 266, 220, 20);
    gApp.editScreenBounds = AddEdit(hwnd, kEditScreenBounds, L"", ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL, 20, 291, 610, 130);
    AddLabel(hwnd, L"Points sauvegardes", 650, 158, 160, 20);
    gApp.listSavedPoints = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY, 650, 182, 340, 190, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kListSavedPoints)), nullptr, nullptr);
    gApp.buttonSavePoint = AddButton(hwnd, kButtonSavePoint, L"Sauver position actuelle", BS_PUSHBUTTON, 650, 382, 160, 30);
    gApp.buttonInsertSavedPoint = AddButton(hwnd, kButtonInsertSavedPoint, L"Inserer dans la macro", BS_PUSHBUTTON, 830, 382, 160, 30);
    AddLabel(hwnd, L"Les points enregistres restent dans la session ou dans un profil sauvegarde.", 650, 420, 320, 20);
    AddLabel(hwnd, L"Macro de clics", 20, 445, 180, 20);
    AddLabel(hwnd, L"Nouveau step", 20, 472, 90, 20);
    AddLabel(hwnd, L"Bouton", 120, 472, 50, 20);
    gApp.comboMacroButton = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 175, 468, 120, 160, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kComboMacroButton)), nullptr, nullptr);
    SendMessageW(gApp.comboMacroButton, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Gauche"));
    SendMessageW(gApp.comboMacroButton, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Droit"));
    SendMessageW(gApp.comboMacroButton, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Milieu"));
    SendMessageW(gApp.comboMacroButton, CB_SETCURSEL, 0, 0);
    AddLabel(hwnd, L"Count", 310, 472, 40, 20);
    gApp.editMacroCount = AddEdit(hwnd, kEditMacroCount, L"1", ES_AUTOHSCROLL, 355, 468, 60, 24);
    AddLabel(hwnd, L"Delay ms", 430, 472, 60, 20);
    gApp.editMacroDelay = AddEdit(hwnd, kEditMacroDelay, L"50", ES_AUTOHSCROLL, 495, 468, 70, 24);
    gApp.buttonCaptureMacro = AddButton(hwnd, kButtonCaptureMacro, L"Capturer pour la macro (F8)", BS_PUSHBUTTON, 580, 466, 220, 28);
    gApp.buttonLoadExample = AddButton(hwnd, kButtonLoadExample, L"Charger un exemple", BS_PUSHBUTTON, 810, 466, 180, 28);
    AddLabel(hwnd, L"Format: x,y,button,count,delayMs", 20, 505, 260, 20);
    gApp.editMacro = AddEdit(hwnd, kEditMacro, L"", ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL, 20, 530, 970, 270);
    AddButton(hwnd, kButtonStart, L"Demarrer (F6)", BS_DEFPUSHBUTTON, 710, 825, 130, 34);
    AddButton(hwnd, kButtonStop, L"Arreter", BS_PUSHBUTTON, 860, 825, 130, 34);
    gApp.statusLabel = CreateWindowExW(0, L"STATIC", L"Pret. F6 start/stop, F7 capture point fixe, F8 ajoute une etape macro, F9 capture la fenetre cible.", WS_CHILD | WS_VISIBLE, 20, 830, 670, 40, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kLabelStatus)), nullptr, nullptr);
    const HWND children[] = { gApp.radioSingle, gApp.radioMacro, gApp.radioOutputCursor, gApp.radioOutputBackground, gApp.radioOutputBackgroundPlus, gApp.checkBackgroundFocus, gApp.buttonCaptureTargetWindow, gApp.buttonTestClick, gApp.buttonCaptureSingle, gApp.buttonLoadExample, gApp.buttonCaptureMacro, gApp.buttonSavePoint, gApp.buttonInsertSavedPoint, gApp.buttonSaveProfile, gApp.buttonLoadProfile, gApp.buttonClearLog, gApp.editTargetWindow, gApp.editSingleX, gApp.editSingleY, gApp.comboSingleButton, gApp.editSingleRepeats, gApp.editSingleInterval, gApp.editMacro, gApp.comboMacroButton, gApp.editMacroCount, gApp.editMacroDelay, gApp.listSavedPoints, gApp.editScreenBounds, gApp.editLog, gApp.statusLabel };
    for (HWND child : children)
    {
        SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
    RefreshTargetWindowDisplay();
    RefreshScreenBounds();
    UpdateUiState();
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        gApp.hwnd = hwnd;
        BuildUi(hwnd);
        RegisterHotKey(hwnd, kHotkeyToggle, 0, VK_F6);
        RegisterHotKey(hwnd, kHotkeyCaptureSingle, 0, VK_F7);
        RegisterHotKey(hwnd, kHotkeyCaptureMacro, 0, VK_F8);
        RegisterHotKey(hwnd, kHotkeyCaptureTarget, 0, VK_F9);
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case kModeSingle:
        case kModeMacro:
        case kRadioOutputCursor:
        case kRadioOutputBackground:
            UpdateUiState();
            return 0;
        case kButtonCaptureSingle: CaptureCurrentCursorToSingleFields(); return 0;
        case kButtonStart: StartClicking(); return 0;
        case kButtonStop: StopClicking(); return 0;
        case kButtonLoadExample: FillMacroExample(); return 0;
        case kButtonCaptureMacro: CaptureCurrentCursorToMacro(); return 0;
        case kButtonSavePoint: SaveCurrentCursorPoint(); return 0;
        case kButtonInsertSavedPoint: InsertSelectedSavedPointIntoMacro(); return 0;
        case kButtonCaptureTargetWindow: CaptureTargetWindowFromCursor(); return 0;
        case kButtonTestClick: TestCurrentClick(); return 0;
        case kButtonSaveProfile: SaveProfile(); return 0;
        case kButtonLoadProfile: LoadProfile(); return 0;
        case kButtonClearLog: SetWindowTextW(gApp.editLog, L""); AppendLogLine(L"Journal efface."); return 0;
        default: break;
        }
        break;
    case WM_HOTKEY:
        if (wParam == kHotkeyToggle)
        {
            if (gApp.running.load()) StopClicking(); else StartClicking();
            return 0;
        }
        if (wParam == kHotkeyCaptureSingle) { CaptureCurrentCursorToSingleFields(); return 0; }
        if (wParam == kHotkeyCaptureMacro) { CaptureCurrentCursorToMacro(); return 0; }
        if (wParam == kHotkeyCaptureTarget) { CaptureTargetWindowFromCursor(); return 0; }
        break;
    case WM_DISPLAYCHANGE:
        RefreshScreenBounds();
        return 0;
    case WM_APP + 1:
        if (!gApp.running.load()) SetStatus(L"Sequence terminee.");
        return 0;
    case WM_APP + 2:
        SetStatus(L"Echec du clic en arriere-plan. Verifie la fenetre cible ou repasse en mode curseur reel.");
        return 0;
    case WM_DESTROY:
        StopClicking();
        UnregisterHotKey(hwnd, kHotkeyToggle);
        UnregisterHotKey(hwnd, kHotkeyCaptureSingle);
        UnregisterHotKey(hwnd, kHotkeyCaptureMacro);
        UnregisterHotKey(hwnd, kHotkeyCaptureTarget);
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&controls);
    const wchar_t className[] = L"AutoClickerWindowClass";
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = className;
    RegisterClassExW(&windowClass);
    HWND hwnd = CreateWindowExW(0, className, L"AutoClicker C++ Windows", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, kWindowWidth, kWindowHeight, nullptr, nullptr, instance, nullptr);
    if (hwnd == nullptr)
    {
        return 0;
    }
    ShowWindow(hwnd, showCommand);
    UpdateWindow(hwnd);
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

















