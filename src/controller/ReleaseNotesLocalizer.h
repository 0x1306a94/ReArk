#ifndef REARK_RELEASE_NOTES_LOCALIZER_H
#define REARK_RELEASE_NOTES_LOCALIZER_H

#include <QString>
#include <QStringList>

namespace ReleaseNotesLocalizer {

[[nodiscard]] QString selectForLocale(const QString& body, const QString& locale);
[[nodiscard]] QString selectForLocales(const QString& body, const QStringList& locales);

} // namespace ReleaseNotesLocalizer

#endif // REARK_RELEASE_NOTES_LOCALIZER_H
