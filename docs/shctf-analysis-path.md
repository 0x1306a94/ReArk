# SHCTF 样本正确分析路径

这份文档记录 SHCTF HarmonyOS 样本的正确分析链路，用于复盘和实测 ReArk Agent。它是样本级别的分析记录，不应被写入 Agent 的通用产品逻辑。

## 目标

对当前 HAP 样本完成以下闭环：

- 静态定位口令校验逻辑。
- 从 ABC迹索中提取 `secretKey` 和编码常量。
- 用脚本复算口令，避免手算常量。
- 用 `encode(candidate) == secretKey` 做静态正确性证明。
- 连接设备安装、启动、输入并验证运行态结果。
- 当长口令或特殊字符无法可靠输入时，使用 verifier patch 验证成功分支，但必须区分“成功分支验证”和“原始口令精确输入验证”。

## 正确入口

优先看应用入口页和相关模块：

- `Index.ets`：页面、输入框、按钮、口令校验、成功/失败提示。
- `encode.ets`：口令编码函数。
- `maze.ets`：校验通过后的迷宫/文件生成逻辑。
- `modules.abc` 或对应 ABC 反汇编：确认源码不可见或源码不可信时的真实常量和调用关系。

不要一开始就安装运行或盲试输入。这个样本的关键是静态恢复 verifier 和 encode 公式。

## 静态证据路径

1. 在 `Index.ets` 或反汇编中定位按钮回调。
2. 确认用户输入会调用 `encode(passwd)`。
3. 确认 `encode(passwd)` 的结果与硬编码 `secretKey` 比较。
4. 比较通过后调用 `maze.CreateMaze(passwd)`。
5. 比较失败时只提示口令错误。

关键结论：

```text
success condition:
encode(input_password) == secretKey

success branch:
maze.CreateMaze(input_password)
Toast: 口令正确！
generated artifact: filesDir 下的 What
```

## 必须从 ABC 精确提取的值

`secretKey`：

```text
r.k)4|)k[.)cq.#kkqPPq4UJq)UJ.w4qDUDr9fP??)P9D)P#?ff)kw[#PPqwU)?wqrD#r|k4[k.?|rD[[4Jq#k)|fr49DUJ|Pf4D#Jrr9PqP|J.9qJUrfD4|UJq|[9r4UU?[#wPk#)9D#P.JDUwtP)P,f?P4fPJ)rrswqJ4r.zuq).PL9
```

`encode.ets` 相关常量：

| 名称 | 十六进制 | 十进制 | 用途 |
| --- | --- | ---: | --- |
| `v1` | `0x5f` | `95` | 可打印 ASCII 范围大小 |
| `v2` | `0x1d4b42` | `1919810` | 加法常量 |
| `v3` | `0x1bf52` | `114514` | 乘法常量 |
| `v4` | `0x20` | `32` | 可打印 ASCII 起点 |
| `v5` | `0x0` | `0` | `charCodeAt` 参数 |

注意：`0x1d4b42` 的正确十进制是 `1919810`。把它手算成 `1917762` 会得到完全错误的口令。这个样本必须要求 Agent 使用脚本或结构化工具转换常量。

## 编码公式

从 ABC 语义得到：

```text
encoded = ((114514 * (c - 32) + 1919810) % 95) + 32
```

化简模 `95`：

```text
114514 % 95 = 39
1919810 % 95 = 50
encoded = ((39 * (c - 32) + 50) % 95) + 32
```

`39` 在模 `95` 下的逆元仍为 `39`：

```text
39 * 39 = 1521
1521 % 95 = 1
```

解码公式：

```text
c = (39 * ((ord(e) - 82) % 95) % 95) + 32
```

其中 `e` 是 `secretKey` 中的编码字符。

## 正确复算脚本

使用任意受控脚本环境复算，不要自然语言手算：

