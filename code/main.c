/*<=========================附在前面=============================>

项目说明：本项目为C语言大作业，实现了一个局域网互相传输文件的小工具，灵感来源于生活
本程序运行于Windows平台（不会写跨平台/(ㄒoㄒ)/~~）

版本号：v1.0(我瞎编的，因为有没有后面的还两说)

DONE:
- GUI交互，包括连接到对方主机，启动监听等待接收文件，单一界面集成客户端和服务端
- 多线程，程序的GUI、文件上传、文件下载，三者分别在独立的线程进行，并且实现了同步到GUI线程，最后在进程关闭时关闭所有线程
- Winsock编程，实现了局域网基于TCP/IP协议的文件传输功能，并集成客户端和服务端到GUI中

TODO:
- 文件夹传输，因为文件夹的深度遍历和结构的传输难以短时间解决
- 大文件传输，因为断点续传尚未实现
- 更加高效可靠的错误处理，比如说用户突然关闭了进程
- 降低代码耦合性，将网络，文件操作，GUI分离成单独的模块
- 优化GUI界面

<=========================附在前面=============================>*/

#include <windows.h> //Windows API头文件
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>  // bool类型
#include <commctrl.h> // 用于创建列表框
#include <shlobj.h>   // 用于选择文件夹
#include <Shlwapi.h>  // 用于获取可执行文件的路径

#define BUFFER_SIZE 1024
#define PORT 8888

#define IDI_FILETRANSFER_ICON 101

//<===========================全局变量声明开始============================>
// 全局变量
HINSTANCE hInst;
HWND hwndEditConfigIP;             // 输入IP框
HWND hwndButtonConfigIP;           // IP确认按钮
HWND hwndEditConfigLocation;       // 输入文件保存位置框
HWND hwndButtonConfig;             // 按钮
HWND hwndUploadFile;               // 上传文件提示区
HWND hwndUploadFolder;             // 上传文件夹提示区
HWND hwndUploadFileButton;         // 上传文件按钮
HWND hwndUploadListBox;            // 上传区域列表框
HWND hwndAuthorInfo;               // 作者信息栏
HWND hwndUploadListBoxClearButton; // 上传列表清空按钮
HWND hwndDownloadListBox;          // 下载列表提示
HFONT hFont24 = CreateFont(24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                           OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                           DEFAULT_PITCH | FF_DONTCARE, TEXT("微软雅黑"));
HFONT hFont20 = CreateFont(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                           OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                           DEFAULT_PITCH | FF_DONTCARE, TEXT("微软雅黑"));

// HANDLE hUploadThread = NULL; // 上传线程句柄
HANDLE hReceiveThread = NULL; // 接收线程句柄

// const char *defaultTextIP = "192.168.13.132";
// const char *defaultTextLocation = "F:\\program\\project\\code";
const char *defaultTextIP = "请输入对方IP地址";
const char *defaultTextLocation = "请输入文件保存位置";

// 上传文件参数结构体
typedef struct
{
    HWND hwnd;
    char serverIP[256];
    char filePath[260];
} UploadParams;

char IP[16] = {0};            // IP地址
char saveLocation[256] = {0}; // 文件保存位置
//<========================全局变量声明结束============================>

//<========================函数原型声明开始============================>
// 加载图标
HICON LoadIconWithRelativePath(HINSTANCE hInstance, LPCSTR relativePath);

//设置字体大小
void setFontSize24(HWND hwnd);
void setFontSize20(HWND hwnd);

//创建窗口控件
void CreateMainWindowControls(HWND hwnd);

//处理编辑框获得或失去焦点的情况
void HandleEditBoxFocus(HWND hwndEdit, const char *defaultText, BOOL isFocus);

// 检查 IP 地址是否合法
bool isValidIPv4(const char *ip);

// 检查文件保存位置是否合法
bool isValidSaveLocation(const char *path);

// 移除字符串末尾的换行符
void trimTrailingSpaces(char *str);

// 从列表框中获取文件路径
void SelectFileAndAddToList(HWND hwndList);

