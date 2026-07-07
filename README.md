# ReArk

English | [简体中文](README.zh-CN.md)

ReArk is a reverse engineering tool for HarmonyOS NEXT HAP/APP/ABC files. It focuses on package browsing, Ark bytecode disassembly and decompilation, resource preview, signature inspection, search, and optional AI-powered intelligent analysis.

## Overview

ReArk is built for legally authorized application analysis and security research. It provides a desktop UI for opening HarmonyOS packages, navigating decompiled output, inspecting resources, and asking context-aware questions about the current app.

## Features

- Open `.hap`, `.app`, and `.abc` files.
- Browse package structure, source files, resources, and signatures.
- View decompiled source, disassembly, formatted JSON, images, media, text, and hex content.
- Search and quick-open files from the package tree.
- Inspect package signature and certificate information.
- Use ReArk Agent for contextual app analysis with model provider presets, tool-assisted package inspection, and reference knowledge indexing.
- Navigate a desktop UI designed for reverse engineering workflows.

## Screenshots

| Workspace overview | Ark disassembly |
| --- | --- |
| <img src="assets/screenshots/01-overview.png" alt="ReArk workspace overview" width="420"> | <img src="assets/screenshots/02-disassembly.png" alt="Ark bytecode disassembly" width="420"> |

| ABC / Hex inspection | Decompiled pages |
| --- | --- |
| <img src="assets/screenshots/03-abc-hex-view.png" alt="ABC and Hex inspection" width="420"> | <img src="assets/screenshots/04-pages-discompile.png" alt="Decompiled page source" width="420"> |

| ReArk Agent | Agent analysis |
| --- | --- |
| <img src="assets/screenshots/05-ReArk-agent.png" alt="ReArk Agent workspace" width="420"> | <img src="assets/screenshots/06-ReArk-agent-analysis.png" alt="ReArk Agent analysis result" width="420"> |

## Quick Start

### Installation

Download and run the Windows installer:

[ReArk-0.1.0-windows-x64-setup.exe](https://github.com/lkimuk/ReArk/releases/download/v0.1.0/ReArk-0.1.0-windows-x64-setup.exe)

## ReArk Agent

ReArk Agent brings model-assisted reverse engineering directly into the workspace. It can analyze the currently opened application, inspect package metadata and file content on demand, read relevant decompiled source or disassembly, and produce structured answers from the same context you are viewing in ReArk.

It supports multiple model providers and deployment styles, including OpenRouter, OpenAI, OpenAI-compatible endpoints, Anthropic, Gemini, Ollama, DeepSeek, DashScope, and Qwen. Provider presets include default endpoints and recommended models, while advanced users can override base URLs, model names, API key requirements, and embedding settings.

ReArk Agent also supports reference knowledge indexing. You can attach documents such as Markdown, text, HTML, JSON, CSV, PDF, DOCX, PPTX, and XLSX files, then ask questions that combine package context with your own reference material.

The Agent is designed for product-facing answers: it avoids exposing internal tool names or implementation details, follows the user's language, and keeps Markdown output compatible with ReArk's renderer.

### Agent Optimization Notes

Recent Agent work has focused on making analysis faster, less repetitive, and more resilient during long reverse-engineering tasks.

Implemented:

- Task routing separates static analysis, static CTF-style cracking, and device runtime workflows.
- Device runtime tools are exposed only when the latest request explicitly asks for installation, launch, HDC, logs, screenshots, UI automation, signing, or runtime verification.
- Conversation context is denoised and compressed so old device/signing output does not pollute static analysis tasks.
- A durable in-chat scratchpad lets the Agent save intermediate constants, candidate answers, script output, unresolved offsets, and next steps across follow-up turns.
- Reusable Python analysis state lets the Agent preserve constants, helper functions, byte arrays, and extracted data across local analysis script calls.
- Local Python analysis is encouraged for deterministic decoding, hashing, byte conversion, brute-force checks, and repeated string transforms instead of doing arithmetic in natural language.
- The bounded Python execution timeout has been raised for larger static calculations.
- ABC reference-flow analysis can combine literal resolution, cross-references, and call-argument flow evidence in one tool call.
- Reasoning budgets are split by task profile instead of using one global budget for every workflow.

Planned next:

- Persist the Agent scratchpad to disk per chat or per loaded package, with explicit reset and cleanup behavior.
- Add higher-level compound tools for common CTF workflows, such as finding an encryption routine, extracting constants, running a verifier script, and returning the candidate answer.
- Add Agent run telemetry for task mode, tool-call count, elapsed time, budget-stop reason, scratchpad usage, and final outcome.
- Build regression samples for static CTF analysis and compare solve time, tool-call count, and success rate across Agent changes.
- Continue tuning source/disassembly chunking and evidence summaries to reduce repeated reads without hiding important context.

## Safety and Privacy

Use ReArk only for legally authorized reverse engineering, interoperability work, malware analysis, and security research. Do not use it for unauthorized bypass, attacks, or data extraction.

When using ReArk Agent, avoid sharing secrets, certificates, user data, trade secrets, or other sensitive content with remote model providers.

## License

ReArk is licensed under the Apache License 2.0. See [LICENSE](LICENSE) for details.

Third-party notices are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Support

- Issues: [GitHub Issues](https://github.com/lkimuk/ReArk/issues)
- User guide: [cppmore.com/ReArk](https://www.cppmore.com/category/ReArk/)
