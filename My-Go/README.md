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

## 开启调试日志
默认情况下程序不会记录 KataGo 通信日志。如需开启：

1. 打开 `finalproject_6.0.cpp`，找到全局变量 `debugLogEnabled`，将其值设置为 `1`。
2. 重新编译程序并运行。启用后，程序将在运行目录生成 `kata_go.log`，记录与引擎的交互用于调试。

注意：调试日志可能包含本机路径或部分运行信息，仅用于本地调试，请在发布版关闭该开关。

## 发布说明
- 请勿在仓库中提交大型模型或第三方二进制文件。若需要分发模型或可执行，请考虑 GitHub Releases 或外部存储。
- 在发布前确认 `.gitignore` 中包含了需要排除的二进制与数据文件。

## 开发者说明
- 代码使用 EasyX 进行渲染，Windows 平台优先支持。编译器推荐 MinGW-w64（g++）。
- 主要源文件：`finalproject_6.0.cpp`、`easyx_compat.cpp`。资源文件（可选）：`image0.jpg`、`title.png`。

---

Author: 刘浩宇 <251830178@smail.nju.edu.cn>