// 从列表框中获取文件夹路径
void SelectFolderAndAddToList(HWND hwndList);

// 发送文件
void sendFile(SOCKET serverSocket, const char *filePath);

// 用于处理错误的函数，写入.log文件，以追加的方式，带有时间戳
void ErrorHandling(char *message);

// 传输文件到服务器
void transferFileToServer(const char *serverAddress, int port, const char *filePath);

// 文件上传线程
DWORD WINAPI FileUploadThread(LPVOID lpParam);

// 接收文件
// void receiveFile(SOCKET clientSocket, const char *savePath);

// 文件接收线程
DWORD WINAPI FileReceiveThread(LPVOID lpParam);

//将接收到的文件路径添加到列表框中
void AddFilePathToListBox(HWND hwndList, const char *filePath);

// WinMain：程序入口点
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   PSTR szCmdLine, int iCmdShow);

// 窗口过程
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

//<========================函数原型声明结束============================>

// 窗口过程
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{

    switch (message)
    {
    case WM_CREATE:
        CreateMainWindowControls(hwnd);
        return 0;
    case WM_COMMAND:
    {
        // 如果文件保存位置输入框获得焦点，且内容为提示文本，则清空
        if (HIWORD(wParam) == EN_SETFOCUS)
        {
            // 获得焦点
            if ((HWND)lParam == hwndEditConfigLocation)
            {
                HandleEditBoxFocus((HWND)lParam, defaultTextLocation, TRUE);
            }
            else if ((HWND)lParam == hwndEditConfigIP)
            {
                HandleEditBoxFocus((HWND)lParam, defaultTextIP, TRUE);
            }
        }
        else if (HIWORD(wParam) == EN_KILLFOCUS)
        {
            // 失去焦点
            if ((HWND)lParam == hwndEditConfigLocation)
            {
                HandleEditBoxFocus((HWND)lParam, defaultTextLocation, FALSE);
            }
            else if ((HWND)lParam == hwndEditConfigIP)
            {
                HandleEditBoxFocus((HWND)lParam, defaultTextIP, FALSE);
            }
        }

        switch (LOWORD(wParam))
        {
        // case 1:
        //     // MessageBox(hwnd, "IP输入框", "调试信息", MB_OK);
        //     break;
        case 2:
        {
            //获取IP地址
            char serverIP[256];
            GetWindowText(hwndEditConfigIP, serverIP, 256);

            //检查IP地址是否合法
            if (isValidIPv4(serverIP) == false)
            {
                MessageBox(hwnd, "请输入正确的IP地址", "错误提示", MB_OK);
                break;
            }

            char tipsText[512];
            snprintf(tipsText, sizeof(tipsText), "您即将连接到IP地址为：%s的主机", serverIP);
            MessageBox(hwnd, tipsText, "提示信息", MB_OK);

            break;
        }

        case 4:
            // SelectFileAndAddToList(hwndUploadListBox);
            {
                //获取文件保存位置
                GetWindowText(hwndEditConfigLocation, saveLocation, 256);

                //检查文件保存位置是否合法
                if (isValidSaveLocation(saveLocation) == false)
                {
                    MessageBox(hwnd, "请输入正确的文件保存位置", "错误提示", MB_OK);
                    break;
                }

                char tipsText[512];
                snprintf(tipsText, sizeof(tipsText), "开始监听8888端口，接收文件，文件保存位置：\n%s", saveLocation);
                MessageBox(hwnd, tipsText, "提示信息", MB_OK);

                HANDLE hThread = CreateThread(NULL, 0, FileReceiveThread, (LPVOID)hwnd, 0, NULL);
                if (hThread == NULL)
                {
                    // 处理线程创建失败
                    MessageBox(hwnd, "线程创建失败", "错误提示", MB_OK);
                }

                break;
            }
        case 5:
            SelectFileAndAddToList(hwndUploadListBox);
            break;
        case 6:
            MessageBox(hwnd, "抱歉，此功能尚未实现，敬请期待", "提示信息", MB_OK);
            // SelectFolderAndAddToList(hwndUploadListBox);
            break;
        case 7:
        {
            // 获取IP
            char serverIP[256];
            GetWindowText(hwndEditConfigIP, serverIP, 256);

            if (isValidIPv4(serverIP) == false)
            {
                MessageBox(hwnd, "请输入正确的IP地址", "错误提示", MB_OK);
                break;
            }

            // 从hwndUploadListBox中读取每个路径
            int itemCount = SendMessage(hwndUploadListBox, LB_GETCOUNT, 0, 0);
            for (int i = 0; i < itemCount; ++i)
            {
                // 获取每一项的文本长度
                int filePathLength = SendMessage(hwndUploadListBox, LB_GETTEXTLEN, i, 0);

                // 分配足够的内存来存储文本（包括终止字符）
                char *filePath = new char[filePathLength + 1];

                // 获取项的文本
                SendMessage(hwndUploadListBox, LB_GETTEXT, (WPARAM)i, (LPARAM)filePath);

                // 移除字符串末尾的换行符
                trimTrailingSpaces(filePath);

                // 创建参数结构体并分配内存
                UploadParams *params = new UploadParams;
                strcpy(params->serverIP, serverIP);
                strcpy(params->filePath, filePath);
                params->hwnd = hwnd;

                // 创建上传线程
                HANDLE hUploadThread = CreateThread(NULL, 0, FileUploadThread, (LPVOID)params, 0, NULL);
                if (hUploadThread == NULL)
                {
                    // 处理线程创建失败
                    MessageBox(hwnd, "上传线程创建失败", "错误", MB_OK);
                    delete params; // 释放分配的内存
                }

                // 释放分配的内存
                delete[] filePath;
            }
            break;
        }
        case 9:
            // 清空上传列表
            if (MessageBox(hwnd, "是否清空上传列表", "提示信息", MB_OKCANCEL) == IDOK)
            {
                SendMessage(hwndUploadListBox, LB_RESETCONTENT, 0, 0);
            }
            break;
        case 11:
            ShellExecute(0, "open", "https://thelazy.cn/2023/11/25/%E4%BA%A7%E5%93%81%E4%BB%8B%E7%BB%8D/", NULL, NULL, SW_SHOWNORMAL);
            break;
        }
        return 0;
    }
    case WM_USER + 1:
    {
        char *filePath = (char *)wParam;
        // 将接收到的文件路径添加到列表框中
        SendMessage(hwndDownloadListBox, LB_ADDSTRING, 0, (LPARAM)filePath);
        free(filePath); // 释放路径字符串
        break;
    }
    // case WM_USER + 2:
    // {
    //     MessageBox(hwnd, "所有文件上传完成", "提示", MB_OK);

    //     break;
    // }
    case WM_GETMINMAXINFO:
    {
        LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
        lpMMI->ptMinTrackSize.x = 974; // 窗口的最小宽度
        lpMMI->ptMinTrackSize.y = 727; // 窗口的最小高度
        lpMMI->ptMaxTrackSize.x = 974; // 窗口的最大宽度
        lpMMI->ptMaxTrackSize.y = 727; // 窗口的最大高度
        return 0;
    }

    case WM_DESTROY:
    {
        // 关闭下载线程
        if (hReceiveThread != NULL)
        {
            TerminateThread(hReceiveThread, INFINITE);
            CloseHandle(hReceiveThread);
        }

        PostQuitMessage(0);
        return 0;
    }
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
    DeleteObject(hFont24); // 释放字体资源
    DeleteObject(hFont20); // 释放字体资源
}

// WinMain：程序入口点
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   PSTR szCmdLine, int iCmdShow)
{
    static TCHAR myAppName[] = TEXT("FileTransfer");
    HWND hwnd;
    MSG msg;
    WNDCLASS wndclass;

    wndclass.style = CS_HREDRAW | CS_VREDRAW;                                      // 窗口样式
    wndclass.lpfnWndProc = WndProc;                                                // 窗口过程
    wndclass.cbClsExtra = 0;                                                       // 窗口类的附加内存
    wndclass.cbWndExtra = 0;                                                       // 窗口的附加内存
    wndclass.hInstance = hInstance;                                                // 实例句柄
    wndclass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_FILETRANSFER_ICON));  // 图标
    wndclass.hIcon = LoadIconWithRelativePath(hInstance, "src\\fileTransfer.ico"); // 图标，这看起来是重复的，确实，但不知道为什么，只有这样才能正常显示窗口图标，上面的那个是文件图标
    // if(wndclass.hIcon == NULL)
    // {
    //     MessageBox(NULL, TEXT("图标加载失败"), myAppName, MB_ICONERROR);
    //     return 0;
    // }
    wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);      // 光标
    wndclass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // 背景色
    wndclass.lpszMenuName = NULL;                        // 菜单
    wndclass.lpszClassName = myAppName;                  // 窗口类名

    if (!RegisterClass(&wndclass))
    {
        MessageBox(NULL, TEXT("This program requires Windows NT!"),
                   myAppName, MB_ICONERROR);
        return 0;
    }

    hwnd = CreateWindow(myAppName, TEXT("文件传输小工具"),
                        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
                        CW_USEDEFAULT, CW_USEDEFAULT,
                        CW_USEDEFAULT, CW_USEDEFAULT,
                        NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, iCmdShow);
    UpdateWindow(hwnd);

    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
}

