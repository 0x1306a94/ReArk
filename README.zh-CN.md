# ReArk

[English](README.md) | 简体中文

ReArk 是一款面向 HarmonyOS NEXT 应用的专业逆向工具。能够分析 `.hap`、`.app`、`.abc` 文件，支持反汇编、反编译、字符串解析、交叉引用、签名分析、重签名、包体浏览、重打包、应用信息、Agent 智能化分析、真机操纵、迹索分析等功能。

ReArk 能够零门槛逆向分析鸿蒙应用，可以用来检测应用安全、自动挑战鸿蒙 CTF 比赛、逆向应用逻辑、自动化操纵鸿蒙手机、分析应用日志……如何使用，由你探索。

## 安装

下载并运行：

[ReArk-0.2.0-windows-x64-setup.exe](https://github.com/lkimuk/ReArk/releases/download/v0.2.0/ReArk-0.2.0-windows-x64-setup.exe)

## 功能亮点

**包体分析**

- 支持 HarmonyOS NEXT `.hap`、`.app` 和 `.abc` 文件
- 浏览包结构、模块、源码输出、资源、签名和元数据
- 检查签名和证书信息
- 搜索文件，从包文件树中快速打开重点路径

**静态分析**

- 在包上下文中查看 Ark 字节码反汇编和反编译源码
- 检查 ABC 字符串、十六进制内容、格式化 JSON、文本、图片和媒体资源
- ABC 迹索视图跟踪字节码引用、常量和相关代码路径
- 让分析过程围绕真实包体线索展开，减少临时笔记和上下文切换

**ReArk Agent**

- 围绕当前打开的包进行上下文问答
- 按需让 Agent 检查包体元数据、文件、字符串、反汇编和反编译输出
- 支持 OpenRouter、OpenAI、OpenAI-compatible endpoints、Anthropic、Gemini、Ollama、DeepSeek、DashScope 和 Qwen Provider 预设
- 可附加参考文档，结合自己的资料和笔记增强智能化分析能力
- 动态操纵真机设备，静态分析、安装应用、启动验证、追踪日志全流程分析应用

**真机连接**

- 发现连接鸿蒙设备
- 动态安装当前应用到设备
- 截图、检查 UI 节点，并把 UI 证据对应到真实设备状态
- 读取 Hilog，并执行点击、文本输入、返回、主页、滑动等基础 UI 操作

## 截图

| 工作区概览 | Ark 反汇编 |
| --- | --- |
| <img src="assets/screenshots/01-overview.png" alt="ReArk 工作区概览" width="420"> | <img src="assets/screenshots/02-disassembly.png" alt="Ark 字节码反汇编" width="420"> |

| ABC / Hex 检查 | 字符串 |
| --- | --- |
| <img src="assets/screenshots/03-abc-hex-view.png" alt="ABC 和 Hex 检查" width="420"> | <img src="assets/screenshots/04-strings.png" alt="ABC 字符串视图" width="420"> |

| ReArk Agent | Agent 分析 |
| --- | --- |
| <img src="assets/screenshots/05-ReArk-agent.png" alt="ReArk Agent 工作区" width="420"> | <img src="assets/screenshots/06-ReArk-agent-analysis.png" alt="ReArk Agent 分析结果" width="420"> |

| 设备运行态 | ABC 证据视图 |
| --- | --- |
| <img src="assets/screenshots/07-device-runtime.png" alt="HarmonyOS 设备运行态工作区" width="420"> | <img src="assets/screenshots/08-abc-evidance.png" alt="ABC 证据视图" width="420"> |


## 安全与隐私

ReArk 仅应用于合法授权的逆向工程、互操作研究、恶意样本分析和安全研究。

使用 ReArk Agent 连接远程模型服务时，请避免提交密钥、证书、用户数据、商业秘密或其他敏感内容。

## 许可

ReArk 使用 Apache License 2.0 许可。详情见 [LICENSE](LICENSE)。

第三方声明见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

## 支持

- 问题反馈：[GitHub Issues](https://github.com/lkimuk/ReArk/issues)
- 使用指南：[cppmore.com/ReArk](https://www.cppmore.com/category/ReArk/)
