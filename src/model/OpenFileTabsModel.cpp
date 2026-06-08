#include "model/OpenFileTabsModel.h"

#include <algorithm>
#include <utility>

OpenFileTabsModel::OpenFileTabsModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int OpenFileTabsModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(tabs_.size());
}

QVariant OpenFileTabsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    const auto& tab = tabs_.at(static_cast<std::size_t>(index.row()));
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return tab.name;
    case PathRole:
        return tab.path;
    case ContentRole:
        return tab.document ? tab.document->text : QString{};
    case ContentModeRole:
        return tab.contentMode;
    case DiagnosticsRole:
        return tab.document ? tab.document->diagnostics : QString{};
    case NodeIndexRole:
        return tab.nodeIndex;
    case LoadingRole:
        return tab.loading;
    case ActiveRole:
        return index.row() == activeIndex_;
    default:
        return {};
    }
}

QHash<int, QByteArray> OpenFileTabsModel::roleNames() const
{
    return {
        { NameRole, "name" },
        { PathRole, "path" },
        { ContentRole, "content" },
        { ContentModeRole, "contentMode" },
        { DiagnosticsRole, "diagnostics" },
        { NodeIndexRole, "nodeIndex" },
        { LoadingRole, "loading" },
        { ActiveRole, "active" }
    };
}

int OpenFileTabsModel::activeIndex() const
{
    return activeIndex_;
}

QString OpenFileTabsModel::activeContent() const
{
    if (activeIndex_ < 0 || activeIndex_ >= rowCount()) {
        return tr("// Drop a package to start decompiling.");
    }
    const auto& tab = tabs_.at(static_cast<std::size_t>(activeIndex_));
    if (tab.loading && (!tab.document || tab.document->text.isEmpty())) {
        return tr("// Decompiling selected source file...");
    }
    return tab.document ? tab.document->text : QString{};
}

QByteArray OpenFileTabsModel::activeBinaryContent() const
{
    if (activeIndex_ < 0 || activeIndex_ >= rowCount()) {
        return {};
    }
    const auto& tab = tabs_.at(static_cast<std::size_t>(activeIndex_));
    return tab.document ? tab.document->binary : QByteArray{};
}

QString OpenFileTabsModel::activeContentMode() const
{
    if (activeIndex_ < 0 || activeIndex_ >= rowCount()) {
        return QStringLiteral("text");
    }
    return tabs_.at(static_cast<std::size_t>(activeIndex_)).contentMode;
}

QString OpenFileTabsModel::activeName() const
{
    if (activeIndex_ < 0 || activeIndex_ >= rowCount()) {
        return {};
    }
    return tabs_.at(static_cast<std::size_t>(activeIndex_)).name;
}

QString OpenFileTabsModel::activePath() const
{
    if (activeIndex_ < 0 || activeIndex_ >= rowCount()) {
        return {};
    }
    return tabs_.at(static_cast<std::size_t>(activeIndex_)).path;
}

QString OpenFileTabsModel::activeKind() const
{
    if (activeIndex_ < 0 || activeIndex_ >= rowCount()) {
        return {};
    }
    return tabs_.at(static_cast<std::size_t>(activeIndex_)).kind;
}

int OpenFileTabsModel::activeNodeIndex() const
{
    if (activeIndex_ < 0 || activeIndex_ >= rowCount()) {
        return -1;
    }
    return tabs_.at(static_cast<std::size_t>(activeIndex_)).nodeIndex;
}

bool OpenFileTabsModel::activeHasBinary() const
{
    if (activeIndex_ < 0 || activeIndex_ >= rowCount()) {
        return false;
    }
    const auto& tab = tabs_.at(static_cast<std::size_t>(activeIndex_));
    return tab.document && !tab.document->binary.isEmpty();
}

QString OpenFileTabsModel::activeDiagnostics() const
{
    if (activeIndex_ < 0 || activeIndex_ >= rowCount()) {
        return {};
    }
    const auto& tab = tabs_.at(static_cast<std::size_t>(activeIndex_));
    return tab.document ? tab.document->diagnostics : QString{};
}

bool OpenFileTabsModel::hasTabs() const
{
    return !tabs_.empty();
}

void OpenFileTabsModel::clear()
{
    if (tabs_.empty()) {
        return;
    }

    beginResetModel();
    tabs_.clear();
    activeIndex_ = -1;
    endResetModel();
    emit activeIndexChanged();
    emitActiveChanged();
    emit tabsChanged();
}

void OpenFileTabsModel::openOrActivate(int nodeIndex, const QString& name, const QString& path, const QString& kind, std::shared_ptr<DocumentContent> document, const QString& contentMode, bool loading)
{
    const int existingIndex = tabIndexForNode(nodeIndex);
    if (existingIndex >= 0) {
        auto& tab = tabs_.at(static_cast<std::size_t>(existingIndex));
        tab.name = name;
        tab.path = path;
        tab.kind = kind;
        tab.document = std::move(document);
        tab.contentMode = contentMode;
        tab.loading = loading;
        const auto modelIndex = index(existingIndex);
        emit dataChanged(modelIndex, modelIndex, { NameRole, PathRole, ContentRole, ContentModeRole, DiagnosticsRole, LoadingRole });
        setActiveIndex(existingIndex);
        return;
    }

    const int insertIndex = rowCount();
    beginInsertRows({}, insertIndex, insertIndex);
    tabs_.push_back({
        nodeIndex,
        name,
        path,
        kind,
        contentMode,
        std::move(document),
        loading
    });
    endInsertRows();
    emit tabsChanged();
    setActiveIndex(insertIndex);
}

