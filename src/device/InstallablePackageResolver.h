#ifndef REARK_INSTALLABLE_PACKAGE_RESOLVER_H
#define REARK_INSTALLABLE_PACKAGE_RESOLVER_H

#include <QList>
#include <QString>

struct InstallablePackageCandidate {
    QString path;
    QString displayName;
};

struct InstallablePackageSelection {
    bool ok = false;
    QString path;
    QString displayName;
    QString diagnostic;
    QList<InstallablePackageCandidate> candidates;
};

class InstallablePackageResolver {
public:
    [[nodiscard]] static InstallablePackageSelection select(
        QList<InstallablePackageCandidate> candidates,
        const QString& currentPackagePath,
        const QString& moduleSelector = {});

    [[nodiscard]] static QString describeCandidates(
        const QList<InstallablePackageCandidate>& candidates);
};

#endif // REARK_INSTALLABLE_PACKAGE_RESOLVER_H
