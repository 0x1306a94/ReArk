#include "device/HarmonyPackageRewriter.h"

#include "signing/HarmonyHapSigner.h"

#include <hyle/hap/hap.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QTemporaryDir>

#include <algorithm>
#include <cstddef>
#include <vector>

namespace {

std::string toStdString(const QString& value)
{
    const QByteArray utf8 = value.toUtf8();
    return std::string(utf8.constData(), static_cast<std::size_t>(utf8.size()));
}

QString fromStdString(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

bool hasAbcSuffix(const QString& path)
{
    return path.endsWith(QStringLiteral(".abc"), Qt::CaseInsensitive);
}

bool isBundleMetadataResource(const QString& path)
{
    const QString normalized = QDir::cleanPath(path).toCaseFolded();
    return normalized == QStringLiteral("module.json")
        || normalized == QStringLiteral("pack.info")
        || normalized == QStringLiteral("pkgcontextinfo.json");
}

QByteArray bytesToByteArray(const std::vector<std::byte>& bytes)
{
    return QByteArray(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<qsizetype>(bytes.size()));
}

std::vector<std::byte> byteArrayToBytes(const QByteArray& bytes)
{
    std::vector<std::byte> result(static_cast<std::size_t>(bytes.size()));
    std::transform(bytes.cbegin(), bytes.cend(), result.begin(), [](char ch) {
        return static_cast<std::byte>(static_cast<unsigned char>(ch));
    });
    return result;
}

QString patchKindName(hyle::hap::abc_patch_item_kind kind)
{
    using hyle::hap::abc_patch_item_kind;
    switch (kind) {
    case abc_patch_item_kind::protection_flag:
        return QStringLiteral("protection_flag");
    case abc_patch_item_kind::string_exact:
        return QStringLiteral("string_exact");
    case abc_patch_item_kind::string_rewrite:
        return QStringLiteral("string_rewrite");
    case abc_patch_item_kind::class_name_rewrite:
        return QStringLiteral("class_name_rewrite");
    }
    return QStringLiteral("unknown");
}

QString cleanResourcePath(const std::string& rawPath)
{
    QString path = fromStdString(rawPath).trimmed();
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (path.startsWith(QLatin1Char('/'))) {
        path.remove(0, 1);
    }
    return QDir::cleanPath(path);
}

QString safeOutputPath(const QString& rootPath, const QString& resourcePath)
{
    if (resourcePath.isEmpty()
        || resourcePath == QStringLiteral(".")
        || resourcePath.contains(QLatin1Char(':'))
        || resourcePath.startsWith(QStringLiteral("../"))
        || resourcePath.contains(QStringLiteral("/../"))) {
        return {};
    }

    const QString root = QDir::cleanPath(QFileInfo(rootPath).absoluteFilePath()).replace(QLatin1Char('\\'), QLatin1Char('/'));
    const QString candidate = QDir::cleanPath(QFileInfo(QDir(root).filePath(resourcePath)).absoluteFilePath())
        .replace(QLatin1Char('\\'), QLatin1Char('/'));
    return (candidate == root || candidate.startsWith(root + QChar::fromLatin1('/')))
        ? candidate
        : QString();
}

bool writeBytes(const QString& path, const std::vector<std::byte>& bytes, QString* error)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error != nullptr) {
            *error = QStringLiteral("Cannot write %1").arg(path);
        }
        return false;
    }

    const auto* data = reinterpret_cast<const char*>(bytes.data());
    if (file.write(data, static_cast<qint64>(bytes.size())) != static_cast<qint64>(bytes.size())) {
        if (error != nullptr) {
            *error = QStringLiteral("Cannot write all bytes to %1").arg(path);
        }
        return false;
    }
    return true;
}