// 加载图标
HICON LoadIconWithRelativePath(HINSTANCE hInstance, LPCSTR relativePath)
{
    CHAR exePath[MAX_PATH];
    HICON hIcon = NULL;

    // 获取可执行文件的完整路径
    if (GetModuleFileName(NULL, exePath, MAX_PATH) != 0)
    {
        // 移除文件名，只保留目录
        PathRemoveFileSpec(exePath);

        // 构建图标的完整路径
        strcat(exePath, "\\");
        strcat(exePath, relativePath);

        // 加载图标
        hIcon = (HICON)LoadImage(hInstance, exePath, IMAGE_ICON, 64, 64, LR_LOADFROMFILE);
    }

    return hIcon;
}

// 设置字体24号大小
void setFontSize24(HWND hwnd)
{
    SendMessage(hwnd, WM_SETFONT, (WPARAM)hFont24, TRUE);
}

// 设置字体20号大小
void setFontSize20(HWND hwnd)
{
    SendMessage(hwnd, WM_SETFONT, (WPARAM)hFont20, TRUE);
}
// 窗口过程函数声明
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

//创建窗口控件
void CreateMainWindowControls(HWND hwnd)
{

    // 创建IP输入框
    hwndEditConfigIP = CreateWindow(TEXT("edit"), defaultTextIP,
                                    WS_CHILD | WS_VISIBLE | WS_BORDER,
                                    30, 30, 150, 30,
                                    hwnd, (HMENU)1, hInst, NULL);

    setFontSize24(hwndEditConfigIP); // 设置字体24号大小

    //创建IP确认按钮
    hwndButtonConfigIP = CreateWindow(TEXT("button"), TEXT("连接此IP"),
                                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                      200, 30, 110, 30,
                                      hwnd, (HMENU)2, hInst, NULL);
    setFontSize24(hwndButtonConfigIP); // 设置字体24号大小

    //创建文件保存位置框，用户在此输入要保存的文件位置
    hwndEditConfigLocation = CreateWindow(TEXT("edit"), defaultTextLocation,
                                          WS_CHILD | WS_VISIBLE | WS_BORDER,
                                          330, 30, 480, 30,
                                          hwnd, (HMENU)3, hInst, NULL);
    setFontSize24(hwndEditConfigLocation); // 设置字体24号大小

    // 创建文件接收按钮
    hwndButtonConfig = CreateWindow(TEXT("button"), TEXT("启动接收"),
                                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                    830, 30, 100, 30,
                                    hwnd, (HMENU)4, hInst, NULL);
    setFontSize24(hwndButtonConfig); // 设置字体24号大小
    //创建上传文件提示区
    hwndUploadFile = CreateWindow(TEXT("Button"), TEXT("上传文件"),
                                  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                  30, 80, 200, 40,
                                  hwnd, (HMENU)5, hInst, NULL);
    setFontSize24(hwndUploadFile); // 设置字体24号大小

    //创建上传文件夹提示区
    hwndUploadFolder = CreateWindow(TEXT("Button"), TEXT("上传文件夹"),
                                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                    260, 80, 200, 40,
                                    hwnd, (HMENU)6, hInst, NULL);
    setFontSize24(hwndUploadFolder); // 设置字体24号大小

    hwndUploadFileButton = CreateWindow(TEXT("Button"), TEXT("确认上传"),
                                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                        30, 140, 432, 40,
                                        hwnd, (HMENU)7, hInst, NULL);
    setFontSize24(hwndUploadFileButton); // 设置字体24号大小

    // 创建上传区域列表框
    hwndUploadListBox = CreateWindow(TEXT("listbox"), NULL,
                                     WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY | WS_VSCROLL | WS_HSCROLL,
                                     30, 200, 432, 420,
                                     hwnd, (HMENU)8, hInst, NULL);
    setFontSize20(hwndUploadListBox); // 设置字体20号大小

    //创建上传列表清空按钮
    hwndUploadListBoxClearButton = CreateWindow(TEXT("Button"), TEXT("清空列表"),
                                                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                                382, 620, 80, 40,
                                                hwnd, (HMENU)9, hInst, NULL);
    setFontSize24(hwndUploadListBoxClearButton); // 设置字体20号大小

    // 创建文件下载区域区域
    hwndDownloadListBox = CreateWindow(TEXT("listbox"), NULL,
                                       WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY | WS_VSCROLL | WS_HSCROLL,
                                       500, 80, 430, 600,
                                       hwnd, (HMENU)10, hInst, NULL);
    setFontSize20(hwndDownloadListBox); // 设置字体20号大小

    // 创建作者信息栏
    hwndAuthorInfo = CreateWindow(TEXT("Button"), TEXT("作者：TheLazy\n版本：v1.0\n日期：2023-11-25"),
                                  WS_CHILD | WS_VISIBLE | WS_BORDER | BS_PUSHBUTTON,
                                  30, 620, 300, 40,
                                  hwnd, (HMENU)11, hInst, NULL);
    setFontSize20(hwndAuthorInfo); // 设置字体24号大小
}

