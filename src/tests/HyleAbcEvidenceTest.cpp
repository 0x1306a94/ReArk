#include "core/HyleDecompiler.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QRegularExpression>
#include <QString>
#include <QTextStream>

#include <memory>
#include <vector>

namespace {

int fail(const QString& message, const QString& evidence = {})
{
    QTextStream err(stderr);
    err << "FAIL: " << message << '\n';
    if (!evidence.isEmpty()) {
        err << evidence.left(3000) << '\n';
    }
    return 1;
}

bool hasOkStatus(const QString& evidence)
{
    return evidence.contains(QStringLiteral("# status: ok"));
}

std::vector<QString> allCaptures(const QString& text, const QString& pattern)
{
    const QRegularExpression regex(pattern);
    QRegularExpressionMatchIterator it = regex.globalMatch(text);
    std::vector<QString> captures;
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        captures.push_back(match.captured(1));
    }
    return captures;
}

QString compactHexOffset(QString offset)
{
    offset = offset.trimmed();
    if (offset.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        offset = offset.mid(2);
    }
    bool ok = false;
    const qulonglong value = offset.toULongLong(&ok, 16);
    if (!ok) {
        return offset;
    }
    return QStringLiteral("0x%1").arg(value, 0, 16);
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    const QString samplePath = argc > 1
        ? QString::fromLocal8Bit(argv[1])
        : QString::fromUtf8(REARK_ABC_EVIDENCE_SAMPLE);
    if (!QFileInfo::exists(samplePath)) {
        return fail(QStringLiteral("Sample HAP is missing: %1").arg(samplePath));
    }

    auto context = std::make_shared<HyleDecompiler::SessionContext>();
    const auto openResult = HyleDecompiler::openFile(samplePath, context);
    if (!openResult.error.isEmpty()) {
        return fail(QStringLiteral("Open sample failed."), openResult.error);
    }
    if (!context || context->packages.empty()) {
        return fail(QStringLiteral("Open sample did not create a package session."));
    }

    const QString pathQuery = QStringLiteral("modules.abc");

    const QString tree = HyleDecompiler::readAbcTreeEvidence(
        context, samplePath, pathQuery, 8, 12000);
    if (!hasOkStatus(tree) || !tree.contains(QStringLiteral("# literals:"))) {
        return fail(QStringLiteral("ABC tree evidence is not usable."), tree);
    }

    const QString strings = HyleDecompiler::searchAbcStringEvidence(
        context, samplePath, pathQuery, {}, 4, 0, 40, 20000);
    if (!hasOkStatus(strings)
        || !strings.contains(QStringLiteral("# scope: all_strings"))
        || !strings.contains(QStringLiteral("  source: "))) {
        return fail(QStringLiteral("ABC string search returned no evidence."), strings);
    }

    const QString literalStrings = HyleDecompiler::searchAbcLiteralStringEvidence(
        context, samplePath, pathQuery, {}, 4, 0, 80, 24000);
    if (!hasOkStatus(literalStrings) || !literalStrings.contains(QStringLiteral("# scope: literal_strings"))) {
        return fail(QStringLiteral("ABC literal string search returned no evidence."), literalStrings);
    }

    const auto literalOffsets = allCaptures(
        literalStrings, QStringLiteral(R"(container_offset:\s+(0[xX][0-9A-Fa-f]+))"));
    if (literalOffsets.empty()) {
        return fail(QStringLiteral("ABC literal string search produced no literal container offsets."), literalStrings);
    }

    QString literal;
    QString compactOffset;
    QString xrefs;
    for (const QString& literalOffset : literalOffsets) {
        literal = HyleDecompiler::readAbcLiteralEvidence(
            context, samplePath, pathQuery, literalOffset, 16000);
        if (!hasOkStatus(literal) || !literal.contains(QStringLiteral("# literal_offset:"))) {
            continue;
        }

        compactOffset = compactHexOffset(literalOffset);
        xrefs = HyleDecompiler::findAbcXrefEvidence(
            context, samplePath, pathQuery, compactOffset, QStringLiteral("literal"), 20, 16000);
        if (hasOkStatus(xrefs)
            && !xrefs.contains(QStringLiteral("[no ABC xrefs matched]"))
            && xrefs.contains(QStringLiteral("- xref["))) {
            break;
        }
        compactOffset.clear();
    }
    if (compactOffset.isEmpty()) {
        return fail(QStringLiteral("No literal container offset had a bytecode xref."), xrefs);
    }

    const QString flows = HyleDecompiler::findAbcCallArgumentFlowEvidence(
        context, samplePath, pathQuery, compactOffset, QStringLiteral("literal"), 20, 16000);
    if (!hasOkStatus(flows) || !flows.contains(QStringLiteral("# match_count:"))) {
        return fail(QStringLiteral("ABC call argument flow lookup did not return structured evidence."), flows);
    }

    QTextStream(stdout) << "ABC evidence wrappers passed for " << samplePath << '\n';
    return 0;
}
