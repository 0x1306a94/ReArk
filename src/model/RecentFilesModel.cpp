#include "model/RecentFilesModel.h"

#include <QDateTime>
#include <QFileInfo>
#include <QSettings>
#include <QVariantMap>

#include <algorithm>
#include <utility>

namespace {

constexpr qsizetype kMaxRecentFiles = 8;
constexpr auto kSettingsGroup = "RecentFiles";
constexpr auto kPathsKey = "Paths";
constexpr auto kIconUrlsKey = "IconUrls";

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
    case IconUrlRole:
        return QFileInfo(path).isFile() ? iconUrls_.value(cacheKey(path)) : QString();
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
        { IconUrlRole, "iconUrl" },
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

void RecentFilesModel::addFile(const QString& filePath, const QString& iconUrl)
{
    const QString path = normalizedPath(filePath);
    if (path.isEmpty()) {
        return;
    }

    if (!iconUrl.isEmpty()) {
        iconUrls_.insert(cacheKey(path), iconUrl);
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
    iconUrls_.remove(cacheKey(path));
    replacePaths(std::move(nextPaths));
}

void RecentFilesModel::clear()
{
    iconUrls_.clear();
    replacePaths({});
}

void RecentFilesModel::load()
{
    QSettings settings;
    settings.beginGroup(QLatin1String(kSettingsGroup));
    const QStringList storedPaths = settings.value(QLatin1String(kPathsKey)).toStringList();
    const QVariantMap storedIconUrls = settings.value(QLatin1String(kIconUrlsKey)).toMap();
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

    QHash<QString, QString> loadedIconUrls;
    for (const QString& path : loadedPaths) {
        const QString key = cacheKey(path);
        const QString iconUrl = storedIconUrls.value(key).toString();
        if (!iconUrl.isEmpty()) {
            loadedIconUrls.insert(key, iconUrl);
        }
    }

    beginResetModel();
    paths_ = std::move(loadedPaths);
    iconUrls_ = std::move(loadedIconUrls);
    endResetModel();
    emit entriesChanged();
    emit countChanged();
}

void RecentFilesModel::save() const
{
    QSettings settings;
    settings.beginGroup(QLatin1String(kSettingsGroup));
    settings.setValue(QLatin1String(kPathsKey), paths_);
    QVariantMap storedIconUrls;
    for (const QString& path : paths_) {
        const QString key = cacheKey(path);
        const QString iconUrl = iconUrls_.value(key);
        if (!iconUrl.isEmpty()) {
            storedIconUrls.insert(key, iconUrl);
        }
    }
    settings.setValue(QLatin1String(kIconUrlsKey), storedIconUrls);
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

    QHash<QString, QString> nextIconUrls;
    for (const QString& path : paths_) {
        const QString key = cacheKey(path);
        const QString iconUrl = iconUrls_.value(key);
        if (!iconUrl.isEmpty()) {
            nextIconUrls.insert(key, iconUrl);
        }
    }
    iconUrls_ = std::move(nextIconUrls);

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

QString RecentFilesModel::cacheKey(const QString& filePath)
{
    const QString path = normalizedPath(filePath);
    const QFileInfo fileInfo(path);
    return QStringLiteral("%1|%2|%3")
        .arg(path.toCaseFolded())
        .arg(fileInfo.isFile() ? fileInfo.size() : -1)
        .arg(fileInfo.isFile() ? fileInfo.lastModified().toUTC().toMSecsSinceEpoch() : -1);
}

bool RecentFilesModel::samePath(const QString& lhs, const QString& rhs)
{
    return lhs.compare(rhs, Qt::CaseInsensitive) == 0;
}
