# ReArk 性能与加载架构优化记录

本文记录 ReArk 代码/资源预览区域的性能问题根因、已经完成的优化，以及后续要继续推进的方向.目标不是单点修补，而是逐步建立接近 jadx / VS Code 的加载、缓存、渲染和调度模型.

## 背景问题

用户反馈主要集中在两类体验：

- 代码/十六进制内容区域滚动卡顿，尤其是拖动滚动条时明显不丝滑.
- 打开 HAP 后点击文件仍然需要等待，不像 jadx 首次打开后后续文件基本秒开.

这两个问题表面上都是“慢”，但底层原因不同：

- 滚动卡顿主要来自渲染路径不够虚拟化，QML delegate / QTextDocument 全量布局成本过高.
- 点击文件等待主要来自加载架构偏懒加载，打开包时只建树，用户点到文件时才读资源或反编译.

## 已完成优化

### 1. Hyle 调用改为异步 API

ReArk 已接入 Hyle async API：

- `open_decompiled_package_async`
- `summary_async`
- `read_resource_async`
- `decompile_source_file_async`

相关实现集中在：

- `src/core/HyleDecompiler.h`
- `src/core/HyleDecompiler.cpp`
- `src/controller/DecompilerController.cpp`

当前做法：

- 后端持有长期 `SessionContext`，内部包含 Hyle executor、stop source 和 session.
- Hyle 任务通过后台线程执行，不在 Qt UI 线程同步等待.
- 切换文件、关闭包、重新打开样本时调用 `requestStop()` 取消旧任务.
- UI 更新回到 Qt 主线程，通过 `QFutureWatcher` 接收结果.

注意：单独 `.abc` 文件当前仍使用同步 `decompile_abc_sources`，但它运行在 `QtConcurrent` 后台线程，不阻塞 UI.后续如果 Hyle 提供 async `.abc` API，应继续迁移.

### 2. 代码视图改为虚拟绘制

旧路径基于 `QTextDocument` / `QTextCursor`，对长文件会产生较高布局和绘制成本.

当前已改为 `CodeEditorItem` 自绘：

- 使用固定行高和行起始索引.
- 只绘制可见行.
- 横向滚动时只绘制可见列范围.
- 语法高亮按行缓存，并限制同步追赶行数.
- 快速滚动期间跳过高亮，优先保证滚动帧率.

相关文件：

- `src/presentation/CodeEditorItem.h`
- `src/presentation/CodeEditorItem.cpp`
- `src/ui/qml/ReArk/CodeView.qml`

### 3. Hex 视图改为虚拟绘制

旧 `HexView.qml` 使用 `ListView` delegate，每行包含多个 QML 子项和 `MouseArea`.拖动滚动条时会出现大量 delegate 维护、布局和绘制压力，导致滚动条闪烁和内容卡顿.

当前已新增 `HexViewerItem`：

- C++ `QQuickPaintedItem` 自绘十六进制内容.
- 仅绘制可见行.
- 数据从 `HexDocumentModel::byteAt()` 按需读取.
- QML 层只保留 header、Flickable 和 ScrollBar.

相关文件：

- `src/presentation/HexViewerItem.h`
- `src/presentation/HexViewerItem.cpp`
- `src/model/HexDocumentModel.h`
- `src/model/HexDocumentModel.cpp`
- `src/ui/qml/ReArk/HexView.qml`

### 4. 修复 QQuickPaintedItem 渲染和 Flickable 锚定问题

优化过程中发现两个底层问题：

- 强制 `QQuickPaintedItem::FramebufferObject` 在当前 Qt Quick / RHI 路径下可能导致内容区域空白.
- `Flickable` 内部子项实际属于 `contentItem`，不能直接 `anchors.fill: flickable`.

当前处理：

- 移除强制 FBO render target，使用 Qt 默认 painted 后端.
- 内容 item 使用 viewport 固定尺寸，并通过 `x/y = contentX/contentY` 抵消 Flickable 内容偏移.

### 5. 文件内容缓存模型

`SourceTreeModel` 现在保存 `DocumentContent`：

- 文本内容
- 二进制内容
- diagnostics
- kind
- contentMode

用户点过或后台预热完成的文件会进入模型缓存，后续切换 tab / 点击同一文件不再重新读取或反编译.

相关文件：

- `src/model/DocumentContent.h`
- `src/model/SourceTreeModel.h`
- `src/model/SourceTreeModel.cpp`
- `src/model/OpenFileTabsModel.h`
- `src/model/OpenFileTabsModel.cpp`

### 6. 后台预热从全量改为有边界的优先级队列

最初的后台优化是“打开包后把所有 lazy 节点都排队预热”.这个方向能缓解点击等待，但对大 HAP 不够稳，可能浪费 CPU、IO 和内存.

现在已改成更接近 IDE 的调度策略：

- 用户点击的文件永远是前台最高优先级.
- 后台只预热值得预热的内容.
- 后台预热优先级：
  - 当前文件
  - 当前可见区域附近文件
  - 源码文件
  - 少量文本资源
- 后台不预读大二进制、图片、媒体.
- 后台队列最多保留 `512` 个候选.
- 后台并发数限制为 `2`.
- 后台结果超过 `2MB` 不进内存缓存，避免巨大内容撑爆内存.
- 用户真正点击大文件时仍完整加载.

