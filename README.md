# ReArk

English | [简体中文](README.zh-CN.md)

ReArk is a reverse engineering tool for HarmonyOS NEXT HAP/APP/ABC files. It focuses on package browsing, Ark bytecode disassembly and decompilation, resource preview, signature inspection, search, and optional AI-assisted analysis.

## Overview

ReArk is built for legally authorized application analysis and security research. It provides a desktop UI for opening HarmonyOS packages, navigating decompiled output, inspecting resources, and asking context-aware questions about the current app.

## Features

- Open `.hap`, `.app`, and `.abc` files.
- Browse package structure, source files, resources, and signatures.
- View decompiled source, disassembly, formatted JSON, images, media, text, and hex content.
- Search and quick-open files from the package tree.
- Inspect package signature and certificate information.
- Switch UI language between English, Simplified Chinese, and system language.
- Use dark, light, or system theme.
- Use ReArk Agent for optional smart analysis with configurable model and knowledge settings.

## Quick Start

### Installation

Download and run the Windows installer:

[ReArk-0.1.0-windows-x64-setup.exe](https://github.com/lkimuk/ReArk/releases/download/v0.1.0/ReArk-0.1.0-windows-x64-setup.exe)

## ReArk Agent

ReArk Agent is optional. It can answer questions about the currently opened app, summarize findings, and use attached reference documents when knowledge indexing is configured.

The Agent is designed to avoid exposing internal tool names or implementation details in user-facing answers. Its Markdown output also follows a compatibility policy that avoids unstable emoji composition sequences such as keycap emoji.

## Safety and Privacy

Use ReArk only for legally authorized reverse engineering, interoperability work, malware analysis, and security research. Do not use it for unauthorized bypass, attacks, or data extraction.

When using ReArk Agent, avoid sharing secrets, certificates, user data, trade secrets, or other sensitive content with remote model providers.

## License

ReArk is licensed under the Apache License 2.0. See [LICENSE](LICENSE) for details.

Third-party notices are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Support

- Issues: [GitHub Issues](https://github.com/lkimuk/ReArk/issues)
- User guide: [cppmore.com/ReArk](https://www.cppmore.com/category/ReArk/)