```python
secret = "r.k)4|)k[.)cq.#kkqPPq4UJq)UJ.w4qDUDr9fP??)P9D)P#?ff)kw[#PPqwU)?wqrD#r|k4[k.?|rD[[4Jq#k)|fr49DUJ|Pf4D#Jrr9PqP|J.9qJUrfD4|UJq|[9r4UU?[#wPk#)9D#P.JDUwtP)P,f?P4fPJ)rrswqJ4r.zuq).PL9"

def decode_char(e: str) -> str:
    return chr((39 * ((ord(e) - 82) % 95) % 95) + 32)

def encode_char(c: str) -> str:
    return chr(((114514 * (ord(c) - 32) + 1919810) % 95) + 32)

password = "".join(decode_char(ch) for ch in secret)
assert len(password) == len(secret) == 177
assert "".join(encode_char(ch) for ch in password) == secret
print(password)
```

正确口令：

```text
-590a709b50}e5c99e11ea6de06d52ae868-f413301f801c344092bc11e26032e-8c-79ab9537-8bbadec9074-af86d714a8cd--f1e17d5fed6-48a76de7bf-a663bc219c0f8c15d862{101F431a41d0--T2eda-5HCe051Sf
```

这一步的验收标准不是“看起来像 flag”，而是脚本断言：

```text
encode(password) == secretKey
len(password) == 177
```

## 设备验证路径

运行态验证应该按闭环执行：

1. 列出 HDC 设备，确认目标在线。
2. 安装当前 HAP；如签名不匹配，走重签名/包名重写后安装。
3. 启动 bundle/ability，确认应用进入前台。
4. dump UI layout，确认输入框和按钮：
   - 口令输入框：`Password` 类型，placeholder 类似“请输入口令”。
   - 检查按钮：文本“检查”。
5. 输入口令并点击检查。
6. 用运行态证据确认结果：
   - Toast 出现“口令正确！”。
   - 失败时 Toast 为“口令错误！”。
   - 成功分支会生成 `What` 文件。
   - 实际文件路径要从运行态或代码证据确认，不要猜。

该样本实测成功时的文件路径为：

```text
/data/app/el2/100/base/com.example.arks/haps/entry/files/What
```

该路径是样本实测结果，不是通用规则。其他样本必须重新从 bundle、module、runtime 输出或代码中确认路径。

## 输入链路注意事项

这个样本最终口令长度为 `177`，并包含 `{`、`}` 等 shell/键盘链路容易出问题的字符。运行态输入时必须遵守：

- 输入工具成功只代表事件被设备接受，不代表 Password 字段收到完整文本。
- Password 字段通常无法从 UI layout 直接读回明文。
- 对可读输入框，可以 dump layout 比较文本。
- 对 Password 输入框，必须用 Toast、日志、文件生成或业务状态确认。
- 不要因为命令返回成功就宣称“口令正确”。

如果原始长口令无法可靠输入，正确降级路径是 verifier patch：

1. 静态证明原始口令正确。
2. 选一个短安全输入，例如只含 ASCII 字母数字。
3. 用原始 `encode` 公式计算短输入的 encoded verifier。
4. 将 ABC 中的旧 `secretKey` 精确替换为新 encoded verifier。
5. 重打包、重签名、安装。
6. 输入短安全值，点击检查。
7. 用 Toast/文件/日志证明成功分支可达。
8. 最终报告中明确说明：patch 验证证明成功分支和 artifact 生成路径，不等价于证明原始 177 字符口令已被设备精确输入。

## 常见错误路径

不要这样做：

- 手动把 `0x1d4b42` 转成十进制。
- 只看源码文本，不用 ABC literal/xref/call-flow 迹索确认。
- 算出口令后不执行 `encode(candidate) == secretKey` 复核。
- 把 `inputText` 或 `uinput` 返回成功当作业务成功。
- 对 Password 字段要求 UI layout 回显明文。
- 猜测 filesDir 路径。
- 在静态公式和运行结果冲突时继续盲试输入变体。
- 把 verifier patch 成功描述成“原始口令设备输入已验证”。

## 本次 Agent 失败复盘

这次和 Agent 交流暴露出的失败点如下，后续实测时应逐项检查：

1. 常量手算错误。
   - Agent 把 `0x1d4b42` 错算成 `1917762`。
   - 正确值是 `1919810`。
   - 这类值必须由结构化 ABC迹索或脚本输出得到。

