/*
 * This file is part of WinUtil.
 * This software is public domain. See UNLICENSE for more information.
 * WinUtil was created by Alexander Kernozhitsky.
 */

#include "winutil.hpp"
#include <commctrl.h>
#include <cassert>
#include <codecvt>
#include <locale>
#include <map>
#include <string>

HINSTANCE g_hInstance;
static std::map<HWND, CustomWindow *> g_Windows;

bool HandleWindow(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam,
                  LRESULT &result) {
  if (g_Windows.count(hWnd) &&
      g_Windows.at(hWnd)->HandleMessage(message, wParam, lParam, result)) {
    return true;
  }
  return false;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam,
                         LPARAM lParam) {
  LRESULT result = 0;
  if (HandleWindow(hWnd, message, wParam, lParam, result)) {
    return result;
  }
  return DefWindowProcW(hWnd, message, wParam, lParam);
}

LRESULT CALLBACK SubclassProc(HWND hWnd, UINT message, WPARAM wParam,
                              LPARAM lParam) {
  WNDPROC origWindowProc = (WNDPROC)GetWindowLongPtr(hWnd, GWLP_USERDATA);
  LRESULT result = 0;
  if (HandleWindow(hWnd, message, wParam, lParam, result)) {
    return result;
  }
  return CallWindowProcW(origWindowProc, hWnd, message, wParam, lParam);
}

void SubclassWindow(HWND hWnd) {
  WNDPROC oldWndProc =
      (WNDPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)SubclassProc);
  SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)oldWndProc);
}

void RegisterBaseWindowClass() {
  WNDCLASSEXW wndClass = {0};
  wndClass.cbSize = sizeof(WNDCLASSEXW);
  wndClass.lpfnWndProc = (WNDPROC)WndProc;
  wndClass.hInstance = g_hInstance;
  wndClass.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
  wndClass.lpszClassName = L"BaseWindow";
  wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
  wndClass.style = CS_HREDRAW | CS_VREDRAW;
  if (RegisterClassExW(&wndClass) == 0) {
    throw WindowsError("unable to register window class");
  }
}

void InitApplication(HINSTANCE hInstance) {
  g_hInstance = hInstance;
  RegisterBaseWindowClass();
}