void OpenFileTabsModel::updateNode(int nodeIndex, std::shared_ptr<DocumentContent> document)
{
    const int tabIndex = tabIndexForNode(nodeIndex);
    if (tabIndex < 0) {
        return;
    }

    auto& tab = tabs_.at(static_cast<std::size_t>(tabIndex));
    tab.document = std::move(document);
    if (tab.document && !tab.document->kind.isEmpty()) {
        tab.kind = tab.document->kind;
    }
    if (tab.document && !tab.document->contentMode.isEmpty()) {
        tab.contentMode = tab.document->contentMode;
    }
    tab.loading = false;

    const auto modelIndex = index(tabIndex);
    emit dataChanged(modelIndex, modelIndex, { ContentRole, ContentModeRole, DiagnosticsRole, LoadingRole });
    if (tabIndex == activeIndex_) {
        emitActiveChanged();
    }
}

void OpenFileTabsModel::setNodeLoading(int nodeIndex, bool loading)
{
    const int tabIndex = tabIndexForNode(nodeIndex);
    if (tabIndex < 0) {
        return;
    }

    auto& tab = tabs_.at(static_cast<std::size_t>(tabIndex));
    if (tab.loading == loading) {
        return;
    }
    tab.loading = loading;
    const auto modelIndex = index(tabIndex);
    emit dataChanged(modelIndex, modelIndex, { LoadingRole });
    if (tabIndex == activeIndex_) {
        emitActiveChanged();
    }
}

void OpenFileTabsModel::closeTab(int indexToClose)
{
    if (indexToClose < 0 || indexToClose >= rowCount()) {
        return;
    }

    const bool closingActive = indexToClose == activeIndex_;
    beginRemoveRows({}, indexToClose, indexToClose);
    tabs_.erase(tabs_.begin() + indexToClose);
    endRemoveRows();

    if (tabs_.empty()) {
        activeIndex_ = -1;
    } else if (closingActive) {
        activeIndex_ = std::min(indexToClose, rowCount() - 1);
    } else if (indexToClose < activeIndex_) {
        --activeIndex_;
    }

    emit tabsChanged();
    emit activeIndexChanged();
    emitActiveChanged();
}

void OpenFileTabsModel::closeOtherTabs(int indexToKeep)
{
    if (indexToKeep < 0 || indexToKeep >= rowCount() || rowCount() <= 1) {
        return;
    }

    Tab keptTab = std::move(tabs_.at(static_cast<std::size_t>(indexToKeep)));

    beginResetModel();
    tabs_.clear();
    tabs_.push_back(std::move(keptTab));
    activeIndex_ = 0;
    endResetModel();

    emit tabsChanged();
    emit activeIndexChanged();
    emitActiveChanged();
}

void OpenFileTabsModel::closeTabsToLeft(int indexToKeep)
{
    if (indexToKeep <= 0 || indexToKeep >= rowCount()) {
        return;
    }

    beginResetModel();
    tabs_.erase(tabs_.begin(), tabs_.begin() + indexToKeep);
    activeIndex_ = activeIndex_ < indexToKeep ? 0 : activeIndex_ - indexToKeep;
    endResetModel();

    emit tabsChanged();
    emit activeIndexChanged();
    emitActiveChanged();
}

void OpenFileTabsModel::closeTabsToRight(int indexToKeep)
{
    if (indexToKeep < 0 || indexToKeep >= rowCount() - 1) {
        return;
    }

    beginResetModel();
    tabs_.erase(tabs_.begin() + indexToKeep + 1, tabs_.end());
    if (activeIndex_ > indexToKeep) {
        activeIndex_ = indexToKeep;
    }
    endResetModel();

    emit tabsChanged();
    emit activeIndexChanged();
    emitActiveChanged();
}

void OpenFileTabsModel::setActiveIndex(int index)
{
    if (index < -1) {
        index = -1;
    }
    if (index >= rowCount()) {
        index = rowCount() - 1;
    }
    if (activeIndex_ == index) {
        return;
    }

    const int previousIndex = activeIndex_;
    activeIndex_ = index;
    if (previousIndex >= 0) {
        const auto modelIndex = this->index(previousIndex);
        emit dataChanged(modelIndex, modelIndex, { ActiveRole });
    }
    if (activeIndex_ >= 0) {
        const auto modelIndex = this->index(activeIndex_);
        emit dataChanged(modelIndex, modelIndex, { ActiveRole });
    }
    emit activeIndexChanged();
    emitActiveChanged();
}

int OpenFileTabsModel::tabIndexForNode(int nodeIndex) const
{
    const auto found = std::ranges::find_if(tabs_, [nodeIndex](const Tab& tab) {
        return tab.nodeIndex == nodeIndex;
    });
    if (found == tabs_.end()) {
        return -1;
    }
    return static_cast<int>(std::distance(tabs_.begin(), found));
}

void OpenFileTabsModel::emitActiveChanged()
{
    emit activeTabChanged();
}
