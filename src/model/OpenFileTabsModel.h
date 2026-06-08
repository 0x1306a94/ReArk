#ifndef REARK_OPEN_FILE_TABS_MODEL_H
#define REARK_OPEN_FILE_TABS_MODEL_H

#include "model/DocumentContent.h"

#include <QAbstractListModel>
#include <QByteArray>
#include <QString>

#include <memory>
#include <vector>

class OpenFileTabsModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int activeIndex READ activeIndex WRITE setActiveIndex NOTIFY activeIndexChanged)
    Q_PROPERTY(QString activeContent READ activeContent NOTIFY activeTabChanged)
    Q_PROPERTY(QString activeContentMode READ activeContentMode NOTIFY activeTabChanged)
    Q_PROPERTY(QString activeName READ activeName NOTIFY activeTabChanged)
    Q_PROPERTY(QString activePath READ activePath NOTIFY activeTabChanged)
    Q_PROPERTY(QString activeKind READ activeKind NOTIFY activeTabChanged)
    Q_PROPERTY(int activeNodeIndex READ activeNodeIndex NOTIFY activeTabChanged)
    Q_PROPERTY(bool activeHasBinary READ activeHasBinary NOTIFY activeTabChanged)
    Q_PROPERTY(QString activeDiagnostics READ activeDiagnostics NOTIFY activeTabChanged)
    Q_PROPERTY(bool hasTabs READ hasTabs NOTIFY tabsChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        PathRole,
        ContentRole,
        ContentModeRole,
        DiagnosticsRole,
        NodeIndexRole,
        LoadingRole,
        ActiveRole
    };

    explicit OpenFileTabsModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int activeIndex() const;
    [[nodiscard]] QString activeContent() const;
    [[nodiscard]] QByteArray activeBinaryContent() const;
    [[nodiscard]] QString activeContentMode() const;
    [[nodiscard]] QString activeName() const;
    [[nodiscard]] QString activePath() const;
    [[nodiscard]] QString activeKind() const;
    [[nodiscard]] int activeNodeIndex() const;
    [[nodiscard]] bool activeHasBinary() const;
    [[nodiscard]] QString activeDiagnostics() const;
    [[nodiscard]] bool hasTabs() const;

    void clear();
    void openOrActivate(int nodeIndex, const QString& name, const QString& path, const QString& kind, std::shared_ptr<DocumentContent> document, const QString& contentMode, bool loading);
    void updateNode(int nodeIndex, std::shared_ptr<DocumentContent> document);
    void setNodeLoading(int nodeIndex, bool loading);

    Q_INVOKABLE void closeTab(int index);
    Q_INVOKABLE void closeOtherTabs(int index);
    Q_INVOKABLE void closeTabsToLeft(int index);
    Q_INVOKABLE void closeTabsToRight(int index);

public slots:
    void setActiveIndex(int index);

signals:
    void activeIndexChanged();
    void activeTabChanged();
    void tabsChanged();

private:
    struct Tab {
        int nodeIndex = -1;
        QString name;
        QString path;
        QString kind;
        QString contentMode = QStringLiteral("text");
        std::shared_ptr<DocumentContent> document;
        bool loading = false;
    };

    [[nodiscard]] int tabIndexForNode(int nodeIndex) const;
    void emitActiveChanged();

    std::vector<Tab> tabs_;
    int activeIndex_ = -1;
};

#endif // REARK_OPEN_FILE_TABS_MODEL_H
