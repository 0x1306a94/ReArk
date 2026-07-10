#include "device/HarmonyPackageRewriter.h"

#include "signing/HarmonyHapSigner.h"

#include <hyle/hap/core/abc_checksum.h>
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
#include <QTextStream>

#include <algorithm>
#include <cstddef>
#include <cstdint>
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

QString formatStringRewriteReport(
    const hyle::hap::abc_patch_report& report,
    const QString& resourcePath,
    qsizetype inputBytes,
    qsizetype outputBytes,
    int matchedStrings)
{
    QString text = formatPatchReport(report, resourcePath, inputBytes, outputBytes);
    text += QStringLiteral("\nmatched_strings=%1").arg(matchedStrings);
    return text.trimmed();
}

std::vector<qsizetype> findRawByteOccurrences(
    const std::vector<std::byte>& bytes,
    const std::string& needle)
{
    std::vector<qsizetype> offsets;
    if (needle.empty() || bytes.empty()) {
        return offsets;
    }

    auto it = bytes.cbegin();
    while (it != bytes.cend()) {
        it = std::search(
            it,
            bytes.cend(),
            needle.cbegin(),
            needle.cend(),
            [](std::byte lhs, char rhs) {
                return static_cast<unsigned char>(lhs)
                    == static_cast<unsigned char>(rhs);
            });
        if (it == bytes.cend()) {
            break;
        }
        offsets.push_back(std::distance(bytes.cbegin(), it));
        ++it;
    }
    return offsets;
}

void replaceRawBytesAt(
    std::vector<std::byte>* bytes,
    qsizetype offset,
    const std::string& replacement)
{
    if (bytes == nullptr) {
        return;
    }
    for (qsizetype i = 0; i < static_cast<qsizetype>(replacement.size()); ++i) {
        bytes->at(static_cast<std::size_t>(offset + i)) =
            static_cast<std::byte>(static_cast<unsigned char>(replacement.at(static_cast<std::size_t>(i))));
    }
}

QString formatInPlaceStringRewriteReport(
    const QString& resourcePath,
    qsizetype inputBytes,
    qsizetype outputBytes,
    int matchedStrings,
    const std::vector<qsizetype>& rawOffsets,
    const QString& oldText,
    const QString& newText)
{
    QString text;
    text += QStringLiteral("[abc rewrite] %1\n").arg(resourcePath);
    text += QStringLiteral("input_bytes=%1\n").arg(inputBytes);
    text += QStringLiteral("output_bytes=%1\n").arg(outputBytes);
    text += QStringLiteral("strategy=in_place_equal_length\n");
    text += QStringLiteral("matched_strings=%1\n").arg(matchedStrings);
    text += QStringLiteral("raw_occurrences=%1\n").arg(rawOffsets.size());
    const std::size_t maxItems = std::min<std::size_t>(rawOffsets.size(), 80);
    for (std::size_t i = 0; i < maxItems; ++i) {
        text += QStringLiteral("- kind=string_in_place raw_offset=0x%1 old=\"%2\" new=\"%3\"\n")
            .arg(rawOffsets.at(i), 0, 16)
            .arg(oldText, newText);
    }
    if (rawOffsets.size() > maxItems) {
        text += QStringLiteral("- [truncated %1 more item(s)]\n").arg(rawOffsets.size() - maxItems);
    }
    return text.trimmed();
}