// 处理编辑框获得或失去焦点的情况
void HandleEditBoxFocus(HWND hwndEdit, const char *defaultText, BOOL isFocus)
{
    int len = GetWindowTextLength(hwndEdit);
    if (isFocus)
    {
        // 获得焦点
        if (len == 0 || len == strlen(defaultText))
        {
            SetWindowText(hwndEdit, "");
        }
    }
    else
    {
        // 失去焦点
        if (len == 0)
        {
            SetWindowText(hwndEdit, defaultText);
        }
    }
}

// 检查 IP 地址是否合法
bool isValidIPv4(const char *ip)
{
    int num, dots = 0;
    char *ptr;

    if (ip == NULL)
    {
        return false;
    }

    char *ip_copy = strdup(ip); // 复制 IP 地址字符串，因为 strtok 会修改原字符串

    if (ip_copy == NULL)
    {
        return false; // 内存分配失败
    }

    ptr = strtok(ip_copy, "."); // 使用 strtok 分割字符串

    if (ptr == NULL)
    {
        free(ip_copy);
        return false;
    }

    while (ptr)
    {
        // 检查每段是否为数字
        if (!isdigit(*ptr))
        {
            free(ip_copy);
            return false;
        }

        num = atoi(ptr); // 将字符串转换为数字

        // 检查数字是否在 0 到 255 之间
        if (num >= 0 && num <= 255)
        {
            ptr = strtok(NULL, "."); // 获取下一个子串
            if (ptr != NULL)
            {
                dots++; // 计数点的个数
            }
        }
        else
        {
            free(ip_copy);
            return false;
        }
    }

    free(ip_copy);

    // IP 地址应该包含 3 个点
    if (dots != 3)
    {
        return false;
    }

    return true;
}

