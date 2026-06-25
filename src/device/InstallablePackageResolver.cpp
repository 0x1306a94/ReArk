#include "device/InstallablePackageResolver.h"

#include <QFileInfo>
#include <QStringList>

#include <algorithm>

namespace {

bool hasHapSuffix(const QString& path)
{
    return path.endsWith(QStringLiteral(".hap"), Qt::CaseInsensitive);
}

QString candidateName(const InstallablePackageCandidate& candidate)
{
    const QString displayName = candidate.displayName.trimmed();
    if (!displayName.isEmpty()) {
        return displayName;
    }
    return QFileInfo(candidate.path).fileName();
}

InstallablePackageCandidate normalizedCandidate(InstallablePackageCandidate candidate)
{
    candidate.path = candidate.path.trimmed();
    candidate.displayName = candidateName(candidate);
    return candidate;
}

QList<InstallablePackageCandidate> normalizedCandidates(
    const QList<InstallablePackageCandidate>& candidates)
{
    QList<InstallablePackageCandidate> result;
    for (const InstallablePackageCandidate& candidate : candidates) {
        InstallablePackageCandidate normalized = normalizedCandidate(candidate);
        if (!normalized.path.isEmpty() && hasHapSuffix(normalized.path)) {
            result.append(std::move(normalized));
        }
    }
    return result;
}

int selectorScore(const InstallablePackageCandidate& candidate, const QString& selector)
{
    const QString foldedSelector = selector.trimmed().toCaseFolded();
    if (foldedSelector.isEmpty()) {
        return -1;
    }

    const QString foldedPath = candidate.path.toCaseFolded();
    const QString foldedDisplayName = candidate.displayName.toCaseFolded();
    const QString foldedFileName = QFileInfo(candidate.path).fileName().toCaseFolded();
    if (foldedPath == foldedSelector
        || foldedDisplayName == foldedSelector
        || foldedFileName == foldedSelector) {
        return 100;
    }
    if (foldedDisplayName.contains(foldedSelector)
        || foldedFileName.contains(foldedSelector)) {
        return 60;
    }
    if (foldedPath.contains(foldedSelector)) {
        return 40;
    }
    return -1;
}

InstallablePackageSelection selectedCandidate(InstallablePackageCandidate candidate)
{
    InstallablePackageSelection selection;
    selection.ok = true;
    selection.path = candidate.path;
    selection.displayName = candidate.displayName;
    if (!QFileInfo::exists(selection.path)) {
        selection.ok = false;
        selection.diagnostic = QStringLiteral("Installable HAP is not available on disk: %1")
            .arg(selection.path);
    }
    return selection;
}

} // namespace

InstallablePackageSelection InstallablePackageResolver::select(
    QList<InstallablePackageCandidate> candidates,
    const QString& currentPackagePath,
    const QString& moduleSelector)
{
    InstallablePackageSelection selection;
    selection.candidates = normalizedCandidates(candidates);

    if (!moduleSelector.trimmed().isEmpty()) {
        int bestScore = -1;
        QList<InstallablePackageCandidate> bestMatches;
        for (const InstallablePackageCandidate& candidate : selection.candidates) {
            const int score = selectorScore(candidate, moduleSelector);
            if (score < 0) {
                continue;
            }
            if (score > bestScore) {
                bestScore = score;
                bestMatches.clear();
            }
            if (score == bestScore) {
                bestMatches.append(candidate);
            }
        }

        if (bestMatches.size() == 1) {
            InstallablePackageSelection matched = selectedCandidate(bestMatches.first());
            matched.candidates = selection.candidates;
            return matched;
        }
        if (bestMatches.size() > 1) {
            selection.diagnostic = QStringLiteral(
                "Module selector is ambiguous: %1\nAvailable HAP modules:\n%2")
                .arg(moduleSelector, describeCandidates(selection.candidates));
            return selection;
        }
        selection.diagnostic = QStringLiteral(
            "No installable HAP module matched selector: %1\nAvailable HAP modules:\n%2")
            .arg(moduleSelector, describeCandidates(selection.candidates));
        return selection;
    }

    if (selection.candidates.size() == 1) {
        InstallablePackageSelection matched = selectedCandidate(selection.candidates.first());
        matched.candidates = selection.candidates;
        return matched;
    }

    const QString packagePath = currentPackagePath.trimmed();
    if (selection.candidates.isEmpty() && hasHapSuffix(packagePath)) {
        InstallablePackageCandidate candidate {
            .path = packagePath,
            .displayName = QFileInfo(packagePath).fileName()
        };
        selection = selectedCandidate(std::move(candidate));
        return selection;
    }

    if (selection.candidates.isEmpty()) {
        selection.diagnostic = packagePath.isEmpty()
            ? QStringLiteral("No active ReArk package is loaded.")
            : QStringLiteral("Current package has no installable HAP module: %1").arg(packagePath);
        return selection;
    }

    selection.diagnostic = QStringLiteral(
        "Current APP contains multiple HAP modules. Specify module to install one of them:\n%1")
        .arg(describeCandidates(selection.candidates));
    return selection;
}

QString InstallablePackageResolver::describeCandidates(
    const QList<InstallablePackageCandidate>& candidates)
{
    if (candidates.isEmpty()) {
        return QStringLiteral("(none)");
    }

    QStringList lines;
    for (const InstallablePackageCandidate& rawCandidate : candidates) {
        const InstallablePackageCandidate candidate = normalizedCandidate(rawCandidate);
        lines.append(QStringLiteral("- %1 (%2)").arg(candidate.displayName, candidate.path));
    }
    return lines.join(QLatin1Char('\n'));
}
