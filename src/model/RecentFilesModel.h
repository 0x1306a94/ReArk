#ifndef REARK_RECENT_FILES_MODEL_H
#define REARK_RECENT_FILES_MODEL_H

#include <QAbstractListModel>
#include <QString>
#include <QStringList>

class RecentFilesModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString longestDisplayName READ longestDisplayName NOTIFY entriesChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        PathRole,
        ExistsRole
    };

    explicit RecentFilesModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] int count() const;
    [[nodiscard]] QString longestDisplayName() const;

    Q_INVOKABLE void addFile(const QString& filePath);
    Q_INVOKABLE bool fileExists(const QString& filePath) const;
    Q_INVOKABLE void removeFile(const QString& filePath);
    Q_INVOKABLE void clear();

signals:
    void countChanged();
    void entriesChanged();

private:
    void load();
    void save() const;
    void replacePaths(QStringList paths);
    [[nodiscard]] static QString normalizedPath(const QString& filePath);
    [[nodiscard]] static bool samePath(const QString& lhs, const QString& rhs);

    QStringList paths_;
};

#endif // REARK_RECENT_FILES_MODEL_H
