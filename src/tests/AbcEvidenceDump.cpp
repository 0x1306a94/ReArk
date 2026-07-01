#include "core/HyleDecompiler.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QStringList>
#include <QTextStream>

#include <memory>

namespace {

int usage()
{
    QTextStream out(stdout);
    out << "usage: reark_abc_evidence_dump <hap-or-abc> <command> [args...]\n"
        << "commands:\n"
        << "  tree [limit]\n"
        << "  strings <pattern> [limit]\n"
        << "  literal-strings <pattern> [limit]\n"
        << "  literal <offset>\n"
        << "  xrefs <query> <kind> [limit]\n"
        << "  flows <query> <kind> [limit]\n"
        << "  sources <pattern>\n"
        << "  disasm <pattern>\n";
    return 2;
}

QString argOr(const QStringList& args, int index, const QString& fallback)
{
    return index < args.size() ? args.at(index) : fallback;
}

int intArgOr(const QStringList& args, int index, int fallback)
{
    if (index >= args.size()) {
        return fallback;
    }
    bool ok = false;
    const int value = args.at(index).toInt(&ok);
    return ok ? value : fallback;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() < 3) {
        return usage();
    }

    const QString packagePath = args.at(1);
    const QString command = args.at(2);
    if (!QFileInfo::exists(packagePath)) {
        QTextStream(stderr) << "package does not exist: " << packagePath << '\n';
        return 1;
    }

    auto context = std::make_shared<HyleDecompiler::SessionContext>();
    const HyleDecompiler::OpenResult open = HyleDecompiler::openFile(packagePath, context);
    if (!open.error.isEmpty()) {
        QTextStream(stderr) << open.error << '\n';
        return 1;
    }

    constexpr int kMaxChars = 120000;
    const QString pathQuery = QStringLiteral("modules.abc");
    QTextStream out(stdout);

    if (command == QStringLiteral("tree")) {
        out << HyleDecompiler::readAbcTreeEvidence(
            context, packagePath, pathQuery, intArgOr(args, 3, 80), kMaxChars);
        return 0;
    }

    if (command == QStringLiteral("strings")) {
        out << HyleDecompiler::searchAbcStringEvidence(
            context,
            packagePath,
            pathQuery,
            argOr(args, 3, {}),
            1,
            0,
            intArgOr(args, 4, 120),
            kMaxChars);
        return 0;
    }

    if (command == QStringLiteral("literal-strings")) {
        out << HyleDecompiler::searchAbcLiteralStringEvidence(
            context,
            packagePath,
            pathQuery,
            argOr(args, 3, {}),
            1,
            0,
            intArgOr(args, 4, 120),
            kMaxChars);
        return 0;
    }

    if (command == QStringLiteral("literal")) {
        if (args.size() < 4) {
            return usage();
        }
        out << HyleDecompiler::readAbcLiteralEvidence(
            context, packagePath, pathQuery, args.at(3), kMaxChars);
        return 0;
    }

    if (command == QStringLiteral("xrefs")) {
        if (args.size() < 5) {
            return usage();
        }
        out << HyleDecompiler::findAbcXrefEvidence(
            context,
            packagePath,
            pathQuery,
            args.at(3),
            args.at(4),
            intArgOr(args, 5, 80),
            kMaxChars);
        return 0;
    }

    if (command == QStringLiteral("flows")) {
        if (args.size() < 5) {
            return usage();
        }
        out << HyleDecompiler::findAbcCallArgumentFlowEvidence(
            context,
            packagePath,
            pathQuery,
            args.at(3),
            args.at(4),
            intArgOr(args, 5, 80),
            kMaxChars);
        return 0;
    }

    if (command == QStringLiteral("sources")) {
        const QString pattern = argOr(args, 3, {}).toCaseFolded();
        for (int i = 0; i < static_cast<int>(open.files.size()); ++i) {
            const DecompiledSourceFile& file = open.files.at(i);
            const QString haystack = QStringList { file.name, file.kind, file.section }.join(QLatin1Char(' ')).toCaseFolded();
            if (!pattern.isEmpty() && !haystack.contains(pattern)) {
                continue;
            }
            out << "\n# source[" << i << "] " << file.name
                << " kind=" << file.kind
                << " hyleId=" << QString::number(file.hyleId)
                << " lazy=" << (file.lazy ? "true" : "false")
                << " disassemblable=" << (file.disassemblable ? "true" : "false")
                << "\n";
            if (file.lazy) {
                const HyleDecompiler::SourceResult source = HyleDecompiler::decompileSourceFile(
                    context,
                    i,
                    file.hyleId,
                    file.name,
                    context->stopToken(),
                    file.packageId);
                if (!source.error.isEmpty()) {
                    out << source.error << '\n';
                } else {
                    out << source.content << '\n';
                }
            } else if (!file.content.isEmpty()) {
                out << file.content << '\n';
            }
        }
        return 0;
    }

    if (command == QStringLiteral("disasm")) {
        const QString pattern = argOr(args, 3, {}).toCaseFolded();
        for (int i = 0; i < static_cast<int>(open.files.size()); ++i) {
            const DecompiledSourceFile& file = open.files.at(i);
            const QString haystack = QStringList { file.name, file.kind, file.section }.join(QLatin1Char(' ')).toCaseFolded();
            if (!file.disassemblable || (!pattern.isEmpty() && !haystack.contains(pattern))) {
                continue;
            }
            out << "\n# disasm[" << i << "] " << file.name
                << " hyleId=" << QString::number(file.hyleId)
                << "\n";
            const HyleDecompiler::DisassemblyResult disasm = HyleDecompiler::disassembleSourceFileText(
                context,
                i,
                file.hyleId,
                file.name,
                context->stopToken(),
                file.packageId);
            if (!disasm.error.isEmpty()) {
                out << disasm.error << '\n';
            } else {
                out << disasm.content << '\n';
            }
        }
        return 0;
    }

    return usage();
}