int StartMainLoop() {
  MSG msg = {0};
  int msgStatus = 0;
  while ((msgStatus = GetMessage(&msg, NULL, 0, 0))) {
    if (msgStatus == -1) {
      return 1;
    }
    if (IsDialogMessage(GetActiveWindow(), &msg)) {
      continue;
    }
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  return static_cast<int>(msg.wParam);
}

bool Widget::HandleMessage(UINT, WPARAM, LPARAM, LRESULT &) { return false; }

void Widget::AddChild(Widget *widget) {
  assert(!children_.count(widget->widgetId_));
  children_[widget->widgetId_] = widget;
}

void Widget::DeleteChild(Widget *widget) {
  assert(children_.count(widget->widgetId_));
  children_.erase(widget->widgetId_);
}

HMENU Widget::GenerateChildId() {
  return reinterpret_cast<HMENU>(++lastChildId_);
}

Widget::Widget(Widget *parent, const LPCWSTR wndClass, POINT pos, SIZE size,
               Widget::WidgetCreationOptions options)
    : wndClass(wndClass), widgetId_(nullptr), parent_(parent), lastChildId_(0) {
  if (parent_ != nullptr) {
    options.dwStyle |= WS_CHILD;
    widgetId_ = parent_->GenerateChildId();
  }
  hWnd_ =
      CreateWindowExW(options.dwExStyle, wndClass, options.lpWindowName.c_str(),
                      options.dwStyle, pos.x, pos.y, size.cx, size.cy,
                      parent == nullptr ? 0 : parent->Handle(), widgetId_,
                      g_hInstance, options.lpParam);
  if (hWnd_ == 0) {
    throw WindowsError("could not create window");
  }
  if (options.subclassWindow) {
    SubclassWindow(hWnd_);
  }
  SendMessage(hWnd_, WM_SETFONT, (LPARAM)GetStockObject(DEFAULT_GUI_FONT),
              true);
  if (parent_ != nullptr) {
    parent_->AddChild(this);
  }
}

Widget *Widget::FindWidget(HMENU widgetId) {
  if (children_.count(widgetId)) {
    return children_.at(widgetId);
  }
  return nullptr;
}

Widget::~Widget() {
  while (!children_.empty()) {
    Widget *child = children_.begin()->second;
    delete child;
  }
  if (parent_ != nullptr) {
    parent_->DeleteChild(this);
  }
  DestroyWindow(hWnd_);
}

void Widget::MoveToCenter() {
  RECT rect;
  GetWindowRect(parent_ == nullptr ? GetDesktopWindow() : parent_->Handle(),
                &rect);
  SIZE size = GetSize();
  SetPosition({(rect.right - size.cx) / 2, (rect.bottom - size.cy) / 2});
}

POINT Widget::GetPosition() {
  RECT rect;
  GetWindowRect(hWnd_, &rect);
  POINT res{rect.left, rect.top};
  if (parent_ != nullptr) {
    ScreenToClient(parent_->Handle(), &res);
  }
  return res;
}

SIZE Widget::GetSize() {
  RECT rect;
  GetWindowRect(hWnd_, &rect);
  return {rect.right - rect.left, rect.bottom - rect.top};
}

void Widget::SetPosition(const POINT &p) {
  SIZE size = GetSize();
  MoveWindow(hWnd_, p.x, p.y, size.cx, size.cy, true);
}

void Widget::SetSize(const SIZE &sz) {
  POINT pos = GetPosition();
  MoveWindow(hWnd_, pos.x, pos.y, sz.cx, sz.cy, true);
}

std::wstring Widget::GetTitle() {
  wchar_t buf[1000];
  GetWindowTextW(hWnd_, buf, 1000);
  return std::wstring(buf);
}

void Widget::SetTitle(const std::wstring &title) {
  SetWindowTextW(hWnd_, title.c_str());
}

void Widget::SetEnabled(bool enable) { EnableWindow(hWnd_, enable); }

void Widget::SetBorder(BorderStyle borderStyle) {
  LONG_PTR extStyle = GetWindowLongPtrW(Handle(), GWL_EXSTYLE);
  LONG_PTR style = GetWindowLongPtrW(Handle(), GWL_STYLE);
  extStyle &= ~WS_EX_CLIENTEDGE;
  extStyle &= ~WS_EX_STATICEDGE;
  style &= ~WS_BORDER;
  switch (borderStyle) {
    case BorderStyle::Single: {
      style |= WS_BORDER;
      break;
    }
    case BorderStyle::Sunken: {
      extStyle |= WS_EX_CLIENTEDGE;
      break;
    }
    case BorderStyle::Static: {
      extStyle |= WS_EX_STATICEDGE;
      break;
    }
    case BorderStyle::None: {
      // do nothing
      break;
    }
  }
  SetWindowLongPtrW(Handle(), GWL_EXSTYLE, extStyle);
  SetWindowLongPtrW(Handle(), GWL_STYLE, style);
}

CustomWindow::CustomWindow(Widget *parent, POINT pos, SIZE size,
                           Widget::WidgetCreationOptions options,
                           const LPCWSTR wndClass)
    : Widget(parent, wndClass, pos, size, options) {
  g_Windows[Handle()] = this;
}

Window::Window(Widget *parent, SIZE size, bool isMainWindow)
    : CustomWindow(parent, {0, 0}, size, GetCreationOptions(isMainWindow)),
      isMainWindow_(isMainWindow) {
  MoveToCenter();
}

bool CustomWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam,
                                 LRESULT &result) {
  switch (message) {
    case WM_CLOSE: {
      OnClose.Activate();
      break;
    }
    case WM_COMMAND: {
      intptr_t item = LOWORD(wParam);
      Widget *widget = FindWidget((HMENU)item);
      if (widget == nullptr) {
        return false;
      }
      if (widget->HandleMessage(message, wParam, lParam, result)) {
        return true;
      }
      break;
    }
    case WM_SIZE: {
      OnResize.Activate();
      break;
    }
  }
  return false;
}

bool Window::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam,
                           LRESULT &result) {
  if (CustomWindow::HandleMessage(message, wParam, lParam, result)) {
    return true;
  }
  switch (message) {
    case WM_CLOSE: {
      if (isMainWindow_) {
        PostQuitMessage(0);
        return true;
      }
      break;
    }
  }
  return false;
}

Widget::WidgetCreationOptions Window::GetCreationOptions(bool isMainWindow) {
  WidgetCreationOptions options = {0};
  options.lpWindowName = L"Window";
  options.dwExStyle = WS_EX_CONTROLPARENT;
  options.dwStyle = WS_OVERLAPPEDWINDOW;
  if (isMainWindow) {
    options.dwStyle |= WS_VISIBLE;
  }
  return options;
}

CustomWindow::~CustomWindow() { g_Windows.erase(Handle()); }

void Widget::Hide() { ShowWindow(Handle(), SW_HIDE); }

void Widget::Show() { ShowWindow(Handle(), SW_SHOW); }

SIZE GetTextSize(HWND hWnd, const std::wstring &text) {
  HDC dc = GetDC(hWnd);
  SIZE size;
  GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
  ReleaseDC(hWnd, dc);
  return size;
}

Label::Label(Widget *parent, POINT pos, const std::wstring &title)
    : Widget(parent, L"Static", pos, {1, 1}, GetCreationOptions(title)) {
  SIZE size = GetTextSize(Handle(), title);
  size.cx += 2;
  size.cy += 2;
  SetSize(size);
}

Widget::WidgetCreationOptions Label::GetCreationOptions(
    const std::wstring &title) {
  WidgetCreationOptions options = {0};
  options.dwStyle = WS_VISIBLE;
  options.lpWindowName = title.c_str();
  return options;
}

Widget::WidgetCreationOptions Button::GetCreationOptions(
    const std::wstring &title) {
  WidgetCreationOptions options = {0};
  options.dwStyle = WS_VISIBLE | WS_TABSTOP;
  options.lpWindowName = title.c_str();
  return options;
}