QString formatPatchReport(
    const hyle::hap::abc_patch_report& report,
    const QString& resourcePath,
    qsizetype inputBytes,
    qsizetype outputBytes)
{
    QString text;
    text += QStringLiteral("[abc rewrite] %1\n").arg(resourcePath);
    text += QStringLiteral("input_bytes=%1\n").arg(inputBytes);
    text += QStringLiteral("output_bytes=%1\n").arg(outputBytes);
    text += QStringLiteral("replaced_strings=%1\n").arg(report.replaced_strings);
    text += QStringLiteral("replaced_class_names=%1\n").arg(report.replaced_class_names);
    text += QStringLiteral("updated_string_refs=%1\n").arg(report.updated_string_refs);
    text += QStringLiteral("updated_literal_refs=%1\n").arg(report.updated_literal_refs);
    text += QStringLiteral("updated_method_refs=%1\n").arg(report.updated_method_refs);
    text += QStringLiteral("patch_items=%1\n").arg(report.items.size());

    const std::size_t maxItems = std::min<std::size_t>(report.items.size(), 80);
    for (std::size_t i = 0; i < maxItems; ++i) {
        const auto& item = report.items.at(i);
        text += QStringLiteral("- kind=%1 old_offset=0x%2 new_offset=0x%3 ref_count=%4 old=\"%5\" new=\"%6\"\n")
            .arg(patchKindName(item.kind))
            .arg(item.old_offset, 0, 16)
            .arg(item.new_offset, 0, 16)
            .arg(item.ref_count)
            .arg(fromStdString(item.old_value))
            .arg(fromStdString(item.new_value));
    }
    if (report.items.size() > maxItems) {
        text += QStringLiteral("- [truncated %1 more item(s)]\n").arg(report.items.size() - maxItems);
    }
    return text.trimmed();
}

QJsonValue rewriteJsonStrings(
    const QJsonValue& value,
    const QString& oldBundleName,
    const QString& newBundleName,
    int* replacementCount)
{
    if (value.isString()) {
        QString text = value.toString();
        const int occurrences = text.count(oldBundleName);
        if (occurrences > 0) {
            text.replace(oldBundleName, newBundleName);
            if (replacementCount != nullptr) {
                *replacementCount += occurrences;
            }
        }
        return text;
    }

    if (value.isArray()) {
        QJsonArray rewritten;
        for (const QJsonValue& item : value.toArray()) {
            rewritten.append(rewriteJsonStrings(item, oldBundleName, newBundleName, replacementCount));
        }
        return rewritten;
    }

    if (value.isObject()) {
        QJsonObject rewritten;
        const QJsonObject object = value.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            rewritten.insert(it.key(), rewriteJsonStrings(it.value(), oldBundleName, newBundleName, replacementCount));
        }
        return rewritten;
    }

    return value;
}

bool rewriteMetadataBundle(
    std::vector<std::byte>* bytes,
    const QString& oldBundleName,
    const QString& newBundleName,
    QString* error,
    int* replacementCount)
{
    if (bytes == nullptr) {
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(bytesToByteArray(*bytes), &parseError);
    if (document.isNull()) {
        if (error != nullptr) {
            *error = QStringLiteral("Parse HAP metadata JSON failed: %1").arg(parseError.errorString());
        }
        return false;
    }

    int replacements = 0;
    QJsonDocument rewrittenDocument;
    if (document.isArray()) {
        rewrittenDocument = QJsonDocument(rewriteJsonStrings(
            QJsonValue(document.array()),
            oldBundleName,
            newBundleName,
            &replacements).toArray());
    } else if (document.isObject()) {
        rewrittenDocument = QJsonDocument(rewriteJsonStrings(
            QJsonValue(document.object()),
            oldBundleName,
            newBundleName,
            &replacements).toObject());
    } else {
        return false;
    }

    if (replacements <= 0) {
        return false;
    }

    *bytes = byteArrayToBytes(rewrittenDocument.toJson(QJsonDocument::Compact));
    if (replacementCount != nullptr) {
        *replacementCount = replacements;
    }
    return true;
}

QString requiredPackingInputError(const QString& unpackedRoot)
{
    const QDir root(unpackedRoot);
    const QStringList required {
        QStringLiteral("module.json"),
        QStringLiteral("ets"),
        QStringLiteral("resources"),
        QStringLiteral("pack.info"),
        QStringLiteral("resources.index")
    };
    for (const QString& relative : required) {
        if (!QFileInfo::exists(root.filePath(relative))) {
            return QStringLiteral("Repacked HAP input is missing %1").arg(root.filePath(relative));
        }
    }
    return {};
}

} // namespace

