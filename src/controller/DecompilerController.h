#ifndef REARK_DECOMPILER_CONTROLLER_H
#define REARK_DECOMPILER_CONTROLLER_H

#include "core/HyleDecompiler.h"
#include "model/HexDocumentModel.h"
#include "model/OpenFileTabsModel.h"
#include "model/SourceTreeModel.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVector>

#include <memory>
#include <deque>
#include <functional>
#include <set>
#include <vector>

class ResourcePreviewProvider;

class DecompilerController : public QObject {
    Q_OBJECT
    Q_PROPERTY(SourceTreeModel* treeModel READ treeModel CONSTANT)
    Q_PROPERTY(OpenFileTabsModel* tabsModel READ tabsModel CONSTANT)
    Q_PROPERTY(HexDocumentModel* hexModel READ hexModel CONSTANT)
    Q_PROPERTY(QString selectedContent READ selectedContent NOTIFY selectedContentChanged)
    Q_PROPERTY(QString selectedName READ selectedName NOTIFY selectedNameChanged)
    Q_PROPERTY(QString diagnostics READ diagnostics NOTIFY diagnosticsChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(double loadingProgress READ loadingProgress NOTIFY loadingProgressChanged)
    Q_PROPERTY(QStringList activityLog READ activityLog NOTIFY activityLogChanged)
    Q_PROPERTY(bool hasPackage READ hasPackage NOTIFY packageChanged)
    Q_PROPERTY(QString packagePath READ packagePath NOTIFY packageChanged)
    Q_PROPERTY(QString appIconUrl READ appIconUrl NOTIFY appIconChanged)
    Q_PROPERTY(QString appIconDataUrl READ appIconDataUrl NOTIFY appIconChanged)
    Q_PROPERTY(QString appIconPath READ appIconPath NOTIFY appIconChanged)
    Q_PROPERTY(bool appIconLayered READ appIconLayered NOTIFY appIconChanged)
    Q_PROPERTY(bool activeSupportsDisassembly READ activeSupportsDisassembly NOTIFY activeDisassemblyChanged)
    Q_PROPERTY(bool activeDisassemblyLoading READ activeDisassemblyLoading NOTIFY activeDisassemblyChanged)
    Q_PROPERTY(QString activeDisassemblyContent READ activeDisassemblyContent NOTIFY activeDisassemblyChanged)
    Q_PROPERTY(bool abcEvidenceBusy READ abcEvidenceBusy NOTIFY abcEvidenceChanged)
    Q_PROPERTY(QString abcEvidenceKind READ abcEvidenceKind NOTIFY abcEvidenceChanged)
    Q_PROPERTY(QString abcEvidenceTitle READ abcEvidenceTitle NOTIFY abcEvidenceChanged)
    Q_PROPERTY(QString abcEvidenceContent READ abcEvidenceContent NOTIFY abcEvidenceChanged)
    Q_PROPERTY(bool abcXrefsBusy READ abcXrefsBusy NOTIFY abcXrefsChanged)
    Q_PROPERTY(QString abcXrefsQuery READ abcXrefsQuery NOTIFY abcXrefsChanged)
    Q_PROPERTY(QString abcXrefsKind READ abcXrefsKind NOTIFY abcXrefsChanged)
    Q_PROPERTY(QString abcXrefsError READ abcXrefsError NOTIFY abcXrefsChanged)
    Q_PROPERTY(QVariantList abcXrefRows READ abcXrefRows NOTIFY abcXrefsChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedIndexChanged)

public:
    struct AgentFileSnapshot {
        QString name;
        QString path;
        QString kind;
        QString section;
        QString contentMode;
        QString content;
        QString disassembly;
        std::size_t hyleId = 0;
        std::size_t packageId = 0;
        bool loaded = false;
        bool hasDisassembly = false;
        bool disassemblyLoaded = false;
    };

    struct AgentSnapshot {
        QString packageSummary;
        QString fileList;
        QString entryPoints;
        QString signatureSummary;
        QVector<AgentFileSnapshot> files;
        QString packagePath;
        std::shared_ptr<HyleDecompiler::SessionContext> packageContext;
    };

    explicit DecompilerController(ResourcePreviewProvider* previewProvider, QObject* parent = nullptr);

    [[nodiscard]] SourceTreeModel* treeModel();
    [[nodiscard]] OpenFileTabsModel* tabsModel();
    [[nodiscard]] HexDocumentModel* hexModel();
    [[nodiscard]] QString selectedContent() const;
    [[nodiscard]] QString selectedName() const;
    [[nodiscard]] QString diagnostics() const;
    [[nodiscard]] QString status() const;
    [[nodiscard]] bool busy() const;
    [[nodiscard]] double loadingProgress() const;
    [[nodiscard]] QStringList activityLog() const;
    [[nodiscard]] bool hasPackage() const;
    [[nodiscard]] QString packagePath() const;
    [[nodiscard]] QString appIconUrl() const;
    [[nodiscard]] QString appIconDataUrl() const;
    [[nodiscard]] QString appIconPath() const;
    [[nodiscard]] bool appIconLayered() const;
    [[nodiscard]] bool activeSupportsDisassembly() const;
    [[nodiscard]] bool activeDisassemblyLoading() const;
    [[nodiscard]] QString activeDisassemblyContent() const;
    [[nodiscard]] bool abcEvidenceBusy() const;
    [[nodiscard]] QString abcEvidenceKind() const;
    [[nodiscard]] QString abcEvidenceTitle() const;
    [[nodiscard]] QString abcEvidenceContent() const;
    [[nodiscard]] bool abcXrefsBusy() const;
    [[nodiscard]] QString abcXrefsQuery() const;
    [[nodiscard]] QString abcXrefsKind() const;
    [[nodiscard]] QString abcXrefsError() const;
    [[nodiscard]] QVariantList abcXrefRows() const;
    [[nodiscard]] int selectedIndex() const;

    Q_INVOKABLE void decompileFile(const QString& filePath);
    Q_INVOKABLE void activateIndex(int index);
    Q_INVOKABLE void openActivePreviewFile() const;
    Q_INVOKABLE void revealPackageInFileExplorer();
    Q_INVOKABLE QVariantList quickOpenCandidates(const QString& query) const;
    Q_INVOKABLE QVariantList searchCandidates(const QString& query) const;
    Q_INVOKABLE QVariantList entryPointCandidates() const;
    Q_INVOKABLE void navigateToNode(int nodeIndex);
    Q_INVOKABLE void loadActiveDisassembly();
    Q_INVOKABLE void requestAbcLiteralEvidence(const QString& offset, const QString& pathOrQuery = QStringLiteral("modules.abc"));
    Q_INVOKABLE void requestAbcStringSearch(const QString& pattern, int minLen, int maxLen, int limit, const QString& pathOrQuery = QStringLiteral("modules.abc"));
    Q_INVOKABLE void requestAbcTreeEvidence(const QString& pathOrQuery = QStringLiteral("modules.abc"), int limit = 200);
    Q_INVOKABLE void requestAbcXrefs(const QString& query, const QString& kind, int limit, const QString& pathOrQuery = QStringLiteral("modules.abc"));
    Q_INVOKABLE void requestAbcXrefRows(const QString& query, const QString& kind, int limit, const QString& pathOrQuery = QStringLiteral("modules.abc"));
    Q_INVOKABLE void requestAbcFlows(const QString& query, const QString& kind, int limit, const QString& pathOrQuery = QStringLiteral("modules.abc"));
    Q_INVOKABLE void clearAbcEvidence();
    Q_INVOKABLE void clearAbcXrefRows();
    Q_INVOKABLE bool navigateToAbcXref(const QVariantMap& row);
    Q_INVOKABLE QVariantList parseAbcStringRows(const QString& evidence) const;
    Q_INVOKABLE void showStatusMessage(const QString& message);
    Q_INVOKABLE QString formatJson(const QString& content) const;
    Q_INVOKABLE void copyTextToClipboard(const QString& text) const;
    Q_INVOKABLE void clear();

    [[nodiscard]] QString agentPackageSummary() const;
    [[nodiscard]] QString agentListFiles(const QString& query, int limit) const;
    [[nodiscard]] QString agentSearchLoadedContent(const QString& query, int limit) const;
    [[nodiscard]] QString agentReadSource(const QString& pathOrQuery, int maxChars) const;
    [[nodiscard]] QString agentReadDisassembly(const QString& pathOrQuery, int maxChars) const;
    [[nodiscard]] QString agentEntryPoints() const;
    [[nodiscard]] QString agentSignatureSummary(int maxChars) const;
    [[nodiscard]] AgentSnapshot agentSnapshot(int maxFiles = 500, int maxContentChars = 12000, int maxDisassemblyChars = 20000) const;

public slots:
    void setSelectedIndex(int index);

signals:
    void selectedContentChanged();
    void selectedNameChanged();
    void diagnosticsChanged();
    void statusChanged();
    void busyChanged();
    void loadingProgressChanged();
    void activityLogChanged();
    void packageChanged();
    void appIconChanged();
    void packageOpened(const QString& filePath, const QString& appIconDataUrl);
    void activeDisassemblyChanged();
    void abcEvidenceChanged();
    void abcXrefsChanged();
    void codeNavigationSearchRequested(const QString& query);
    void codeNavigationRevealRequested(const QStringList& queries);
    void selectedIndexChanged();

private:
    void setStatus(const QString& status);
    void setBusy(bool busy);
    void setLoadingProgress(double progress);
    void clearActivityLog();
    void appendActivity(const QString& activity);
    void setHasPackage(bool hasPackage);
    void clearAppIcon();
    void applyOpenResult(quint64 requestId, HyleDecompiler::OpenResult result);
    void applySourceResult(quint64 requestId, HyleDecompiler::SourceResult result);
    void applySourceBatchResult(quint64 requestId, HyleDecompiler::SourceBatchResult result);
    void applyDisassemblyResult(quint64 requestId, HyleDecompiler::DisassemblyResult result);
    void applyAbcEvidenceResult(quint64 requestId, const QString& content);
    void applyAbcXrefRowsResult(quint64 requestId, const QString& query, const QString& kind, HyleDecompiler::AbcXrefSearchResult result);
    void openFileTab(int nodeIndex);
    void startNodeLoad(int nodeIndex, bool foreground);
    void startSourceBatchLoad(std::vector<int> nodeIndices);
    void startDisassemblyLoad(int nodeIndex);
    void startAbcEvidenceRequest(const QString& kind, const QString& title, std::function<QString()> task);
    void setAbcEvidenceState(const QString& kind, const QString& title, const QString& content, bool busy);
    void resetLoadingState();
    void rebuildBackgroundPreloadQueue(int centerNode);
    void startNextBackgroundPreloads();
    void updateBackgroundPreloadProgress();
    void refreshActiveHexDocument();

    SourceTreeModel treeModel_;
    OpenFileTabsModel tabsModel_;
    HexDocumentModel hexModel_;
    ResourcePreviewProvider* previewProvider_ = nullptr;
    std::shared_ptr<HyleDecompiler::SessionContext> packageContext_;
    QString pendingPackagePath_;
    QString packagePath_;
    QString appIconUrl_;
    QString appIconDataUrl_;
    QString appIconPath_;
    bool appIconLayered_ = false;
    bool hasPackage_ = false;
    std::set<int> foregroundLoadingNodes_;
    std::set<int> backgroundLoadingNodes_;
    std::set<int> backgroundSkippedNodes_;
    std::set<int> disassemblyLoadingNodes_;
    std::deque<int> backgroundPreloadQueue_;
    int activeBackgroundPreloads_ = 0;
    int backgroundPreloadTotal_ = 0;
    int backgroundPreloadCompleted_ = 0;
    QString status_ = tr("Ready");
    bool busy_ = false;
    bool abcEvidenceBusy_ = false;
    QString abcEvidenceKind_;
    QString abcEvidenceTitle_;
    QString abcEvidenceContent_;
    bool abcXrefsBusy_ = false;
    QString abcXrefsQuery_;
    QString abcXrefsKind_;
    QString abcXrefsError_;
    QVariantList abcXrefRows_;
    double loadingProgress_ = 0.0;
    QStringList activityLog_;
    quint64 openRequestId_ = 0;
    quint64 abcEvidenceRequestId_ = 0;
    quint64 abcXrefsRequestId_ = 0;
};

#endif // REARK_DECOMPILER_CONTROLLER_H