void tracePackageRewrite(const QString& message)
{
    if (!qEnvironmentVariableIsSet("REARK_TRACE_PACKAGE_REWRITE")) {
        return;
    }
    QTextStream(stderr) << "[package rewrite] " << message << '\n';
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

bool stripModuleRequestPermissions(
    std::vector<std::byte>* bytes,
    const QStringList& permissions,
    QString* error,
    int* removedCount)
{
    if (bytes == nullptr || permissions.isEmpty()) {
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(bytesToByteArray(*bytes), &parseError);
    if (!document.isObject()) {
        if (error != nullptr && document.isNull()) {
            *error = QStringLiteral("Parse module.json failed while stripping permissions: %1")
                .arg(parseError.errorString());
        }
        return false;
    }

    QJsonObject root = document.object();
    QJsonObject module = root.value(QStringLiteral("module")).toObject();
    QJsonArray requestPermissions = module.value(QStringLiteral("requestPermissions")).toArray();
    if (requestPermissions.isEmpty()) {
        return false;
    }

    QJsonArray kept;
    int removed = 0;
    for (const QJsonValue& value : requestPermissions) {
        const QString name = value.toObject().value(QStringLiteral("name")).toString().trimmed();
        if (!name.isEmpty() && permissions.contains(name, Qt::CaseSensitive)) {
            ++removed;
            continue;
        }
        kept.append(value);
    }

    if (removed <= 0) {
        return false;
    }

    module.insert(QStringLiteral("requestPermissions"), kept);
    root.insert(QStringLiteral("module"), module);
    *bytes = byteArrayToBytes(QJsonDocument(root).toJson(QJsonDocument::Compact));
    if (removedCount != nullptr) {
        *removedCount = removed;
    }
    return true;
}

int encodedHarmonyApiVersion(int api)
{
    if (api <= 0) {
        return 0;
    }
    if (api == 17) {
        return 50005017;
    }
    if (api == 22) {
        return 60002022;
    }
    if (api == 23) {
        return 60100023;
    }
    if (api == 24) {
        return 60101024;
    }
    return api;
}

int compatibleApiValueForExistingField(int api, const QJsonValue& currentValue)
{
    if (api <= 0) {
        return 0;
    }
    if (currentValue.toInt() > 1000) {
        return encodedHarmonyApiVersion(api);
    }
    return api;
}

bool ensureStringArrayContains(QJsonObject* object, const QString& key, const QStringList& values)
{
    if (object == nullptr || values.isEmpty()) {
        return false;
    }

    QJsonArray array = object->value(key).toArray();
    bool changed = false;
    for (const QString& rawValue : values) {
        const QString value = rawValue.trimmed();
        if (value.isEmpty()) {
            continue;
        }

        bool found = false;
        for (const QJsonValue& item : array) {
            if (item.toString() == value) {
                found = true;
                break;
            }
        }
        if (!found) {
            array.append(value);
            changed = true;
        }
    }

    if (changed) {
        object->insert(key, array);
    }
    return changed;
}

bool patchModuleCompatibilityMetadata(
    std::vector<std::byte>* bytes,
    const QStringList& forcedDeviceTypes,
    int forcedCompatibleApi,
    int forcedTargetApi,
    QString* error,
    QStringList* changes)
{
    if (bytes == nullptr
        || (forcedDeviceTypes.isEmpty() && forcedCompatibleApi <= 0 && forcedTargetApi <= 0)) {
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(bytesToByteArray(*bytes), &parseError);
    if (!document.isObject()) {
        if (error != nullptr && document.isNull()) {
            *error = QStringLiteral("Parse module.json failed while patching compatibility metadata: %1")
                .arg(parseError.errorString());
        }
        return false;
    }

    QJsonObject root = document.object();
    QJsonObject app = root.value(QStringLiteral("app")).toObject();
    QJsonObject module = root.value(QStringLiteral("module")).toObject();
    bool changed = false;

    if (!forcedDeviceTypes.isEmpty()
        && ensureStringArrayContains(&module, QStringLiteral("deviceTypes"), forcedDeviceTypes)) {
        changed = true;
        if (changes != nullptr) {
            changes->append(QStringLiteral("module.deviceTypes+=%1").arg(forcedDeviceTypes.join(QStringLiteral(","))));
        }
    }

    if (forcedCompatibleApi > 0) {
        const QJsonValue currentValue = app.value(QStringLiteral("minAPIVersion"));
        const int patchedValue = compatibleApiValueForExistingField(forcedCompatibleApi, currentValue);
        if (currentValue.toInt() != patchedValue) {
            app.insert(QStringLiteral("minAPIVersion"), patchedValue);
            changed = true;
            if (changes != nullptr) {
                changes->append(QStringLiteral("app.minAPIVersion=%1").arg(patchedValue));
            }
        }
    }

    if (forcedTargetApi > 0) {
        const QJsonValue currentValue = app.value(QStringLiteral("targetAPIVersion"));
        const int patchedValue = compatibleApiValueForExistingField(forcedTargetApi, currentValue);
        if (currentValue.toInt() != patchedValue) {
            app.insert(QStringLiteral("targetAPIVersion"), patchedValue);
            changed = true;
            if (changes != nullptr) {
                changes->append(QStringLiteral("app.targetAPIVersion=%1").arg(patchedValue));
            }
        }
    }

    if (!changed) {
        return false;
    }

    root.insert(QStringLiteral("app"), app);
    root.insert(QStringLiteral("module"), module);
    *bytes = byteArrayToBytes(QJsonDocument(root).toJson(QJsonDocument::Compact));
    return true;
}

bool patchPackInfoCompatibilityMetadata(
    std::vector<std::byte>* bytes,
    const QStringList& forcedDeviceTypes,
    int forcedCompatibleApi,
    int forcedTargetApi,
    QString* error,
    QStringList* changes)
{
    if (bytes == nullptr
        || (forcedDeviceTypes.isEmpty() && forcedCompatibleApi <= 0 && forcedTargetApi <= 0)) {
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(bytesToByteArray(*bytes), &parseError);
    if (!document.isObject()) {
        if (error != nullptr && document.isNull()) {
            *error = QStringLiteral("Parse pack.info failed while patching compatibility metadata: %1")
                .arg(parseError.errorString());
        }
        return false;
    }

    QJsonObject root = document.object();
    QJsonObject summary = root.value(QStringLiteral("summary")).toObject();
    QJsonArray modules = summary.value(QStringLiteral("modules")).toArray();
    QJsonArray rewrittenModules;
    bool changed = false;

    for (const QJsonValue& value : modules) {
        QJsonObject module = value.toObject();
        if (!forcedDeviceTypes.isEmpty()
            && ensureStringArrayContains(&module, QStringLiteral("deviceType"), forcedDeviceTypes)) {
            changed = true;
            if (changes != nullptr) {
                changes->append(QStringLiteral("pack.summary.modules.deviceType+=%1")
                    .arg(forcedDeviceTypes.join(QStringLiteral(","))));
            }
        }

        QJsonObject apiVersion = module.value(QStringLiteral("apiVersion")).toObject();
        if (forcedCompatibleApi > 0
            && apiVersion.value(QStringLiteral("compatible")).toInt() != forcedCompatibleApi) {
            apiVersion.insert(QStringLiteral("compatible"), forcedCompatibleApi);
            changed = true;
            if (changes != nullptr) {
                changes->append(QStringLiteral("pack.summary.modules.apiVersion.compatible=%1")
                    .arg(forcedCompatibleApi));
            }
        }
        if (forcedTargetApi > 0
            && apiVersion.value(QStringLiteral("target")).toInt() != forcedTargetApi) {
            apiVersion.insert(QStringLiteral("target"), forcedTargetApi);
            changed = true;
            if (changes != nullptr) {
                changes->append(QStringLiteral("pack.summary.modules.apiVersion.target=%1")
                    .arg(forcedTargetApi));
            }
        }
        if (!apiVersion.isEmpty()) {
            module.insert(QStringLiteral("apiVersion"), apiVersion);
        }
        rewrittenModules.append(module);
    }

    QJsonArray packages = root.value(QStringLiteral("packages")).toArray();
    QJsonArray rewrittenPackages;
    for (const QJsonValue& value : packages) {
        QJsonObject package = value.toObject();
        if (!forcedDeviceTypes.isEmpty()
            && ensureStringArrayContains(&package, QStringLiteral("deviceType"), forcedDeviceTypes)) {
            changed = true;
            if (changes != nullptr) {
                changes->append(QStringLiteral("pack.packages.deviceType+=%1")
                    .arg(forcedDeviceTypes.join(QStringLiteral(","))));
            }
        }
        rewrittenPackages.append(package);
    }

    if (!changed) {
        return false;
    }

    summary.insert(QStringLiteral("modules"), rewrittenModules);
    root.insert(QStringLiteral("summary"), summary);
    root.insert(QStringLiteral("packages"), rewrittenPackages);
    *bytes = byteArrayToBytes(QJsonDocument(root).toJson(QJsonDocument::Compact));
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
    tracePackageRewrite(QStringLiteral("rewriteBundleIdentity start input=%1 output=%2 old=%3 new=%4")
        .arg(request.inputHapPath, request.outputHapPath, request.oldBundleName, request.newBundleName));

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
    tracePackageRewrite(QStringLiteral("opened HAP resources=%1").arg(session->resources().size()));

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
        tracePackageRewrite(QStringLiteral("resource %1").arg(resourcePath));
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

        if (resourcePath == QStringLiteral("module.json") && !request.strippedRequestPermissions.isEmpty()) {
            QString stripError;
            int removedPermissions = 0;
            if (stripModuleRequestPermissions(
                    &bytes,
                    request.strippedRequestPermissions,
                    &stripError,
                    &removedPermissions)) {
                reports.append(QStringLiteral("[hap permission strip] %1\npermissions=\"%2\"\nremoved=%3")
                    .arg(resourcePath, request.strippedRequestPermissions.join(QStringLiteral(",")))
                    .arg(removedPermissions));
            } else if (!stripError.isEmpty()) {
                result.error = QStringLiteral("%1: %2").arg(resourcePath, stripError);
                result.report = reports.join(QStringLiteral("\n\n"));
                return result;
            }
        }

        if (resourcePath == QStringLiteral("module.json")) {
            QString patchError;
            QStringList changes;
            if (patchModuleCompatibilityMetadata(
                    &bytes,
                    request.forcedDeviceTypes,
                    request.forcedCompatibleApi,
                    request.forcedTargetApi,
                    &patchError,
                    &changes)) {
                reports.append(QStringLiteral("[hap compatibility patch] %1\n%2")
                    .arg(resourcePath, changes.join(QStringLiteral("\n"))));
            } else if (!patchError.isEmpty()) {
                result.error = QStringLiteral("%1: %2").arg(resourcePath, patchError);
                result.report = reports.join(QStringLiteral("\n\n"));
                return result;
            }
        } else if (resourcePath == QStringLiteral("pack.info")) {
            QString patchError;
            QStringList changes;
            if (patchPackInfoCompatibilityMetadata(
                    &bytes,
                    request.forcedDeviceTypes,
                    request.forcedCompatibleApi,
                    request.forcedTargetApi,
                    &patchError,
                    &changes)) {
                reports.append(QStringLiteral("[hap compatibility patch] %1\n%2")
                    .arg(resourcePath, changes.join(QStringLiteral("\n"))));
            } else if (!patchError.isEmpty()) {
                result.error = QStringLiteral("%1: %2").arg(resourcePath, patchError);
                result.report = reports.join(QStringLiteral("\n\n"));
                return result;
            }
        }

        if (resource.is_abc || resource.kind == hyle::hap::hap_resource_kind::abc || hasAbcSuffix(resourcePath)) {
            ++abcCount;
            tracePackageRewrite(QStringLiteral("parse ABC %1 bytes=%2").arg(resourcePath).arg(bytes.size()));
            hyle::hap::abc_parser parser(bytes);
            auto parsed = parser.parse();
            if (!parsed) {
                result.error = QStringLiteral("Parse ABC failed: %1: %2")
                    .arg(resourcePath, fromStdString(parsed.error().message()));
                return result;
            }
            tracePackageRewrite(QStringLiteral("parsed ABC %1").arg(resourcePath));

            hyle::hap::abc_modifier modifier(parser);
            tracePackageRewrite(QStringLiteral("replace bundle identity %1").arg(resourcePath));
            auto replaced = modifier.replace_bundle_identity(
                toStdString(request.oldBundleName),
                toStdString(request.newBundleName));
            if (!replaced) {
                result.error = QStringLiteral("Rewrite bundle identity failed: %1: %2")
                    .arg(resourcePath, fromStdString(replaced.error().message()));
                return result;
            }
            tracePackageRewrite(QStringLiteral("replaced bundle identity %1").arg(resourcePath));

            tracePackageRewrite(QStringLiteral("build rewritten ABC %1").arg(resourcePath));
            auto patched = modifier.build({ true });
            if (!patched) {
                result.error = QStringLiteral("Build rewritten ABC failed: %1: %2")
                    .arg(resourcePath, fromStdString(patched.error().message()));
                return result;
            }
            tracePackageRewrite(QStringLiteral("built rewritten ABC %1 bytes=%2").arg(resourcePath).arg(patched->size()));

            tracePackageRewrite(QStringLiteral("verify rewritten ABC %1").arg(resourcePath));
            const auto verify = hyle::hap::verify_abc(*patched);
            if (!verify.valid) {
                result.error = QStringLiteral("Verify rewritten ABC failed: %1: %2")
                    .arg(resourcePath, fromStdString(verify.message));
                return result;
            }
            tracePackageRewrite(QStringLiteral("verified rewritten ABC %1").arg(resourcePath));

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
    tracePackageRewrite(QStringLiteral("pack finished success=%1").arg(result.packingResult.succeeded()));
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

HarmonyAbcStringRewriteResult HarmonyPackageRewriter::rewriteAbcString(
    const HarmonyAbcStringRewriteRequest& request)
{
    HarmonyAbcStringRewriteResult result;
    result.inputHapPath = request.inputHapPath;
    result.outputHapPath = request.outputHapPath;

    const QFileInfo inputInfo(request.inputHapPath);
    if (!inputInfo.exists() || !inputInfo.isFile()) {
        result.error = QStringLiteral("Input HAP does not exist: %1").arg(request.inputHapPath);
        return result;
    }
    if (request.oldText.isEmpty()) {
        result.error = QStringLiteral("oldText is required.");
        return result;
    }
    if (request.newText.isEmpty()) {
        result.error = QStringLiteral("newText is required.");
        return result;
    }
    if (request.outputHapPath.trimmed().isEmpty()) {
        result.error = QStringLiteral("Output HAP path is required.");
        return result;
    }
    if (request.stopToken.stop_requested()) {
        result.error = QStringLiteral("ABC string rewrite cancelled.");
        return result;
    }

    QTemporaryDir unpacked(QStringLiteral("%1/ReArk-rewrite-abc-string-XXXXXX").arg(QDir::tempPath()));
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

    const std::string oldText = toStdString(request.oldText);
    const std::string newText = toStdString(request.newText);
    QStringList reports;
    for (const auto& resource : session->resources()) {
        if (request.stopToken.stop_requested()) {
            result.error = QStringLiteral("ABC string rewrite cancelled.");
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
        if (resource.is_abc || resource.kind == hyle::hap::hap_resource_kind::abc || hasAbcSuffix(resourcePath)) {
            ++result.abcCount;
            hyle::hap::abc_parser parser(bytes);
            auto parsed = parser.parse();
            if (!parsed) {
                result.error = QStringLiteral("Parse ABC failed: %1: %2")
                    .arg(resourcePath, fromStdString(parsed.error().message()));
                return result;
            }

            const hyle::hap::abc_strings strings = parser.collect_all_strings();
            std::vector<std::uint32_t> offsets;
            for (const auto& item : strings) {
                if (item.second.data() == oldText) {
                    offsets.push_back(item.first);
                }
            }

            if (!offsets.empty()) {
                if (request.requireUnique && offsets.size() > 1) {
                    result.error = QStringLiteral("ABC string rewrite expected one match, found %1 in %2.")
                        .arg(offsets.size())
                        .arg(resourcePath);
                    result.report = reports.join(QStringLiteral("\n\n"));
                    return result;
                }

                if (oldText.size() == newText.size()) {
                    const std::vector<qsizetype> rawOffsets = findRawByteOccurrences(bytes, oldText);
                    if (rawOffsets.size() != offsets.size()) {
                        result.error = QStringLiteral(
                            "Equal-length ABC string rewrite expected raw byte occurrence count %1 to match parsed string count %2 in %3.")
                            .arg(rawOffsets.size())
                            .arg(offsets.size())
                            .arg(resourcePath);
                        result.report = reports.join(QStringLiteral("\n\n"));
                        return result;
                    }
                    if (request.requireUnique && rawOffsets.size() > 1) {
                        result.error = QStringLiteral("Equal-length ABC string rewrite expected one raw match, found %1 in %2.")
                            .arg(rawOffsets.size())
                            .arg(resourcePath);
                        result.report = reports.join(QStringLiteral("\n\n"));
                        return result;
                    }

                    for (qsizetype rawOffset : rawOffsets) {
                        replaceRawBytesAt(&bytes, rawOffset, newText);
                    }
                    hyle::hap::update_abc_checksum(bytes);

                    const auto verify = hyle::hap::verify_abc(bytes);
                    if (!verify.valid) {
                        result.error = QStringLiteral("Verify in-place rewritten ABC failed: %1: %2")
                            .arg(resourcePath, fromStdString(verify.message));
                        result.report = reports.join(QStringLiteral("\n\n"));
                        return result;
                    }

                    ++result.rewrittenAbcCount;
                    result.replacementCount += static_cast<int>(rawOffsets.size());
                    reports.append(formatInPlaceStringRewriteReport(
                        resourcePath,
                        static_cast<qsizetype>(content->bytes.size()),
                        static_cast<qsizetype>(bytes.size()),
                        static_cast<int>(offsets.size()),
                        rawOffsets,
                        request.oldText,
                        request.newText));
                    QString writeError;
                    if (!writeBytes(outputPath, bytes, &writeError)) {
                        result.error = writeError;
                        return result;
                    }
                    continue;
                }

                hyle::hap::abc_modifier modifier(parser);
                for (std::uint32_t offset : offsets) {
                    auto replaced = modifier.replace_string(offset, newText);
                    if (!replaced) {
                        result.error = QStringLiteral("Rewrite ABC string failed: %1 offset=0x%2: %3")
                            .arg(resourcePath)
                            .arg(offset, 0, 16)
                            .arg(fromStdString(replaced.error().message()));
                        result.report = reports.join(QStringLiteral("\n\n"));
                        return result;
                    }
                }

                auto patched = modifier.build({ true });
                if (!patched) {
                    result.error = QStringLiteral("Build rewritten ABC failed: %1: %2")
                        .arg(resourcePath, fromStdString(patched.error().message()));
                    result.report = reports.join(QStringLiteral("\n\n"));
                    return result;
                }

                const auto verify = hyle::hap::verify_abc(*patched);
                if (!verify.valid) {
                    result.error = QStringLiteral("Verify rewritten ABC failed: %1: %2")
                        .arg(resourcePath, fromStdString(verify.message));
                    result.report = reports.join(QStringLiteral("\n\n"));
                    return result;
                }

                ++result.rewrittenAbcCount;
                result.replacementCount += static_cast<int>(offsets.size());
                reports.append(formatStringRewriteReport(
                    modifier.patch_report(),
                    resourcePath,
                    static_cast<qsizetype>(bytes.size()),
                    static_cast<qsizetype>(patched->size()),
                    static_cast<int>(offsets.size())));
                bytes = std::move(*patched);
            }
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
    if (result.abcCount == 0) {
        result.error = QStringLiteral("HAP contains no ABC resource to rewrite: %1").arg(request.inputHapPath);
        return result;
    }
    if (result.replacementCount == 0) {
        result.error = QStringLiteral("ABC string was not found in HAP: %1").arg(request.oldText);
        result.report = reports.join(QStringLiteral("\n\n"));
        return result;
    }
    if (request.stopToken.stop_requested()) {
        result.error = QStringLiteral("ABC string rewrite cancelled.");
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