// 检查文件保存位置是否合法
bool isValidSaveLocation(const char *path)
{
    DWORD attr;

    // 检查路径长度
    if (strlen(path) > MAX_PATH)
    {
        return false;
    }

    // 检查路径是否存在
    attr = GetFileAttributes(path);
    if (attr == INVALID_FILE_ATTRIBUTES)
    {
        return false; // 路径不存在
    }

    // 检查是否为目录
    if (!(attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        return false; // 不是一个目录
    }

    // 检查写入权限（尝试在该路径下创建然后删除一个临时文件）
    char tempFilePath[MAX_PATH];
    snprintf(tempFilePath, MAX_PATH, "%s\\tempfile.tmp", path);
    HANDLE hFile = CreateFile(tempFilePath, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return false; // 无法写入
    }
    CloseHandle(hFile);
    DeleteFile(tempFilePath); // 清理临时文件
    return true;
}

// 移除字符串末尾的换行符
void trimTrailingSpaces(char *str)
{
    int length = strlen(str);
    while (length > 0 && (str[length - 1] == '\n' || str[length - 1] == '\r' || str[length - 1] == ' '))
    {
        str[length - 1] = '\0';
        length--;
    }
}

// 从列表框中获取文件路径
void SelectFileAndAddToList(HWND hwndList)
{
    OPENFILENAME ofn;
    char szFile[260] = {0};

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwndList;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "All Files\0*.*\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn) == TRUE)
    {
        // 添加文件路径到列表
        SendMessage(hwndList, LB_ADDSTRING, 0, (LPARAM)szFile);
    }
}

// 从列表框中获取文件夹路径
void SelectFolderAndAddToList(HWND hwndList)
{
    BROWSEINFO bi = {0};
    bi.lpszTitle = "Select a folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.hwndOwner = hwndList;

    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);

    if (pidl != 0)
    {
        TCHAR path[MAX_PATH];
        if (SHGetPathFromIDList(pidl, path))
        {
            // 添加文件夹路径到列表
            SendMessage(hwndList, LB_ADDSTRING, 0, (LPARAM)path);
        }
        CoTaskMemFree(pidl);
    }
}

