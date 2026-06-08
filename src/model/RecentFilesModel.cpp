#include "model/RecentFilesModel.h"

#include <QFileInfo>
#include <QSettings>

#include <algorithm>
#include <utility>

namespace {

constexpr qsizetype kMaxRecentFiles = 8;
constexpr auto kSettingsGroup = "RecentFiles";
constexpr auto kPathsKey = "Paths";

} // namespace

RecentFilesModel::RecentFilesModel(QObject* parent)
    : QAbstractListModel(parent)
{
    load();
}

int RecentFilesModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return paths_.size();
}

QVariant RecentFilesModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= paths_.size()) {
        return {};
    }

    const QString& path = paths_.at(index.row());
    switch (role) {
    case NameRole:
        return QFileInfo(path).fileName();
    case PathRole:
        return path;
    case ExistsRole:
        return QFileInfo(path).isFile();
    default:
        return {};
    }
}

QHash<int, QByteArray> RecentFilesModel::roleNames() const
{
    return {
        { NameRole, "name" },
        { PathRole, "path" },
        { ExistsRole, "exists" },
    };
}

int RecentFilesModel::count() const
{
    return paths_.size();
}

QString RecentFilesModel::longestDisplayName() const
{
    QString longest;
    for (const QString& path : paths_) {
        const QFileInfo fileInfo(path);
        QString displayName = fileInfo.fileName();
        if (!fileInfo.isFile()) {
            displayName = tr("%1 (missing)").arg(displayName);
        }
        if (displayName.size() > longest.size()) {
            longest = std::move(displayName);
        }
    }
    return longest;
}

void RecentFilesModel::addFile(const QString& filePath)
{
    const QString path = normalizedPath(filePath);
    if (path.isEmpty()) {
        return;
    }

    QStringList nextPaths;
    nextPaths.reserve(std::min(paths_.size() + 1, kMaxRecentFiles));
    nextPaths.append(path);
    for (const QString& existingPath : paths_) {
        if (!samePath(path, existingPath) && nextPaths.size() < kMaxRecentFiles) {
            nextPaths.append(existingPath);
        }
    }
    replacePaths(std::move(nextPaths));
}

bool RecentFilesModel::fileExists(const QString& filePath) const
{
    const QString path = normalizedPath(filePath);
    return !path.isEmpty() && QFileInfo(path).isFile();
}

void RecentFilesModel::removeFile(const QString& filePath)
{
    const QString path = normalizedPath(filePath);
    if (path.isEmpty()) {
        return;
    }

    QStringList nextPaths;
    nextPaths.reserve(paths_.size());
    for (const QString& existingPath : paths_) {
        if (!samePath(path, existingPath)) {
            nextPaths.append(existingPath);
        }
    }
    replacePaths(std::move(nextPaths));
}

void RecentFilesModel::clear()
{
    replacePaths({});
}

void RecentFilesModel::load()
{
    QSettings settings;
    settings.beginGroup(QLatin1String(kSettingsGroup));
    const QStringList storedPaths = settings.value(QLatin1String(kPathsKey)).toStringList();
    settings.endGroup();

    QStringList loadedPaths;
    loadedPaths.reserve(std::min(storedPaths.size(), kMaxRecentFiles));
    for (const QString& storedPath : storedPaths) {
        const QString path = normalizedPath(storedPath);
        if (path.isEmpty()) {
            continue;
        }

        const bool duplicate = std::any_of(loadedPaths.cbegin(), loadedPaths.cend(), [&path](const QString& existingPath) {
            return samePath(path, existingPath);
        });
        if (!duplicate) {
            loadedPaths.append(path);
        }
        if (loadedPaths.size() >= kMaxRecentFiles) {
            break;
        }
    }
    if (loadedPaths.isEmpty()) {
        return;
    }

    beginResetModel();
    paths_ = std::move(loadedPaths);
    endResetModel();
    emit entriesChanged();
    emit countChanged();
}

void RecentFilesModel::save() const
{
    QSettings settings;
    settings.beginGroup(QLatin1String(kSettingsGroup));
    settings.setValue(QLatin1String(kPathsKey), paths_);
    settings.endGroup();
}

void RecentFilesModel::replacePaths(QStringList paths)
{
    if (paths_ == paths) {
        return;
    }

    const int oldCount = paths_.size();
    beginResetModel();
    paths_ = std::move(paths);
    endResetModel();
    save();
    emit entriesChanged();

    if (oldCount != paths_.size()) {
        emit countChanged();
    }
}

QString RecentFilesModel::normalizedPath(const QString& filePath)
{
    const QString trimmedPath = filePath.trimmed();
    if (trimmedPath.isEmpty()) {
        return {};
    }

    return QFileInfo(trimmedPath).absoluteFilePath();
}

bool RecentFilesModel::samePath(const QString& lhs, const QString& rhs)
{
    return lhs.compare(rhs, Qt::CaseInsensitive) == 0;
}
