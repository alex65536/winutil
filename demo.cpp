/*
 * This file is part of WinUtil.
 * This software is public domain. See UNLICENSE for more information.
 * WinUtil was created by Alexander Kernozhitsky.
 */

#include <sstream>
#include <string>
#include "winutil.hpp"

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                     LPTSTR lpCmdLine, int nCmdShow) {
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);
  UNREFERENCED_PARAMETER(nCmdShow);

  InitApplication(hInstance);

  Window window(nullptr, {800, 450}, true);
  window.SetTitle(L"WinUtil demo");

  GroupBox group(&window, {0, 0}, {200, 200}, L"Group");
  Panel panel(&window, {220, 10}, {200, 200});
  panel.SetBorder(BorderStyle::Single);

  Button *activeButton = new Button(&group, {10, 20}, L"I am active!");
  activeButton->OnClick.AddEvent([&]() {
    MessageBoxW(0, L"Hello!", L"Message", MB_ICONINFORMATION | MB_OK);
  });
  Button *inactiveButton = new Button(&group, {10, 50}, L"I am inactive!");
  inactiveButton->SetEnabled(false);

  Edit *edit = new Edit(&group, {10, 80}, 100, L"Edit me!");
  Edit *inactiveEdit = new Edit(&group, {10, 110}, 100, L"Don\'t edit me!");
  inactiveEdit->SetReadOnly(true);

  new Label(&window, {4, 230}, L"Labels just keep static text");

  new Memo(&panel, {5, 5}, {190, 190},
           L"Here you can enter long\nmultiline text");

  Panel listBoxPanel(&window, {450, 0}, {300, 400});
  ListBox listBox(&listBoxPanel, {5, 5}, {290, 300});

  Button pushButton(&listBoxPanel, {5, 305}, L"Push to list");
  Button popButton(&listBoxPanel, {10 + pushButton.GetSize().cx, 305},
                   L"Pop from list");
  Button testButton(
      &listBoxPanel,
      {10 + popButton.GetPosition().x + popButton.GetSize().cx, 305},
      L"Test list");
  int lastItem = 0;
  pushButton.OnClick.AddEvent([&]() {
    ++lastItem;
    listBox.AddLine(Utf8ToWideString(std::to_string(lastItem)));
  });
  popButton.OnClick.AddEvent([&]() {
    if (listBox.GetCount() == 0) {
      MessageBoxW(0, L"List box is empty!", L"Error", MB_ICONERROR | MB_OK);
    }
    listBox.RemoveLine(0);
  });
  testButton.OnClick.AddEvent([&]() {
    std::ostringstream os;
    os << "List size = " << listBox.GetCount()
       << ", selected index = " << listBox.GetSelectedItem();
    std::wstring msg = Utf8ToWideString(os.str());
    MessageBoxW(0, msg.c_str(), L"Info", MB_ICONINFORMATION | MB_OK);
  });

  return StartMainLoop();
}
