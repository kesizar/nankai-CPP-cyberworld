# AI-MUD Cyberworld

基于 **Qt 6** 的桌面文字冒险游戏：赛博朋克题材界面，剧情由 **DeepSeek** 大模型驱动，玩家通过输入行动推进故事，并伴随 HP、人性值、背包与义体等状态管理。

## 环境要求

| 依赖 | 说明 |
|------|------|
| CMake | 3.16 及以上 |
| C++ 编译器 | 支持 **C++17**（Windows 上常用 Visual Studio / MSVC） |
| Qt 6 | 模块：`Core`、`Gui`、`Widgets`、`Network` |
| 网络 | 运行时需要能访问 DeepSeek API（`https://api.deepseek.com`） |

## 克隆仓库

```bash
git clone <你的仓库 HTTPS或 SSH 地址>
cd <克隆生成的项目文件夹>
```

将第一行中的地址换成你在 GitHub 上该仓库的实际 URL；第二行目录名以克隆后本地文件夹名为准。

## 本地构建（CMake）

在**项目根目录**（含 `CMakeLists.txt` 的目录）执行：

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="<Qt6 安装路径>"
cmake --build build --config Release
```

**Windows 说明：**

- `CMAKE_PREFIX_PATH` 需指向 Qt 6 的安装根目录（例如 `C:/Qt/6.x.x/msvc2019_64`），以便 `find_package(Qt6 …)` 能找到 Qt。
- 若使用 Visual Studio 生成器，也可用 CMake GUI 或 VS打开 `build` 目录中的解决方案进行编译。

编译成功后，可执行文件通常在 `build/Release/cyberworld.exe`（单配置生成器可能是 `build/cyberworld.exe`，以实际输出为准）。

## 配置 API Key（必须）

仓库**不包含**密钥文件（`config.json` 已被 `.gitignore` 忽略）。首次运行前，在**与可执行文件同一目录**下新建 `config.json`，例如：

```json
{
  "api_key": "在此填入你的 DeepSeek API Key"
}
```

也支持字段名 `apiKey`。密钥请向 [DeepSeek 开放平台](https://platform.deepseek.com/) 申请，**切勿**将含真实 Key 的 `config.json` 提交到 Git。

## 运行

1. 确认 `config.json` 已放在与 `cyberworld`（或 `cyberworld.exe`）**相同目录**。
2. 双击运行或在终端启动该可执行文件。
3. 若 LLM 无法连接，请检查网络与 API Key 是否有效。

## 存档

游戏会在可执行文件目录下读写 `savegame.json`（默认同样被 Git 忽略）。换电脑或重装后不会自动带上旧存档，需自行备份该文件。
