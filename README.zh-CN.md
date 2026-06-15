# ReArk

[English](README.md) | 简体中文

ReArk 是面向 HarmonyOS NEXT HAP/APP/ABC 文件的逆向分析工具，核心能力包括包结构浏览、Ark 字节码反汇编与反编译、资源预览、签名检查、搜索，以及可选的 AI 辅助分析。

## 概览

ReArk 面向合法授权的应用分析与安全研究场景。它提供桌面界面，用于打开 HarmonyOS 包、浏览反编译结果、检查资源文件，并围绕当前应用进行上下文问答。

当前版本：`0.1.0`

## 功能

- 打开 `.hap`、`.app` 和 `.abc` 文件。
- 浏览包结构、源码、资源和签名信息。
- 查看反编译源码、反汇编、格式化 JSON、图片、媒体、文本和十六进制内容。
- 在包文件树中搜索和快速打开文件。
- 检查包签名和证书信息。
- 在英文、简体中文和系统语言之间切换界面语言。
- 支持深色、浅色和系统主题。
- 可选使用 ReArk Agent 进行智能分析，并配置模型与知识库设置。

## 快速开始

### 安装

下载并运行 Windows 安装器：

[ReArk-0.1.0-windows-x64-setup.exe](https://github.com/lkimuk/ReArk/releases/download/v0.1.0/ReArk-0.1.0-windows-x64-setup.exe)

## ReArk Agent

ReArk Agent 是可选功能。它可以围绕当前打开的应用回答问题、总结分析结果，并在配置知识索引后使用附加参考文档。

Agent 的用户侧回答不会暴露内部工具名称或实现细节。它的 Markdown 输出也遵循兼容性规范，避免使用数字键帽 emoji 等不稳定组合序列。

## 安全与隐私

ReArk 仅应用于合法授权的逆向工程、互操作研究、恶意样本分析和安全研究。请勿将其用于未授权绕过、攻击或数据提取。

使用 ReArk Agent 时，请避免向远程模型服务提供密钥、证书、用户数据、商业秘密或其他敏感内容。

## 许可

ReArk 使用 Apache License 2.0 许可。详情见 [LICENSE](LICENSE)。

第三方声明见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

## 支持

- 问题反馈：[GitHub Issues](https://github.com/lkimuk/ReArk/issues)
- 使用指南：[cppmore.com/ReArk](https://www.cppmore.com/category/ReArk/)