Button::Button(Widget *parent, POINT pos, const std::wstring &title)
    : Widget(parent, L"Button", pos, {1, 1}, GetCreationOptions(title)) {
  SIZE size = GetTextSize(Handle(), title);
  size.cx += 16;
  size.cy += 8;
  SetSize(size);
}

bool Button::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam,
                           LRESULT &result) {
  UNREFERENCED_PARAMETER(wParam);
  UNREFERENCED_PARAMETER(lParam);
  switch (message) {
    case WM_COMMAND: {
      OnClick.Activate();
      result = 0;
      return true;
      break;
    }
  }
  return false;
}

Widget::WidgetCreationOptions GroupBox::GetCreationOptions(
    const std::wstring &title) {
  WidgetCreationOptions options = {0};
  options.dwExStyle = WS_EX_CONTROLPARENT;
  options.dwStyle = WS_VISIBLE | BS_GROUPBOX | WS_GROUP;
  options.lpWindowName = title.c_str();
  options.subclassWindow = true;
  return options;
}

GroupBox::GroupBox(Widget *parent, POINT pos, SIZE size,
                   const std::wstring &title)
    : CustomWindow(parent, pos, size, GetCreationOptions(title), L"Button") {}

CustomEdit::CustomEdit(Widget *parent, POINT pos, SIZE size,
                       Widget::WidgetCreationOptions options)
    : Widget(parent, L"Edit", pos, size, options) {}

bool CustomEdit::GetReadOnly() {
  return GetWindowLongPtrW(Handle(), GWL_STYLE) & ES_READONLY;
}

void CustomEdit::SetReadOnly(bool value) {
  SendMessageW(Handle(), EM_SETREADONLY, (WPARAM)value, 0);
}

Widget::WidgetCreationOptions Edit::GetCreationOptions(
    const std::wstring &title) {
  return CustomEdit::GetCreationOptions(title);
}

Edit::Edit(Widget *parent, POINT pos, int width, const std::wstring &title)
    : CustomEdit(parent, pos, {1, 1}, GetCreationOptions(title)) {
  SIZE size = GetTextSize(Handle(), title + L" ");
  size.cx = width;
  size.cy += 12;
  SetSize(size);
}

Widget::WidgetCreationOptions Memo::GetCreationOptions(
    const std::wstring &title) {
  WidgetCreationOptions options = CustomEdit::GetCreationOptions(title);
  options.dwStyle |= WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL;
  return options;
}

Memo::Memo(Widget *parent, POINT pos, SIZE size, const std::wstring &title)
    : CustomEdit(parent, pos, size, GetCreationOptions(title)) {}

Widget::WidgetCreationOptions CustomEdit::GetCreationOptions(
    const std::wstring &title) {
  WidgetCreationOptions options = {0};
  options.dwExStyle = WS_EX_CLIENTEDGE;
  options.dwStyle = WS_VISIBLE | WS_BORDER | WS_TABSTOP;
  options.lpWindowName = title.c_str();
  return options;
}

Widget::WidgetCreationOptions Panel::GetCreationOptions() {
  WidgetCreationOptions options = {0};
  options.dwExStyle = WS_EX_STATICEDGE | WS_EX_CONTROLPARENT;
  options.dwStyle = WS_VISIBLE;
  return options;
}

Panel::Panel(Widget *parent, POINT pos, SIZE size)
    : CustomWindow(parent, pos, size, GetCreationOptions()) {}

void ListBox::AddLine(const std::wstring &line) {
  SendMessageW(Handle(), LB_ADDSTRING, 0, (LPARAM)line.c_str());
}

void ListBox::InsertLine(int position, const std::wstring &line) {
  SendMessageW(Handle(), LB_INSERTSTRING, position, (LPARAM)line.c_str());
}

void ListBox::RemoveLine(int position) {
  SendMessageW(Handle(), LB_DELETESTRING, position, 0);
}

void ListBox::Clear() { SendMessageW(Handle(), LB_RESETCONTENT, 0, 0); }

int ListBox::GetCount() { return SendMessageW(Handle(), LB_GETCOUNT, 0, 0); }

int ListBox::GetSelectedItem() {
  int res = SendMessageW(Handle(), LB_GETCURSEL, 0, 0);
  if (res == LB_ERR) {
    return -1;
  }
  return res;
}

Widget::WidgetCreationOptions ListBox::GetCreationOptions() {
  WidgetCreationOptions options = {0};
  options.dwExStyle = WS_EX_CLIENTEDGE;
  options.dwStyle = WS_VISIBLE | WS_TABSTOP;
  return options;
}

ListBox::ListBox(Widget *parent, POINT pos, SIZE size)
    : Widget(parent, L"ListBox", pos, size, GetCreationOptions()) {}

static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> convert;

std::string WideStringToUtf8(const std::wstring &str) {
  return convert.to_bytes(str);
}

std::wstring Utf8ToWideString(const std::string &str) {
  return convert.from_bytes(str);
}