相关入口：

- `DecompilerController::rebuildBackgroundPreloadQueue`
- `DecompilerController::startNextBackgroundPreloads`
- `SourceTreeModel::prioritizedPreloadNodeIndices`
- `SourceTreeModel::nodeEligibleForBackgroundLoad`

## 当前架构判断

当前架构已经修掉了最主要的方向性问题：

- 不再在 UI 线程同步调用 Hyle.
- 不再依赖重型 QML delegate 渲染大量内容.
- 不再让用户每点一个文件才从零开始加载.
- 不再无脑全量预热所有资源.

但还不能宣称已经完全达到 jadx / VS Code 的成熟程度.当前版本是正确底座，后续仍需要压测和迭代.

## 后续方向

### 1. 真实大样本压测

需要用真实 HAP 做闭环测试，例如：

- 首次拖入到文件树出现的耗时.
- 默认文件显示耗时.
- 后台预热完成速度.
- 连续点击源码文件是否接近秒开.
- 点击资源、图片、媒体时是否合理延迟.
- 快速切换文件时是否产生旧任务污染.
- 内存是否持续增长.
- CPU 是否长期高占用.
- 拖动代码/hex 滚动条是否稳定.

建议建立固定测试样本集：

- 小型 HAP
- 中型 HAP
- 大型 HAP
- 超多源码文件 HAP
- 大资源文件 HAP
- 单独 `.abc` 文件

### 2. LRU 内存缓存

当前缓存进入 `SourceTreeModel` 后没有真正的淘汰策略.后续需要引入 LRU：

- 按字节数控制缓存总量.
- 当前 tab、最近访问文件、当前可见附近文件优先保留.
- 大文件默认不常驻，除非用户正在查看.

建议指标：

- 文本缓存总量上限.
- 二进制缓存总量上限.
- 图片/媒体预览缓存单独管理.

### 3. 磁盘缓存

为了接近 jadx 的体验，后续应考虑磁盘缓存：

- 以 HAP 路径、大小、mtime、hash 作为 cache key.
- 缓存已反编译源码和资源预览.
- 下次打开同一 HAP 时可以先显示磁盘缓存，再后台验证更新.

需要注意：

- cache invalidation 必须可靠.
- 不能缓存敏感样本到不可控位置.
- 应提供清理缓存机制.

### 4. 前台任务抢占后台任务

当前策略能避免重复任务，但没有真正抢占已经运行的后台任务.后续可以：

- 把 Hyle 任务封装为可取消 job.
- 前台请求到来时暂停派发新的后台任务.
- 如果后台任务可取消，则取消低优先级任务，优先执行用户点击目标.

### 5. 批量/节流 UI 更新

后台预热完成时会写入模型并触发更新.大样本下如果完成频率高，可能造成 UI 抖动.

后续应考虑：

- 后台结果批量合并.
- 非当前文件的 `dataChanged` 节流.
- 当前文件仍立即更新.

### 6. 更细粒度资源策略

现在主要根据 section 和 contentMode 判断是否后台预热.更理想的是 Hyle 在资源索引中提供 size / mime / type 元数据，ReArk 根据这些元数据决策：

- 小文本资源可以预热.
- 大文本资源只预览前 N KB.
- 图片只预取元信息或缩略图.
- 媒体不预读，只在用户点击时准备预览.
- 巨大二进制只进入 hex 虚拟读取，不整体进内存.

### 7. Hex 超大文件的进一步虚拟化

当前 Hex 绘制已经虚拟化，但数据仍来自 `QByteArray`.对巨大资源，后续应避免一次性读入全部内容：

- 支持按范围读取 resource.
- Hex view 按 visible range 请求数据页.
- 维护页缓存.
- 滚动时预取相邻页.

这需要 Hyle API 支持 range read，或者 ReArk 建立临时文件映射层.

### 8. 代码高亮后台化

当前代码视图快速滚动时会跳过高亮，避免卡顿.后续可以继续优化：

- 可见区域先纯文本显示.
- 后台计算高亮 token.
- 高亮结果按行缓存.
- 滚动停止后补齐高亮.

## 验证清单

每次修改加载/缓存/绘制逻辑后，应至少验证：

- Debug 构建通过.
- 打开 HAP 不崩溃.
- 文件树能显示.
- 默认文件能显示.
- 点击源码文件能显示.
- 点击 resource text/json 能显示.
- 点击 binary resource 能显示 Hex.
- 点击图片/媒体不破坏文本/hex 路径.
- 快速切换文件不会显示旧文件结果.
- 关闭/重新打开样本不会复用旧 session 结果.
- 拖动代码滚动条无明显卡顿.
- 拖动 Hex 滚动条无明显卡顿.
- 长时间后台预热时 UI 仍可操作.
- 内存增长在预期范围内.

## 当前结论

本轮优化已经把 ReArk 从“同步/懒加载/重型 delegate”的模式推进到“异步/虚拟绘制/优先级预热/有限缓存”的模式.

这不是终点，但已经是正确的底层方向.后续要继续用真实大样本压测驱动，把 LRU、磁盘缓存、任务抢占、批量 UI 更新和超大资源分页读取补齐，才能更接近 jadx / VS Code 的成熟体验.
