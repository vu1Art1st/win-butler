#include <Windows.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    MessageBoxW(
        nullptr,
        L"This executable exists only to give the win-butler context-menu fixture a packaged application identity.",
        L"win-butler fixture host",
        MB_OK | MB_ICONINFORMATION);
    return 0;
}
