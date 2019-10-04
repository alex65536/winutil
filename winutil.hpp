/*
 * This file is part of WinUtil.
 * This software is public domain. See UNLICENSE for more information.
 * WinUtil was created by Alexander Kernozhitsky.
 */

#ifndef WINUTIL_H_INCLUDED
#define WINUTIL_H_INCLUDED

#include <windows.h>
#include <map>
#include <string>
#include "eventhandler.hpp"

std::string WideStringToUtf8(const std::wstring &str);
std::wstring Utf8ToWideString(const std::string &str);

class WindowsError : public std::runtime_error {
 public:
  explicit WindowsError(const char *what) noexcept : std::runtime_error(what) {}
  explicit WindowsError(const std::string &what) noexcept
      : std::runtime_error(what) {}
};

enum class BorderStyle { None, Single, Sunken, Static };

class Widget {
 public:
  Widget(const Widget &) = delete;
  Widget(Widget &&) = delete;
  Widget &operator=(const Widget &) = delete;
  Widget &operator=(Widget &&) = delete;
  virtual ~Widget();

  inline Widget *Parent() const { return parent_; }
  inline HWND Handle() const { return hWnd_; }
  inline HMENU WidgetId() const { return widgetId_; }

  void SetPosition(const POINT &p);
  void SetSize(const SIZE &sz);
  void SetTitle(const std::wstring &title);

  POINT GetPosition();
  SIZE GetSize();
  std::wstring GetTitle();

  void SetBorder(BorderStyle borderStyle);

  void Show();
  void Hide();

  void SetEnabled(bool enabled);

  void MoveToCenter();

 protected:
  struct WidgetCreationOptions {
    DWORD dwExStyle;
    std::wstring lpWindowName;
    DWORD dwStyle;
    LPVOID lpParam;
    bool subclassWindow;
  };

  Widget(Widget *parent, const LPCWSTR wndClass, POINT pos, SIZE size,
         WidgetCreationOptions options);

  virtual void AddChild(Widget *widget);
  virtual void DeleteChild(Widget *widget);

  Widget *FindWidget(HMENU widgetId);

  const LPCWSTR wndClass;

  HMENU GenerateChildId();

  virtual bool HandleMessage(UINT message, WPARAM wParam, LPARAM lParam,
                             LRESULT &result);

  friend class CustomWindow;

 private:
  HWND hWnd_;
  HMENU widgetId_;
  Widget *parent_;
  std::map<HMENU, Widget *> children_;
  intptr_t lastChildId_;
};

class CustomWindow : public Widget {
 public:
  ~CustomWindow() override;

  friend bool HandleWindow(HWND hWnd, UINT message, WPARAM wParam,
                           LPARAM lParam, LRESULT &result);

  EventHandler<void()> OnClose;
  EventHandler<void()> OnResize;

 protected:
  CustomWindow(Widget *parent, POINT pos, SIZE size,
               WidgetCreationOptions options,
               const LPCWSTR wndClass = L"BaseWindow");

  bool HandleMessage(UINT message, WPARAM wParam, LPARAM lParam,
                     LRESULT &result) override;
};

class Panel : public CustomWindow {
 public:
  Panel(Widget *parent, POINT pos, SIZE size);

 private:
  WidgetCreationOptions GetCreationOptions();
};

class Window : public CustomWindow {
 public:
  Window(Widget *parent, SIZE size, bool isMainWindow = false);

 protected:
  WidgetCreationOptions GetCreationOptions(bool isMainWindow);

  bool HandleMessage(UINT message, WPARAM wParam, LPARAM lParam,
                     LRESULT &result) override;

 private:
  bool isMainWindow_;
};

class Label : public Widget {
 public:
  Label(Widget *parent, POINT pos, const std::wstring &title = L"Label");

 private:
  WidgetCreationOptions GetCreationOptions(const std::wstring &title);
};

class Button : public Widget {
 public:
  Button(Widget *parent, POINT pos, const std::wstring &title = L"Button");

  EventHandler<void()> OnClick;

 protected:
  virtual bool HandleMessage(UINT message, WPARAM wParam, LPARAM lParam,
                             LRESULT &result);

 private:
  WidgetCreationOptions GetCreationOptions(const std::wstring &title);
};

class CustomEdit : public Widget {
 public:
  void SetReadOnly(bool value);
  bool GetReadOnly();

 protected:
  CustomEdit(Widget *parent, POINT pos, SIZE size,
             WidgetCreationOptions options);

  WidgetCreationOptions GetCreationOptions(const std::wstring &title);
};

class Edit : public CustomEdit {
 public:
  Edit(Widget *parent, POINT pos, int width,
       const std::wstring &title = L"Edit");

 protected:
  WidgetCreationOptions GetCreationOptions(const std::wstring &title);
};

class Memo : public CustomEdit {
 public:
  Memo(Widget *parent, POINT pos, SIZE size,
       const std::wstring &title = L"Memo");

 protected:
  WidgetCreationOptions GetCreationOptions(const std::wstring &title);
};

class GroupBox : public CustomWindow {
 public:
  GroupBox(Widget *parent, POINT pos, SIZE size,
           const std::wstring &title = L"Group Box");

 private:
  WidgetCreationOptions GetCreationOptions(const std::wstring &title);
};

class ListBox : public Widget {
 public:
  ListBox(Widget *parent, POINT pos, SIZE size);

  void AddLine(const std::wstring &line);
  void InsertLine(int position, const std::wstring &line);
  void RemoveLine(int position);
  void Clear();

 private:
  WidgetCreationOptions GetCreationOptions();
};

void InitApplication(HINSTANCE hInstance);
int StartMainLoop();

#endif  // WINUTIL_H_INCLUDED