// 发送文件
void sendFile(SOCKET serverSocket, const char *filePath)
{
    FILE *file;
    char buffer[BUFFER_SIZE];
    int bytes_read;
    const char *fileName;

    // Extract file name from file path
    fileName = strrchr(filePath, '\\') ? strrchr(filePath, '\\') + 1 : filePath;

    // MessageBox(NULL, fileName, "调试信息", MB_OK);

    char tipsText[512];
    snprintf(tipsText, sizeof(tipsText), "您即将上传文件：%s，是否继续", fileName);

    // MessageBox(NULL, tipsText, "提示信息", MB_OKCANCEL);
    if (MessageBox(NULL, tipsText, "提示信息", MB_OKCANCEL) == IDCANCEL)
    {
        MessageBox(NULL, "取消上传", "提示信息", MB_OK);
        return;
    }

    // Send file name first
    send(serverSocket, fileName, strlen(fileName), 0);

    // Open file for reading
    file = fopen(filePath, "rb");
    if (file == NULL)
    {
        char tipsText[512];
        sprintf(tipsText, "Failed to open file: %s\n", filePath);
        ErrorHandling(tipsText);
        return;
    }

    // Read and send file content
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0)
    {
        send(serverSocket, buffer, bytes_read, 0);
    }

    // Close file
    fclose(file);

    sprintf(tipsText, "文件：%s上传完成", fileName);
    MessageBox(NULL, tipsText, "提示信息", MB_OK);
    // MessageBox(NULL, "文件上传完成", "提示信息", MB_OK);
}

// 用于处理错误的函数，写入.log文件，以追加的方式，带有时间戳
void ErrorHandling(char *message)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    FILE *fp = fopen("error.log", "a");
    fprintf(fp, "[%d-%d-%d %d:%d:%d]\t%s\n", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, message);
    fclose(fp);
}

