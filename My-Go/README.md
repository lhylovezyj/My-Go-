# My Go !

桌面围棋程序，基于 EasyX 实现图形界面，内含对弈记录与引擎对弈支持。

## 功能概述
- 可视化棋盘与信息面板
- 人机对弈与人人对弈模式
- 可配置外部围棋引擎路径（可选，用于扩展人机对弈能力）

## 编译（Windows，MinGW）
示例（请根据本机路径调整）：

```powershell
E:\mingw64\bin\g++.exe -g -finput-charset=UTF-8 -fexec-charset=GBK -Ithird_party/easyx/include -Lthird_party/easyx/lib easyx_compat.cpp finalproject_6.0.cpp -leasyx -lcomdlg32 -o finalproject_6.0.exe
```

`build.bat` 提供了一个可参考的构建命令样例。

## 许可证
请参阅 `LICENSE` 文件了解授权信息。

## 开发者说明
- 代码使用 EasyX 进行渲染，Windows 平台优先支持。编译器推荐 MinGW-w64（g++）。
- 主要源文件：`finalproject_6.0.cpp`、`easyx_compat.cpp`。资源文件（可选）：`image0.jpg`、`title.png`。

---

Author: 刘浩宇 <251830178@smail.nju.edu.cn>