HarmonyBundleRewriteResult HarmonyPackageRewriter::rewriteBundleIdentity(
    const HarmonyBundleRewriteRequest& request)
{
    HarmonyBundleRewriteResult result;
    result.inputHapPath = request.inputHapPath;
    result.outputHapPath = request.outputHapPath;

    const QFileInfo inputInfo(request.inputHapPath);
    if (!inputInfo.exists() || !inputInfo.isFile()) {
        result.error = QStringLiteral("Input HAP does not exist: %1").arg(request.inputHapPath);
        return result;
    }
    if (request.oldBundleName.trimmed().isEmpty() || request.newBundleName.trimmed().isEmpty()) {
        result.error = QStringLiteral("Both old and new bundle names are required.");
        return result;
    }
    if (request.outputHapPath.trimmed().isEmpty()) {
        result.error = QStringLiteral("Output HAP path is required.");
        return result;
    }
    if (request.stopToken.stop_requested()) {
        result.error = QStringLiteral("Bundle rewrite cancelled.");
        return result;
    }

    QTemporaryDir unpacked(QStringLiteral("%1/ReArk-rewrite-hap-XXXXXX").arg(QDir::tempPath()));
    if (!unpacked.isValid()) {
        result.error = QStringLiteral("Could not create temporary HAP rewrite directory.");
        return result;
    }
    result.unpackedDirectory = unpacked.path();

    auto session = hyle::hap::open_decompiled_package(toStdString(request.inputHapPath));
    if (!session) {
        result.error = QStringLiteral("Open HAP failed: %1").arg(fromStdString(session.error().message()));
        return result;
    }

    QStringList reports;
    int abcCount = 0;
    int rewrittenAbcCount = 0;
    int rewrittenMetadataCount = 0;
    for (const auto& resource : session->resources()) {
        if (request.stopToken.stop_requested()) {
            result.error = QStringLiteral("Bundle rewrite cancelled.");
            result.report = reports.join(QStringLiteral("\n\n"));
            return result;
        }

        const QString resourcePath = cleanResourcePath(resource.path);
        const QString outputPath = safeOutputPath(unpacked.path(), resourcePath);
        if (outputPath.isEmpty()) {
            result.error = QStringLiteral("Unsafe HAP resource path: %1").arg(fromStdString(resource.path));
            return result;
        }

        if (resource.is_directory) {
            QDir().mkpath(outputPath);
            continue;
        }

        auto content = session->read_resource(resource.id);
        if (!content) {
            result.error = QStringLiteral("Read HAP resource failed: %1: %2")
                .arg(resourcePath, fromStdString(content.error().message()));
            return result;
        }

        std::vector<std::byte> bytes = content->bytes;
        if (isBundleMetadataResource(resourcePath)) {
            QString metadataError;
            int metadataReplacements = 0;
            if (rewriteMetadataBundle(
                    &bytes,
                    request.oldBundleName,
                    request.newBundleName,
                    &metadataError,
                    &metadataReplacements)) {
                ++rewrittenMetadataCount;
                reports.append(QStringLiteral("[hap metadata rewrite] %1\nold=\"%2\"\nnew=\"%3\"\nreplacements=%4")
                    .arg(resourcePath, request.oldBundleName, request.newBundleName)
                    .arg(metadataReplacements));
            } else if (!metadataError.isEmpty()) {
                result.error = QStringLiteral("%1: %2").arg(resourcePath, metadataError);
                result.report = reports.join(QStringLiteral("\n\n"));
                return result;
            }
        }

        if (resource.is_abc || resource.kind == hyle::hap::hap_resource_kind::abc || hasAbcSuffix(resourcePath)) {
            ++abcCount;
            hyle::hap::abc_parser parser(bytes);
            auto parsed = parser.parse();
            if (!parsed) {
                result.error = QStringLiteral("Parse ABC failed: %1: %2")
                    .arg(resourcePath, fromStdString(parsed.error().message()));
                return result;
            }

            hyle::hap::abc_modifier modifier(parser);
            auto replaced = modifier.replace_bundle_identity(
                toStdString(request.oldBundleName),
                toStdString(request.newBundleName));
            if (!replaced) {
                result.error = QStringLiteral("Rewrite bundle identity failed: %1: %2")
                    .arg(resourcePath, fromStdString(replaced.error().message()));
                return result;
            }

            auto patched = modifier.build({ true });
            if (!patched) {
                result.error = QStringLiteral("Build rewritten ABC failed: %1: %2")
                    .arg(resourcePath, fromStdString(patched.error().message()));
                return result;
            }

            const auto verify = hyle::hap::verify_abc(*patched);
            if (!verify.valid) {
                result.error = QStringLiteral("Verify rewritten ABC failed: %1: %2")
                    .arg(resourcePath, fromStdString(verify.message));
                return result;
            }

            if (!modifier.patch_report().items.empty()) {
                ++rewrittenAbcCount;
            }
            reports.append(formatPatchReport(
                modifier.patch_report(),
                resourcePath,
                static_cast<qsizetype>(bytes.size()),
                static_cast<qsizetype>(patched->size())));
            bytes = std::move(*patched);
        }

        QString writeError;
        if (!writeBytes(outputPath, bytes, &writeError)) {
            result.error = writeError;
            return result;
        }
    }

    QDir(unpacked.path()).mkpath(QStringLiteral("ets"));
    QDir(unpacked.path()).mkpath(QStringLiteral("libs"));
    QDir(unpacked.path()).mkpath(QStringLiteral("resources"));

    const QString packingInputError = requiredPackingInputError(unpacked.path());
    if (!packingInputError.isEmpty()) {
        result.error = packingInputError;
        return result;
    }
    if (abcCount == 0) {
        result.error = QStringLiteral("HAP contains no ABC resource to rewrite: %1").arg(request.inputHapPath);
        return result;
    }
    if (rewrittenAbcCount == 0) {
        result.error = QStringLiteral("Hyle did not rewrite any ABC item for bundle %1.")
            .arg(request.oldBundleName);
        result.report = reports.join(QStringLiteral("\n\n"));
        return result;
    }
    if (rewrittenMetadataCount == 0) {
        result.error = QStringLiteral("HAP metadata did not contain bundle %1.")
            .arg(request.oldBundleName);
        result.report = reports.join(QStringLiteral("\n\n"));
        return result;
    }
    if (request.stopToken.stop_requested()) {
        result.error = QStringLiteral("Bundle rewrite cancelled.");
        result.report = reports.join(QStringLiteral("\n\n"));
        return result;
    }

    result.packingResult = CommandRunner::runBlocking(HarmonyHapSigner::packCommand({
        .javaProgram = request.javaProgram,
        .packingToolPath = request.packingToolPath,
        .unpackedDirectory = unpacked.path(),
        .outputHapPath = request.outputHapPath,
        .timeoutMs = request.timeoutMs
    }), request.stopToken);
    if (!result.packingResult.succeeded()) {
        unpacked.setAutoRemove(false);
        result.unpackedDirectory = unpacked.path();
        result.error = QStringLiteral("Official HAP packing failed.");
        result.report = reports.join(QStringLiteral("\n\n"));
        return result;
    }

    result.report = reports.join(QStringLiteral("\n\n"));
    result.ok = true;
    return result;
}