2. 没有保持上下文连续。
   - 前面已经提到 Python 能力、候选公式和候选口令，后面仍反复说“没有 Python 环境”或重新从头推导。
   - 关键常量、公式、候选口令、验证结果必须写入 scratchpad 或 Python session，并在继续分析前读取。

3. 没有形成静态验收闭环。
   - 输出报告时一度只给脚本让用户本地运行。
   - 正确完成标准必须包含实际候选口令和 `encode(candidate) == secretKey` 的断言结果。

4. 公式摇摆但没有证据化纠错。
   - Agent 在 `charCode - 32` 和 `32 - charCode` 之间摇摆。
   - 如果推翻前一个公式，必须说明来自哪条 ABC 指令语义、哪个结构化证据，并重新跑正向 verifier。

5. 设备验证证据不足。
   - “Toast node mount”“应用没崩溃”“Password 显示星号”都不是成功证据。
   - 必须拿到语义证据：成功 Toast 文本、生成的 `What` 文件、文件内容、明确日志或 UI 状态。

6. patch 验证结论过度。
   - 将 `secretKey` patch 成短输入的 encoded verifier 可以证明成功分支可达。
   - 它不能证明原始 177 字符口令已经通过设备输入链路精确传入。

7. 最终答案不彻底。
   - 如果已经能计算口令，就不能最后仍说“请本地运行脚本解码”。
   - 如果验证没完成，要明确剩余 blocker，而不是写成完整破解报告。
   - 如果当前只完成静态证明，最终答案底部必须明确“运行态未验证”，并给出下一步设备安装、输入、Toast/文件/日志证据验证路径。
   - 如果用户已经要求“验证/检验/跑通”或当前任务处于设备验证阶段，Agent 不应停在静态报告，而应继续连接设备完成运行态闭环，除非工具返回了明确 blocker。

8. 第二轮实测仍出现的伪完成。
   - Agent 再次输出 `0x1D4B42 = 1917762`，说明提示词要求“不要手算”仍不足以约束模型。
   - Agent 给出的完整口令仍是错误口令，但写成“精确解码”。
   - Agent 计算 verifier patch 时继续基于错误常量，随后又把 Toast mount 当成成功证据。
   - 这类问题需要 host-side 质量闸门：最终答案中出现 `0x... = 十进制` 时由宿主复算；出现本地脚本甩锅或语义证据不足时显式标记为未完成。

9. 第三轮实测暴露的守卫和交互问题。
   - host-side 十六进制复算守卫不能把拆位说明里的 `d=13` 误判成 `0x1d4b42 = 13`；只应检查紧邻 hex 的十进制声明或表格数字列。
   - Agent 仍会说“没有 Python 执行环境”，说明最终答案守卫必须覆盖中文变体，而不仅是“无法执行 Python”。
   - Agent 用“随机抽查验证”包装完整候选值，仍不足以证明 177 字符 flag 正确；最终答案必须包含 `encode(candidate) == secretKey` 这种全量断言。
   - 有设备自动化能力时，不应把“完整 flag 设备验证”主要交给用户手动输入。若长 Password 输入无法保真，应明确“原始长输入精确传递未验证”，并优先报告自动化尝试、patch 成功分支、文件/日志/UI 状态等证据。

## Agent 实测验收标准

一次专业的 ReArk Agent 输出应该至少包含：

- 明确指出校验关系是 `encode(passwd) == secretKey`。
- 给出 `0x1d4b42 = 1919810`，且说明由工具/脚本得到。
- 给出可复现的编码/解码脚本。
- 断言 `encode(candidate) == secretKey`。
- 给出正确 177 字符口令。
- 设备验证时能区分：
  - 静态口令正确性。
  - 原始长口令是否成功输入。
  - patch 后短输入是否证明成功分支可达。
- 成功时用 Toast、日志、UI 状态或 `What` 文件作为证据。

如果 Agent 在任一关键点缺少证据，应当继续收集证据或如实说明未验证，而不是补叙事。
