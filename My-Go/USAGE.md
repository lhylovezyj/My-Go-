# Usage

快速说明：本仓库包含一个基于 EasyX 的桌面围棋程序。下面给出构建与运行的最小步骤（Windows，MinGW）：

构建示例（根据你的环境调整路径）：

```powershell
E:\mingw64\bin\g++.exe -g -finput-charset=UTF-8 -fexec-charset=GBK -Ithird_party/easyx/include -Lthird_party/easyx/lib easyx_compat.cpp finalproject_6.0.cpp -leasyx -lcomdlg32 -o finalproject_6.0.exe
```

运行：

1. 将资源文件（如 `image0.jpg`、`title.png`，如果使用）放在可执行文件同目录。
2. 双击 `finalproject_6.0.exe` 或在命令行中运行它。

可选：配置外部围棋引擎（例如 KataGo）以启用更强的 AI 对弈功能。程序会在当前目录生成 `kata_config.ini` 用于记录本地设置。

注：请不要将大型模型或第三方二进制直接提交到仓库，使用 `.gitignore` 已排除常见大文件类型。
