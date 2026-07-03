# Release and update notes

这份文档记录 ReArk 发布说明、更新检查和更新预览的维护规则。它面向维护者，不放在 README 中。

## GitHub Release 内容格式

更新窗口会读取 GitHub latest release API 返回的原始 `body` 字段。为了支持中英双语，不要把英文和中文顺序堆在同一份 Markdown 里；应使用 ReArk 约定的语言块，让应用按当前界面语言只显示对应内容。

推荐格式：

```markdown
<!-- reark:lang=en -->
Include following updates:

**1. ReArk Agent upgrades**
- Added ...
<!-- /reark:lang -->

<!-- reark:lang=zh-CN -->
包含以下更新：

**1. ReArk Agent 升级**
- 新增 ...
<!-- /reark:lang -->
```

规则：

- 语言块开始标记为 `<!-- reark:lang=<locale> -->`。
- 语言块结束标记为 `<!-- /reark:lang -->`。
- `<locale>` 支持 `en`、`en-US`、`zh-CN`、`zh_CN` 这类写法；解析时会统一大小写，并把 `_` 视为 `-`。
- 当前界面语言为中文时，优先选择 `zh-CN` 或 `zh`。
- 当前界面语言为英文时，优先选择 `en-US` 或 `en`。
- 找不到当前语言时，回退到英文块。
- 如果没有英文块，回退到第一个有效语言块。
- 如果 release body 没有任何语言块，则保留旧行为，直接显示完整 body。

GitHub Release 页面会把 HTML 注释隐藏起来，但 GitHub API 的 `body` 字段会保留这些注释，所以应用端可以稳定解析这些标记。

## 更新窗口文案

更新窗口顶部只展示一行版本摘要：

```text
新版本：ReArk 0.2.1 | 发布日期：2026-07-02
```

实际 QML 使用富文本让版本号加粗。该摘要、按钮、空更新日志提示都必须走 `qsTr`，不要在 QML 中写死中文。

正式更新内容来自 GitHub Release body，不应在应用中硬编码。

## 更新检查路径

正式更新检查只访问：

```text
https://api.github.com/repos/lkimuk/ReArk/releases/latest
```

应用读取以下字段：

- `tag_name`：最新版本号。
- `html_url`：发布页面地址。
- `body`：发布说明 Markdown。
- `published_at`：发布日期。

只有当 `tag_name` 高于当前 `QCoreApplication::applicationVersion()` 时，才弹出更新窗口。

## 调试更新预览

更新预览只用于本地调 UI，不属于正式更新流程。

默认情况下，预览入口是隐藏的。需要调试时，在 Debug 构建中设置环境变量后启动：

```powershell
$env:REARK_UPDATE_PREVIEW="1"
.\build\Debug\ReArk.exe
```

随后可在菜单中打开：

```text
帮助 -> 预览可用更新
```

可识别的开启值：

```text
1
true
yes
on
```

约束：

- Release 构建中预览入口始终不可用。
- Release 构建中不应包含预览用的假版本号或假更新正文。
- 预览数据只能用于检查窗口布局、按钮样式、滚动区域和 Markdown 渲染。
- 提交发布前应确认正式检查更新仍然走 GitHub latest release API，而不是预览路径。

## 发布前检查

发布前至少检查：

- GitHub Release body 是否使用了正确语言块。
- 中文和英文内容是否分别位于对应语言块内。
- 没有把中文追加到英文正文末尾，避免中文用户需要滚到底部才能看到。
- 更新窗口新增文案是否已进入 `resources/translations/reark_zh_CN.ts`。
- `reark_release_notes_localizer_test` 通过。
- Debug/Release 构建通过。
- Release 可执行文件中不包含预览更新正文。
