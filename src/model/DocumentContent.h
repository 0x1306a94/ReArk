#ifndef REARK_DOCUMENT_CONTENT_H
#define REARK_DOCUMENT_CONTENT_H

#include <QByteArray>
#include <QString>

struct DocumentContent {
    QString text;
    QString disassembly;
    QByteArray binary;
    QString diagnostics;
    QString kind;
    QString contentMode = QStringLiteral("text");
    bool disassemblyLoaded = false;
};

#endif // REARK_DOCUMENT_CONTENT_H