// 传输文件到服务器
void transferFileToServer(const char *serverAddress, int port, const char *filePath)
{
    WSADATA wsaData;
    SOCKET serverSocket;
    struct sockaddr_in server;
    int result;

    // char message[128];

    // Initialize Winsock
    result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0)
    {
        char tipsText[512];
        sprintf(tipsText, "WSAStartup failed: %d\n", result);
        ErrorHandling(tipsText);
        return;
    }

    // Create socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET)
    {
        char tipsText[512];
        sprintf(tipsText, "Could not create socket: %d\n", WSAGetLastError());
        ErrorHandling(tipsText);
        WSACleanup();
        return;
    }

    // Set up server address and port
    server.sin_addr.s_addr = inet_addr(serverAddress);
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    // Connect to the server
    if (connect(serverSocket, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        ErrorHandling((char *)"Connect error");
        // puts("Connect error");
        closesocket(serverSocket);
        WSACleanup();
        return;
    }

    // Send file
    sendFile(serverSocket, filePath);

    // Close socket
    closesocket(serverSocket);

    // Cleanup Winsock
    WSACleanup();
}

// 文件上传线程
DWORD WINAPI FileUploadThread(LPVOID lpParam)
{
    UploadParams *params = (UploadParams *)lpParam;

    // 从参数中获取服务器 IP 和文件路径
    const char *serverIP = params->serverIP;
    const char *filePath = params->filePath;

    // 调用文件传输函数
    transferFileToServer(serverIP, PORT, filePath);

    // 可选：更新UI或通知主线程
    PostMessage(params->hwnd, WM_USER + 2, 0, 0);

    // 释放分配的内存
    delete params;

    return 0;
}

//将接收到的文件路径添加到列表框中
void AddFilePathToListBox(HWND hwndList, const char *filePath)
{
    // 获取文件名
    const char *fileName = strrchr(filePath, '\\') ? strrchr(filePath, '\\') + 1 : filePath;

    // 添加文件名到列表框
    SendMessage(hwndList, LB_ADDSTRING, 0, (LPARAM)fileName);
}

DWORD WINAPI FileReceiveThread(LPVOID lpParam)
{
    HWND hwnd = (HWND)lpParam;

    WSADATA wsaData;
    SOCKET listeningSocket, clientSocket;
    struct sockaddr_in serverAddr;
    int iResult;
    char buffer[1024];
    char receivedFilePath[MAX_PATH];
    int bytesReceived;

    // 初始化 Winsock
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // 创建套接字
    listeningSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listeningSocket == INVALID_SOCKET)
    {
        // 错误处理
        WSACleanup();
        return 1;
    }

    // 绑定套接字
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    iResult = bind(listeningSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr));
    if (iResult == SOCKET_ERROR)
    {
        // 错误处理
        closesocket(listeningSocket);
        WSACleanup();
        return 1;
    }

    // 监听
    listen(listeningSocket, SOMAXCONN);

    while (true) // 持续监听循环
    {
        // 接受连接
        clientSocket = accept(listeningSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET)
        {
            continue; // 如果接受失败，则继续监听
        }

        bytesReceived = recv(clientSocket, receivedFilePath, MAX_PATH, 0);
        if (bytesReceived > 0)
        {
            receivedFilePath[bytesReceived] = '\0'; // 确保字符串结尾

            // 构建完整的文件保存路径
            char fullPath[MAX_PATH];
            snprintf(fullPath, MAX_PATH, "%s\\%s", saveLocation, receivedFilePath); // 注意路径连接

            // 创建文件
            FILE *file = fopen(fullPath, "wb");
            if (file != NULL)
            {
                // 循环接收数据，直到连接关闭
                while ((bytesReceived = recv(clientSocket, buffer, 1024, 0)) > 0)
                {
                    fwrite(buffer, 1, bytesReceived, file);
                }
                fclose(file);

                // 发送消息到主窗口更新列表框
                PostMessage(hwnd, WM_USER + 1, (WPARAM)strdup(fullPath), 0);
            }
        }

        closesocket(clientSocket); // 完成后关闭客户端套接字
    }

    // 清理（永远不会执行，但为了代码完整性保留）
    closesocket(listeningSocket);
    WSACleanup();

    return 0;
}

