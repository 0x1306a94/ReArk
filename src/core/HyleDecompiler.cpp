#include "core/HyleDecompiler.h"

#include "core/PerformanceTrace.h"

#include <hyle.h>

#include <QFileInfo>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QImage>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMimeDatabase>
#include <QMimeType>
#include <QPainter>
#include <QStringList>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <string_view>
#include <thread>
#include <system_error>
#include <utility>

namespace {

struct LinkedStopToken {
    std::stop_source source;
    std::stop_callback<std::function<void()>> sessionCallback;
    std::stop_callback<std::function<void()>> callerCallback;

    LinkedStopToken(std::stop_token sessionStopToken, std::stop_token callerStopToken)
        : sessionCallback(sessionStopToken, [this] {
            source.request_stop();
        })
        , callerCallback(callerStopToken, [this] {
            source.request_stop();
        })
    {
        if (sessionStopToken.stop_requested() || callerStopToken.stop_requested()) {
            source.request_stop();
        }
    }

    [[nodiscard]] std::stop_token token() const noexcept
    {
        return source.get_token();
    }
};

QString fromUtf8(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

QString fromUtf8(std::string_view value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

std::string toLower(std::string value)
{
    std::ranges::transform(value, value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

QString errorMessage(const char* context, const std::error_code& error)
{
    return QStringLiteral("%1: %2").arg(QString::fromLatin1(context), fromUtf8(error.message()));
}

std::string toUtf8Path(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

bool isAbcFile(const QString& filePath)
{
    return QFileInfo(filePath).suffix().compare(QStringLiteral("abc"), Qt::CaseInsensitive) == 0;
}

bool hasAbcSuffix(const std::string& path)
{
    constexpr std::string_view suffix = ".abc";
    const std::string lower = toLower(path);
    return lower.size() >= suffix.size()
        && lower.compare(lower.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool isAppFile(const QString& filePath)
{
    return QFileInfo(filePath).suffix().compare(QStringLiteral("app"), Qt::CaseInsensitive) == 0;
}

bool hasHapSuffix(const std::string& path)
{
    constexpr std::string_view suffix = ".hap";
    const std::string lower = toLower(path);
    return lower.size() >= suffix.size()
        && lower.compare(lower.size() - suffix.size(), suffix.size(), suffix) == 0;
}

QString sanitizedTempFileName(QString value)
{
    value.replace(QLatin1Char('\\'), QLatin1Char('/'));
    value = QFileInfo(value).fileName();
    if (value.isEmpty()) {
        value = QStringLiteral("module.hap");
    }
    for (QChar& ch : value) {
        if (ch == QLatin1Char('/') || ch == QLatin1Char('\\') || ch == QLatin1Char(':')
            || ch == QLatin1Char('*') || ch == QLatin1Char('?') || ch == QLatin1Char('"')
            || ch == QLatin1Char('<') || ch == QLatin1Char('>') || ch == QLatin1Char('|')) {
            ch = QLatin1Char('_');
        }
    }
    if (!value.endsWith(QStringLiteral(".hap"), Qt::CaseInsensitive)) {
        value += QStringLiteral(".hap");
    }
    return value;
}

QString sanitizedEvidenceFileName(QString value)
{
    value.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (value.isEmpty()) {
        value = QStringLiteral("modules.abc");
    }
    for (QChar& ch : value) {
        if (ch == QLatin1Char('/') || ch == QLatin1Char('\\') || ch == QLatin1Char(':')
            || ch == QLatin1Char('*') || ch == QLatin1Char('?') || ch == QLatin1Char('"')
            || ch == QLatin1Char('<') || ch == QLatin1Char('>') || ch == QLatin1Char('|')) {
            ch = QLatin1Char('_');
        }
    }
    if (!value.endsWith(QStringLiteral(".abc"), Qt::CaseInsensitive)) {
        value += QStringLiteral(".abc");
    }
    return value;
}

QString hexOffset(std::uint32_t offset)
{
    return QStringLiteral("0x%1")
        .arg(offset, 8, 16, QLatin1Char('0'))
        .toUpper();
}

QString bytecodeReferenceKindName(hyle::hap::bytecode_reference_kind kind)
{
    using hyle::hap::bytecode_reference_kind;
    switch (kind) {
    case bytecode_reference_kind::string:
        return QStringLiteral("string");
    case bytecode_reference_kind::method:
        return QStringLiteral("method");
    case bytecode_reference_kind::literal:
        return QStringLiteral("literal");
    }
    return QStringLiteral("unknown");
}

std::optional<hyle::hap::bytecode_reference_kind> parseBytecodeReferenceKind(const QString& value)
{
    using hyle::hap::bytecode_reference_kind;
    const QString folded = value.trimmed().toCaseFolded();
    if (folded.isEmpty() || folded == QStringLiteral("any")) {
        return std::nullopt;
    }
    if (folded == QStringLiteral("string")) {
        return bytecode_reference_kind::string;
    }
    if (folded == QStringLiteral("method")) {
        return bytecode_reference_kind::method;
    }
    if (folded == QStringLiteral("literal")) {
        return bytecode_reference_kind::literal;
    }
    return std::nullopt;
}

std::optional<std::uint32_t> parseAbcOffset(QString value)
{
    value = value.trimmed();
    if (value.startsWith(QStringLiteral("literal@"), Qt::CaseInsensitive)) {
        value = value.mid(8);
    }
    bool ok = false;
    const int base = value.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive) ? 16 : 10;
    const qulonglong parsed = value.toULongLong(&ok, base);
    if (!ok || parsed > std::numeric_limits<std::uint32_t>::max()) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(parsed);
}

QString rawHex(const std::vector<std::byte>& bytes, qsizetype maxBytes = 64)
{
    if (bytes.empty()) {
        return {};
    }
    const qsizetype size = std::min<qsizetype>(static_cast<qsizetype>(bytes.size()), maxBytes);
    QByteArray data(reinterpret_cast<const char*>(bytes.data()), size);
    QString text = QString::fromLatin1(data.toHex());
    if (static_cast<qsizetype>(bytes.size()) > maxBytes) {
        text += QStringLiteral("...");
    }
    return text;
}

QString quotedPreview(QString value, int maxChars = 220)
{
    value.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    value.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    value.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
    value.replace(QLatin1Char('\r'), QStringLiteral("\\r"));
    value.replace(QLatin1Char('\t'), QStringLiteral("\\t"));
    if (value.size() > maxChars) {
        value = value.left(maxChars) + QStringLiteral("...");
    }
    return QStringLiteral("\"%1\"").arg(value);
}

QString boundedEvidenceText(QString text, int maxChars)
{
    constexpr int kDefaultMaxChars = 24000;
    constexpr int kHardMaxChars = 120000;
    if (maxChars <= 0) {
        maxChars = kDefaultMaxChars;
    }
    maxChars = std::clamp(maxChars, 1000, kHardMaxChars);
    if (text.size() <= maxChars) {
        return text;
    }
    return text.left(maxChars)
        + QStringLiteral("\n\n[truncated: %1 of %2 characters shown]")
              .arg(maxChars)
              .arg(text.size());
}

QString normalizeSourceContent(QString content);
QString languageKind(const std::string& language);

QString diagnosticsText(const std::vector<hyle::hap::diagnostic>& diagnostics)
{
    QString text;
    for (const auto& diagnostic : diagnostics) {
        if (!text.isEmpty()) {
            text += QLatin1Char('\n');
        }
        text += fromUtf8(diagnostic.source);
        text += QStringLiteral(": ");
        text += fromUtf8(diagnostic.message);
    }
    return text;
}

void applyDecompiledSource(
    HyleDecompiler::SourceResult& result,
    const hyle::hap::decompiled_source_file& source)
{
    result.content = normalizeSourceContent(fromUtf8(source.content));
    result.kind = languageKind(source.language);
    result.contentMode = QStringLiteral("text");
}

QString languageKind(const std::string& language)
{
    if (language.empty()) {
        return QStringLiteral("SRC");
    }
    return QString::fromStdString(toLower(language)).toUpper();
}

QString resourceKindName(hyle::hap::hap_resource_kind kind)
{
    using hyle::hap::hap_resource_kind;
    switch (kind) {
    case hap_resource_kind::directory:
        return QStringLiteral("DIR");
    case hap_resource_kind::abc:
        return QStringLiteral("ABC");
    case hap_resource_kind::json:
        return QStringLiteral("JSON");
    case hap_resource_kind::image:
        return QStringLiteral("IMAGE");
    case hap_resource_kind::media:
        return QStringLiteral("MEDIA");
    case hap_resource_kind::native_library:
        return QStringLiteral("LIB");
    case hap_resource_kind::signature:
        return QStringLiteral("SIGNATURE");
    case hap_resource_kind::resource_index:
        return QStringLiteral("RESOURCE_INDEX");
    case hap_resource_kind::text:
        return QStringLiteral("TXT");
    case hap_resource_kind::binary:
        return QStringLiteral("BIN");
    case hap_resource_kind::unknown:
        return QStringLiteral("UNKNOWN");
    }
    return QStringLiteral("UNKNOWN");
}

bool isTextResource(hyle::hap::hap_resource_kind kind)
{
    using hyle::hap::hap_resource_kind;
    return kind == hap_resource_kind::json
        || kind == hap_resource_kind::resource_index
        || kind == hap_resource_kind::signature
        || kind == hap_resource_kind::text;
}

bool isImageResource(hyle::hap::hap_resource_kind kind)
{
    return kind == hyle::hap::hap_resource_kind::image;
}

bool isImageMime(const QMimeType& mime)
{
    return mime.isValid() && mime.name().startsWith(QStringLiteral("image/"));
}

bool isMediaMime(const QMimeType& mime)
{
    return mime.isValid()
        && (mime.name().startsWith(QStringLiteral("video/"))
            || mime.name().startsWith(QStringLiteral("audio/")));
}

bool isImageResource(hyle::hap::hap_resource_kind kind, const std::string& path)
{
    if (isImageResource(kind)) {
        return true;
    }

    QMimeDatabase mimeDatabase;
    return isImageMime(mimeDatabase.mimeTypeForFile(fromUtf8(path), QMimeDatabase::MatchExtension));
}

QString bytesToUtf8Text(const std::vector<std::byte>& bytes)
{
    if (bytes.empty()) {
        return {};
    }
    return QString::fromUtf8(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<qsizetype>(bytes.size()));
}

QByteArray bytesToByteArray(const std::vector<std::byte>& bytes)
{
    if (bytes.empty()) {
        return {};
    }
    return QByteArray(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<qsizetype>(bytes.size()));
}

QImage imageFromResourceContent(const hyle::hap::hap_resource_content& content)
{
    QImage image;
    const QByteArray bytes = bytesToByteArray(content.bytes);
    image.loadFromData(bytes);
    return image;
}

void drawImagePreservingAspect(QPainter& painter, const QImage& image, const QRect& bounds)
{
    if (image.isNull() || bounds.isEmpty()) {
        return;
    }

    const QSize scaled = image.size().scaled(bounds.size(), Qt::KeepAspectRatio);
    const QPoint topLeft(
        bounds.x() + (bounds.width() - scaled.width()) / 2,
        bounds.y() + (bounds.height() - scaled.height()) / 2);
    painter.drawImage(QRect(topLeft, scaled), image);
}

QByteArray imageToPngBytes(const QImage& image)
{
    if (image.isNull()) {
        return {};
    }

    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly)) {
        return {};
    }
    if (!image.save(&buffer, "PNG")) {
        return {};
    }
    return bytes;
}

QByteArray composeLayeredAppIcon(const hyle::hap::hap_app_icon_content& icon)
{
    std::vector<QImage> backgroundLayers;
    std::vector<QImage> foregroundLayers;
    backgroundLayers.reserve(icon.layers.size());
    foregroundLayers.reserve(icon.layers.size());

    QSize canvasSize;
    for (std::size_t i = 0; i < icon.layers.size(); ++i) {
        const auto& layer = icon.layers.at(i);
        QImage image = imageFromResourceContent(layer);
        if (image.isNull()) {
            continue;
        }
        canvasSize = canvasSize.expandedTo(image.size());
        const bool foreground = i < icon.icon.layers.size()
            && icon.icon.layers.at(i).role == hyle::hap::hap_app_icon_layer_role::foreground;
        if (foreground) {
            foregroundLayers.push_back(std::move(image));
        } else {
            backgroundLayers.push_back(std::move(image));
        }
    }

    if ((backgroundLayers.empty() && foregroundLayers.empty()) || canvasSize.isEmpty()) {
        return {};
    }

    QImage composed(canvasSize, QImage::Format_ARGB32_Premultiplied);
    composed.fill(Qt::transparent);

    QPainter painter(&composed);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const QRect bounds(QPoint(0, 0), canvasSize);
    for (const auto& image : backgroundLayers) {
        drawImagePreservingAspect(painter, image, bounds);
    }
    for (const auto& image : foregroundLayers) {
        drawImagePreservingAspect(painter, image, bounds);
    }
    painter.end();

    return imageToPngBytes(composed);
}

QByteArray appIconBytes(const hyle::hap::hap_app_icon_content& icon)
{
    if (icon.icon.layered) {
        return composeLayeredAppIcon(icon);
    }
    return bytesToByteArray(icon.resource.bytes);
}

bool isJsonContent(const QByteArray& bytes)
{
    if (bytes.isEmpty()) {
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    return parseError.error == QJsonParseError::NoError && !document.isNull();
}

QMimeType detectResourceMimeType(const hyle::hap::hap_resource_content& content)
{
    QMimeDatabase mimeDatabase;
    const auto bytes = bytesToByteArray(content.bytes);
    auto mime = mimeDatabase.mimeTypeForFileNameAndData(fromUtf8(content.path), bytes);
    if (!mime.isValid() || mime.name() == QStringLiteral("application/octet-stream")) {
        mime = mimeDatabase.mimeTypeForData(bytes);
    }
    return mime;
}

bool isImageResource(const hyle::hap::hap_resource_content& content)
{
    return isImageResource(content.kind, content.path) || isImageMime(detectResourceMimeType(content));
}

bool isMediaResource(hyle::hap::hap_resource_kind kind, const std::string& path)
{
    if (kind == hyle::hap::hap_resource_kind::media) {
        return true;
    }

    QMimeDatabase mimeDatabase;
    return isMediaMime(mimeDatabase.mimeTypeForFile(fromUtf8(path), QMimeDatabase::MatchExtension));
}

bool isMediaResource(const hyle::hap::hap_resource_content& content)
{
    return isMediaResource(content.kind, content.path) || isMediaMime(detectResourceMimeType(content));
}

QString boolText(bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

QString signatureStatusText(hyle::hap::hap_signature_status status)
{
    using hyle::hap::hap_signature_status;
    switch (status) {
    case hap_signature_status::not_found:
        return QStringLiteral("not found");
    case hap_signature_status::profile_found:
        return QStringLiteral("profile found");
    case hap_signature_status::certificate_found:
        return QStringLiteral("certificate found");
    case hap_signature_status::certificate_parse_failed:
        return QStringLiteral("certificate parse failed");
    case hap_signature_status::malformed_profile:
        return QStringLiteral("malformed profile");
    }
    return QStringLiteral("unknown");
}

void appendField(QStringList& lines, const QString& name, const QString& value)
{
    if (!value.isEmpty()) {
        lines.append(QStringLiteral("%1: %2").arg(name, value));
    }
}

void appendField(QStringList& lines, const QString& name, bool value)
{
    lines.append(QStringLiteral("%1: %2").arg(name, boolText(value)));
}

void appendField(QStringList& lines, const QString& name, std::size_t value)
{
    if (value > 0) {
        lines.append(QStringLiteral("%1: %2").arg(name).arg(static_cast<qulonglong>(value)));
    }
}

QString formatCertificate(const hyle::hap::hap_certificate_info& certificate, int index)
{
    QStringList lines;
    lines.append(QStringLiteral("Certificate %1").arg(index + 1));
    lines.append(QStringLiteral("----------------"));
    appendField(lines, QStringLiteral("Status"), fromUtf8(certificate.status));
    appendField(lines, QStringLiteral("Status message"), fromUtf8(certificate.status_message));
    appendField(lines, QStringLiteral("Valid"), certificate.is_valid);
    appendField(lines, QStringLiteral("Signer"), fromUtf8(certificate.signer));
    appendField(lines, QStringLiteral("Issuer"), fromUtf8(certificate.issuer));
    appendField(lines, QStringLiteral("Valid from"), fromUtf8(certificate.valid_from));
    appendField(lines, QStringLiteral("Valid to"), fromUtf8(certificate.valid_to));
    appendField(lines, QStringLiteral("Algorithm"), fromUtf8(certificate.algorithm));
    appendField(lines, QStringLiteral("Key size"), certificate.key_size);
    appendField(lines, QStringLiteral("Thumbprint"), fromUtf8(certificate.thumbprint));

    if (!certificate.fingerprints.empty()) {
        lines.append(QStringLiteral("Fingerprints:"));
        for (const auto& fingerprint : certificate.fingerprints) {
            lines.append(QStringLiteral("  - %1").arg(fromUtf8(fingerprint)));
        }
    }

    return lines.join(QLatin1Char('\n'));
}

QString formatSignatureInfo(const hyle::hap::hap_signature_info& signature)
{
    QStringList lines;
    lines.append(QStringLiteral("HAP Signature"));
    lines.append(QStringLiteral("============="));
    lines.append(QString());

    appendField(lines, QStringLiteral("Status"), signatureStatusText(signature.status));
    appendField(lines, QStringLiteral("Signed"), signature.is_signed);
    appendField(lines, QStringLiteral("Profile found"), signature.profile_found);
    appendField(lines, QStringLiteral("Certificate found"), signature.certificate_found);
    appendField(lines, QStringLiteral("Certificate parsed"), signature.certificate_parsed);
    appendField(lines, QStringLiteral("AppGallery issued"), signature.app_gallery_issued);
    appendField(lines, QStringLiteral("Version name"), fromUtf8(signature.version_name));
    appendField(lines, QStringLiteral("Issuer"), fromUtf8(signature.issuer));
    appendField(lines, QStringLiteral("Certificate status"), fromUtf8(signature.certificate_status));
    appendField(lines, QStringLiteral("Certificate status message"), fromUtf8(signature.certificate_status_message));
    appendField(lines, QStringLiteral("Certificate valid"), signature.certificate_valid);
    appendField(lines, QStringLiteral("Signer"), fromUtf8(signature.signer));
    appendField(lines, QStringLiteral("Certificate issuer"), fromUtf8(signature.certificate_issuer));
    appendField(lines, QStringLiteral("Valid from"), fromUtf8(signature.valid_from));
    appendField(lines, QStringLiteral("Valid to"), fromUtf8(signature.valid_to));
    appendField(lines, QStringLiteral("Algorithm"), fromUtf8(signature.algorithm));
    appendField(lines, QStringLiteral("Key size"), signature.key_size);
    appendField(lines, QStringLiteral("Thumbprint"), fromUtf8(signature.thumbprint));

    if (!signature.fingerprints.empty()) {
        lines.append(QString());
        lines.append(QStringLiteral("Fingerprints"));
        lines.append(QStringLiteral("------------"));
        for (const auto& fingerprint : signature.fingerprints) {
            lines.append(QStringLiteral("- %1").arg(fromUtf8(fingerprint)));
        }
    }

    if (!signature.certificates.empty()) {
        lines.append(QString());
        lines.append(QStringLiteral("Certificates"));
        lines.append(QStringLiteral("------------"));
        for (int i = 0; i < static_cast<int>(signature.certificates.size()); ++i) {
            if (i > 0) {
                lines.append(QString());
            }
            lines.append(formatCertificate(signature.certificates.at(static_cast<std::size_t>(i)), i));
        }
    }

    if (!signature.diagnostics.empty()) {
        lines.append(QString());
        lines.append(QStringLiteral("Diagnostics"));
        lines.append(QStringLiteral("-----------"));
        for (const auto& diagnostic : signature.diagnostics) {
            lines.append(QStringLiteral("- %1").arg(fromUtf8(diagnostic)));
        }
    }

    if (!signature.development_certificate_pem.empty()) {
        lines.append(QString());
        lines.append(QStringLiteral("Development certificate PEM"));
        lines.append(QStringLiteral("---------------------------"));
        lines.append(fromUtf8(signature.development_certificate_pem));
    }

    return normalizeSourceContent(lines.join(QLatin1Char('\n')));
}

DecompiledSourceFile inspectSignatureFile(const std::string& hylePath)
{
    auto signature = hyle::hap::inspect_hap_signature(hylePath);
    if (!signature) {
        return {
            QStringLiteral("Signature.txt"),
            QStringLiteral("TXT"),
            errorMessage("Inspect signature failed", signature.error()),
            {},
            QStringLiteral("signature"),
            QStringLiteral("text"),
            0,
            false
        };
    }

    return {
        QStringLiteral("Signature.txt"),
        QStringLiteral("TXT"),
        formatSignatureInfo(*signature),
        {},
        QStringLiteral("signature"),
        QStringLiteral("text"),
        0,
        false
    };
}

DecompiledSourceFile lazySignatureFile(const QString& name, std::size_t packageId)
{
    DecompiledSourceFile file {
        name,
        QStringLiteral("TXT"),
        {},
        {},
        QStringLiteral("signature"),
        QStringLiteral("text"),
        0,
        true
    };
    file.packageId = packageId;
    return file;
}

DecompiledSourceFile lazySummaryFile(const QString& name, std::size_t packageId)
{
    DecompiledSourceFile file {
        name,
        QStringLiteral("TXT"),
        {},
        {},
        QStringLiteral("summary"),
        QStringLiteral("text"),
        0,
        true
    };
    file.packageId = packageId;
    return file;
}

QString normalizeSourceContent(QString content)
{
    content.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    content.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    QStringList lines = content.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    while (!lines.isEmpty() && lines.last().trimmed().isEmpty()) {
        lines.removeLast();
    }
    return lines.join(QLatin1Char('\n'));
}

std::vector<DecompiledSourceFile> fromSourceFiles(
    const std::vector<hyle::hap::decompiled_source_file>& sourceFiles)
{
    std::vector<DecompiledSourceFile> files;
    files.reserve(sourceFiles.size());

    for (const auto& file : sourceFiles) {
        files.push_back({
            fromUtf8(file.path),
            languageKind(file.language),
            normalizeSourceContent(fromUtf8(file.content)),
            {},
            QStringLiteral("source"),
            QStringLiteral("text"),
            0,
            false
        });
    }

    return files;
}

std::optional<std::size_t> moduleIdForSourceFile(
    const hyle::hap::decompiled_source_file_info& sourceFile,
    const std::vector<hyle::hap::decompiled_module>& modules)
{
    for (const std::string& entry : sourceFile.module_entries) {
        const auto found = std::ranges::find_if(modules, [&entry](const hyle::hap::decompiled_module& module) {
            return module.entry_path == entry;
        });
        if (found != modules.end()) {
            return found->id;
        }
    }

    if (sourceFile.module_entries.empty() && modules.size() == 1U) {
        return modules.front().id;
    }

    return std::nullopt;
}

HyleDecompiler::PackageSession* packageSession(
    const std::shared_ptr<HyleDecompiler::SessionContext>& context,
    std::size_t packageId)
{
    if (!context || packageId >= context->packages.size()) {
        return nullptr;
    }
    return &context->packages.at(packageId);
}

QString prefixedPath(const QString& prefix, const std::string& path)
{
    const QString itemPath = fromUtf8(path);
    if (prefix.isEmpty()) {
        return itemPath;
    }
    if (itemPath.isEmpty()) {
        return prefix;
    }
    return prefix + QLatin1Char('/') + itemPath;
}

std::size_t appendOpenedPackageFiles(
    HyleDecompiler::OpenResult& result,
    HyleDecompiler::PackageSession& package,
    std::size_t packageId,
    const QString& pathPrefix)
{
    const auto sourceFileCount = package.session.source_files().size();
    result.files.reserve(result.files.size() + sourceFileCount + package.session.resources().size());
    const auto& modules = package.session.modules();
    for (const auto& file : package.session.source_files()) {
        const auto moduleId = moduleIdForSourceFile(file, modules);
        result.files.push_back({
            prefixedPath(pathPrefix, file.path),
            languageKind(file.language),
            {},
            {},
            QStringLiteral("source"),
            QStringLiteral("text"),
            file.id,
            true,
            false,
            moduleId,
            true,
            packageId
        });
    }
    for (const auto& resource : package.session.resources()) {
        result.files.push_back({
            prefixedPath(pathPrefix, resource.path),
            resourceKindName(resource.kind),
            {},
            {},
            QStringLiteral("resource"),
            isImageResource(resource.kind, resource.path)
                ? QStringLiteral("image")
                : isMediaResource(resource.kind, resource.path) ? QStringLiteral("media")
                : isTextResource(resource.kind) ? QStringLiteral("text") : QStringLiteral("hex"),
            resource.id,
            !resource.is_directory,
            resource.is_directory,
            std::nullopt,
            false,
            packageId
        });
    }
    return sourceFileCount;
}

QString openPackageIntoResult(
    HyleDecompiler::OpenResult& result,
    const std::shared_ptr<HyleDecompiler::SessionContext>& context,
    const QString& packagePath,
    const QString& displayName,
    const QString& pathPrefix,
    std::size_t& sourceFileCount)
{
    auto session = hyle::async::sync_wait(
        hyle::hap::open_decompiled_package_async(
            context->scheduler(),
            toUtf8Path(packagePath),
            {},
            context->stopToken()));
    if (!session) {
        return errorMessage("Open package failed", session.error());
    }

    HyleDecompiler::PackageSession package;
    package.path = packagePath;
    package.displayName = displayName;
    package.session = std::move(*session);

    const auto packageId = context->packages.size();
    context->packages.push_back(std::move(package));
    auto& storedPackage = context->packages.back();

    if (result.appIconBytes.isEmpty()) {
        auto launcherIcon = hyle::async::sync_wait(
            storedPackage.session.read_launcher_app_icon_async(
                context->scheduler(),
                context->stopToken()));
        if (launcherIcon) {
            result.appIconBytes = appIconBytes(*launcherIcon);
            result.appIconPath = fromUtf8(launcherIcon->icon.path);
            result.appIconLayered = launcherIcon->icon.layered;
            if (result.appIconPath.isEmpty()) {
                result.appIconPath = fromUtf8(launcherIcon->resource.path);
            }
        }
    }

    sourceFileCount += appendOpenedPackageFiles(result, storedPackage, packageId, pathPrefix);
    return {};
}

QString writeExtractedHap(
    const QString& targetPath,
    const std::vector<std::byte>& bytes)
{
    QFile file(targetPath);
    if (!file.open(QIODevice::WriteOnly)) {
        return QObject::tr("Extract HAP failed: cannot write %1").arg(targetPath);
    }
    const auto* data = reinterpret_cast<const char*>(bytes.data());
    if (file.write(data, static_cast<qint64>(bytes.size())) != static_cast<qint64>(bytes.size())) {
        return QObject::tr("Extract HAP failed: cannot write all bytes to %1").arg(targetPath);
    }
    return {};
}

struct AbcEvidenceTarget {
    QString displayPath;
    QString filePath;
    QString error;
};

bool matchesAbcQuery(const QString& displayPath, const QString& rawPath, const QString& query)
{
    if (query.trimmed().isEmpty()) {
        return true;
    }
    const QString foldedQuery = query.trimmed().toCaseFolded();
    const QString foldedDisplay = displayPath.toCaseFolded();
    const QString foldedRaw = rawPath.toCaseFolded();
    const QString foldedName = QFileInfo(rawPath).fileName().toCaseFolded();
    return foldedDisplay == foldedQuery
        || foldedRaw == foldedQuery
        || foldedName == foldedQuery
        || foldedDisplay.contains(foldedQuery)
        || foldedRaw.contains(foldedQuery);
}

int abcQueryScore(const QString& displayPath, const QString& rawPath, const QString& query)
{
    if (query.trimmed().isEmpty()) {
        return 1;
    }
    const QString foldedQuery = query.trimmed().toCaseFolded();
    const QString foldedDisplay = displayPath.toCaseFolded();
    const QString foldedRaw = rawPath.toCaseFolded();
    const QString foldedName = QFileInfo(rawPath).fileName().toCaseFolded();
    if (foldedDisplay == foldedQuery || foldedRaw == foldedQuery) {
        return 100;
    }
    if (foldedName == foldedQuery) {
        return 90;
    }
    if (foldedDisplay.contains(foldedQuery)) {
        return 60;
    }
    if (foldedRaw.contains(foldedQuery)) {
        return 50;
    }
    return -1;
}

QString ensureAbcEvidenceTempDir(const std::shared_ptr<HyleDecompiler::SessionContext>& context)
{
    if (!context) {
        return QObject::tr("Decompiler session is not available.");
    }
    if (!context->abcEvidenceTempDir) {
        context->abcEvidenceTempDir = std::make_unique<QTemporaryDir>();
    }
    if (!context->abcEvidenceTempDir->isValid()) {
        return QObject::tr("Create temporary ABC evidence directory failed.");
    }
    return {};
}

AbcEvidenceTarget resolveAbcEvidenceTarget(
    const std::shared_ptr<HyleDecompiler::SessionContext>& context,
    const QString& fallbackPackagePath,
    const QString& pathOrQuery,
    std::stop_token stopToken)
{
    if (stopToken.stop_requested() || (context && context->stopToken().stop_requested())) {
        return { {}, {}, QObject::tr("Operation cancelled.") };
    }

    const QString query = pathOrQuery.trimmed();
    const QFileInfo queryFile(query);
    if (!query.isEmpty() && queryFile.exists() && queryFile.isFile() && isAbcFile(queryFile.filePath())) {
        return { queryFile.filePath(), queryFile.filePath(), {} };
    }

    const QFileInfo fallbackFile(fallbackPackagePath);
    if (fallbackFile.exists() && fallbackFile.isFile() && isAbcFile(fallbackFile.filePath())
        && matchesAbcQuery(fallbackFile.filePath(), fallbackFile.fileName(), query)) {
        return { fallbackFile.filePath(), fallbackFile.filePath(), {} };
    }

    struct Candidate {
        HyleDecompiler::PackageSession* package = nullptr;
        const hyle::hap::hap_resource_entry* resource = nullptr;
        QString displayPath;
        std::size_t packageId = 0;
        int score = -1;
    };

    Candidate best;
    if (context) {
        for (std::size_t packageId = 0; packageId < context->packages.size(); ++packageId) {
            auto& package = context->packages.at(packageId);
            for (const auto& resource : package.session.resources()) {
                if (resource.is_directory
                    || !(resource.is_abc || resource.kind == hyle::hap::hap_resource_kind::abc || hasAbcSuffix(resource.path))) {
                    continue;
                }

                const QString rawPath = fromUtf8(resource.path);
                const QString displayPath = package.displayName.isEmpty()
                    ? rawPath
                    : package.displayName + QLatin1Char('/') + rawPath;
                const int score = abcQueryScore(displayPath, rawPath, query);
                if (score > best.score) {
                    best = { &package, &resource, displayPath, packageId, score };
                }
            }
        }
    }

    if (!best.package || !best.resource || best.score < 0) {
        return {
            {},
            {},
            query.isEmpty()
                ? QObject::tr("No ABC file is available in the current target.")
                : QObject::tr("No ABC file matched: %1").arg(pathOrQuery)
        };
    }

    const QString tempError = ensureAbcEvidenceTempDir(context);
    if (!tempError.isEmpty()) {
        return { {}, {}, tempError };
    }

    const QString cacheKey = QStringLiteral("%1:%2:%3")
        .arg(static_cast<qulonglong>(best.packageId))
        .arg(static_cast<qulonglong>(best.resource->id))
        .arg(fromUtf8(best.resource->path));
    {
        std::lock_guard lock(context->abcEvidenceMutex);
        const auto cached = context->abcEvidenceFiles.find(cacheKey);
        if (cached != context->abcEvidenceFiles.end()) {
            if (QFileInfo::exists(cached->second)) {
                return { best.displayPath, cached->second, {} };
            }
            context->abcEvidenceFiles.erase(cached);
        }
    }

    LinkedStopToken linkedStop(context->stopToken(), stopToken);
    auto content = hyle::async::sync_wait(
        best.package->session.read_resource_async(
            context->scheduler(),
            best.resource->id,
            linkedStop.token()));
    if (!content) {
        return { {}, {}, errorMessage("Read ABC resource failed", content.error()) };
    }

    const QString fileName = QStringLiteral("pkg%1_res%2_%3")
        .arg(static_cast<qulonglong>(best.packageId))
        .arg(static_cast<qulonglong>(best.resource->id))
        .arg(sanitizedEvidenceFileName(fromUtf8(best.resource->path)));
    const QString filePath = QDir(context->abcEvidenceTempDir->path()).filePath(fileName);
    {
        std::lock_guard lock(context->abcEvidenceMutex);
        const auto cached = context->abcEvidenceFiles.find(cacheKey);
        if (cached != context->abcEvidenceFiles.end() && QFileInfo::exists(cached->second)) {
            return { best.displayPath, cached->second, {} };
        }
        const QString writeError = writeExtractedHap(filePath, content->bytes);
        if (!writeError.isEmpty()) {
            return { {}, {}, writeError };
        }
        context->abcEvidenceFiles[cacheKey] = filePath;
    }

    return { best.displayPath, filePath, {} };
}

QString formatAbcTargetHeader(const AbcEvidenceTarget& target)
{
    QString text;
    text += QStringLiteral("# status: ok\n");
    text += QStringLiteral("# abc: %1\n").arg(target.displayPath);
    if (target.filePath != target.displayPath) {
        text += QStringLiteral("# evidence_file: %1\n").arg(target.filePath);
    }
    return text;
}

hyle::result<hyle::hap::abc_parser> openParsedAbcParser(const AbcEvidenceTarget& target)
{
    auto parser = hyle::hap::open_abc(std::filesystem::path(toUtf8Path(target.filePath)));
    if (!parser) {
        return hyle::error(parser.error());
    }

    auto parsed = parser->parse();
    if (!parsed) {
        return hyle::error(parsed.error());
    }

    return std::move(*parser);
}

} // namespace

namespace HyleDecompiler {

SessionContext::SessionContext()
    : executor(std::max<std::size_t>(
          2U,
          static_cast<std::size_t>(std::thread::hardware_concurrency())))
{
}

hyle::async::scheduler SessionContext::scheduler() const noexcept
{
    return executor.get_scheduler();
}

std::stop_token SessionContext::stopToken() const noexcept
{
    return stopSource.get_token();
}

void SessionContext::requestStop() noexcept
{
    stopSource.request_stop();
}

OpenResult openFile(
    const QString& filePath,
    const std::shared_ptr<SessionContext>& context)
{
    PerformanceTrace trace(QStringLiteral("HyleDecompiler::openFile"));

    OpenResult result;
    result.context = context;
    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        result.error = QObject::tr("File does not exist: %1").arg(filePath);
        return result;
    }
    if (!context) {
        result.error = QObject::tr("Decompiler worker is not available.");
        return result;
    }
    if (context->stopToken().stop_requested()) {
        result.error = QObject::tr("Open package cancelled.");
        return result;
    }

    const auto hylePath = toUtf8Path(filePath);

    if (isAbcFile(filePath)) {
        auto files = hyle::hap::decompile_abc_sources(hylePath);
        if (!files) {
            result.error = errorMessage("Decompile failed", files.error());
            return result;
        }
        result.files = fromSourceFiles(*files);
        result.status = QObject::tr("Decompiled %1 file(s)").arg(static_cast<int>(result.files.size()));
        return result;
    }

    std::size_t sourceFileCount = 0;
    std::size_t packageCount = 0;
    QStringList diagnostics;

    if (isAppFile(filePath)) {
        auto appSession = hyle::async::sync_wait(
            hyle::hap::open_decompiled_package_async(
                context->scheduler(),
                hylePath,
                {},
                context->stopToken()));
        if (!appSession) {
            result.error = errorMessage("Open APP failed", appSession.error());
            return result;
        }

        context->appTempDir = std::make_unique<QTemporaryDir>();
        if (!context->appTempDir->isValid()) {
            result.error = QObject::tr("Create temporary APP extraction directory failed.");
            return result;
        }

        int extractedIndex = 0;
        for (const auto& resource : appSession->resources()) {
            if (context->stopToken().stop_requested()) {
                result.error = QObject::tr("Open package cancelled.");
                return result;
            }
            if (resource.is_directory || !hasHapSuffix(resource.path)) {
                continue;
            }

            auto content = hyle::async::sync_wait(
                appSession->read_resource_async(
                    context->scheduler(),
                    resource.id,
                    context->stopToken()));
            if (!content) {
                diagnostics.append(errorMessage("Extract HAP failed", content.error()));
                continue;
            }

            const QString displayName = fromUtf8(resource.path);
            const QString fileName = QStringLiteral("%1-%2")
                .arg(extractedIndex++, 3, 10, QLatin1Char('0'))
                .arg(sanitizedTempFileName(displayName));
            const QString extractedPath = QDir(context->appTempDir->path()).filePath(fileName);
            const QString writeError = writeExtractedHap(extractedPath, content->bytes);
            if (!writeError.isEmpty()) {
                diagnostics.append(writeError);
                continue;
            }

            const QString prefix = displayName;
            const QString openError = openPackageIntoResult(
                result,
                context,
                extractedPath,
                displayName,
                prefix,
                sourceFileCount);
            if (!openError.isEmpty()) {
                diagnostics.append(QObject::tr("%1: %2").arg(displayName, openError));
                continue;
            }
            ++packageCount;
        }

        if (packageCount == 0) {
            result.error = diagnostics.isEmpty()
                ? QObject::tr("APP package contains no .hap modules: %1").arg(filePath)
                : diagnostics.join(QLatin1Char('\n'));
            return result;
        }
    } else {
        const QString openError = openPackageIntoResult(
            result,
            context,
            filePath,
            QFileInfo(filePath).fileName(),
            {},
            sourceFileCount);
        if (!openError.isEmpty()) {
            result.error = openError;
            return result;
        }
        packageCount = 1;
    }

    const bool multiPackageMetadata = packageCount > 1;
    for (std::size_t packageId = 0; packageId < context->packages.size(); ++packageId) {
        QString displayName = context->packages.at(packageId).displayName;
        if (displayName.isEmpty()) {
            displayName = QObject::tr("Module %1").arg(static_cast<int>(packageId + 1));
        }

        const QString signatureName = multiPackageMetadata
            ? displayName + QStringLiteral("/Package signature")
            : QStringLiteral("Package signature");
        const QString summaryName = multiPackageMetadata
            ? displayName + QStringLiteral("/Summary")
            : QStringLiteral("Summary");
        result.files.push_back(lazySignatureFile(signatureName, packageId));
        result.files.push_back(lazySummaryFile(summaryName, packageId));
    }

    result.status = isAppFile(filePath)
        ? QObject::tr("Indexed %1 source file(s) from %2 HAP module(s)")
              .arg(static_cast<int>(sourceFileCount))
              .arg(static_cast<int>(packageCount))
        : QObject::tr("Indexed %1 source file(s)").arg(static_cast<int>(sourceFileCount));
    if (!diagnostics.isEmpty()) {
        result.status += QObject::tr(" (%1 warning(s))").arg(diagnostics.size());
    }
    return result;
}

SourceResult readResourceContent(
    const std::shared_ptr<SessionContext>& context,
    int nodeIndex,
    std::size_t hyleId,
    const QString& name,
    std::stop_token stopToken,
    std::size_t packageId)
{
    PerformanceTrace trace(QStringLiteral("HyleDecompiler::readResourceContent"));

    SourceResult result;
    result.nodeIndex = nodeIndex;
    result.name = name;

    auto* package = packageSession(context, packageId);
    if (!package || !package->session.valid()) {
        result.error = QObject::tr("Decompiler session is not available.");
        return result;
    }

    LinkedStopToken linkedStop(context->stopToken(), stopToken);
    auto content = hyle::async::sync_wait(
        package->session.read_resource_async(
            context->scheduler(),
            hyleId,
            linkedStop.token()));
    if (!content) {
        result.error = errorMessage("Read resource failed", content.error());
        return result;
    }

    const QByteArray bytes = bytesToByteArray(content->bytes);
    const bool textResource = isTextResource(content->kind);
    const bool jsonContent = !textResource && isJsonContent(bytes);

    result.kind = jsonContent ? QStringLiteral("JSON") : resourceKindName(content->kind);
    if (isImageResource(*content)) {
        result.contentMode = QStringLiteral("image");
        result.binaryContent = bytes;
    } else if (isMediaResource(*content)) {
        result.contentMode = QStringLiteral("media");
        result.binaryContent = bytes;
    } else if (textResource || jsonContent) {
        result.contentMode = QStringLiteral("text");
        result.binaryContent = bytes;
        result.content = normalizeSourceContent(QString::fromUtf8(bytes.constData(), bytes.size()));
    } else {
        result.contentMode = QStringLiteral("hex");
        result.binaryContent = bytes;
    }
    return result;
}

SourceResult readSignatureContent(
    const QString& filePath,
    int nodeIndex,
    const QString& name)
{
    PerformanceTrace trace(QStringLiteral("HyleDecompiler::readSignatureContent"));

    SourceResult result;
    result.nodeIndex = nodeIndex;
    result.name = name;
    result.kind = QStringLiteral("TXT");
    result.contentMode = QStringLiteral("text");

    const auto signature = inspectSignatureFile(toUtf8Path(filePath));
    result.content = signature.content;
    return result;
}

SourceResult readSignatureContent(
    const std::shared_ptr<SessionContext>& context,
    int nodeIndex,
    const QString& name,
    std::size_t packageId)
{
    auto* package = packageSession(context, packageId);
    if (!package) {
        SourceResult result;
        result.nodeIndex = nodeIndex;
        result.name = name;
        result.kind = QStringLiteral("TXT");
        result.contentMode = QStringLiteral("text");
        result.error = QObject::tr("Decompiler session is not available.");
        return result;
    }
    return readSignatureContent(package->path, nodeIndex, name);
}

SourceResult readSummaryContent(
    const std::shared_ptr<SessionContext>& context,
    int nodeIndex,
    const QString& name,
    std::stop_token stopToken,
    std::size_t packageId)
{
    PerformanceTrace trace(QStringLiteral("HyleDecompiler::readSummaryContent"));

    SourceResult result;
    result.nodeIndex = nodeIndex;
    result.name = name;
    result.kind = QStringLiteral("TXT");
    result.contentMode = QStringLiteral("text");

    auto* package = packageSession(context, packageId);
    if (!package || !package->session.valid()) {
        result.error = QObject::tr("Decompiler session is not available.");
        return result;
    }

    if (stopToken.stop_requested() || context->stopToken().stop_requested()) {
        result.error = QObject::tr("Operation cancelled.");
        return result;
    }

    LinkedStopToken linkedStop(context->stopToken(), stopToken);
    auto summary = hyle::async::sync_wait(
        package->session.summary_async(
            context->scheduler(),
            linkedStop.token()));
    if (!summary) {
        result.error = errorMessage("Summarize package failed", summary.error());
        return result;
    }

    result.content = normalizeSourceContent(fromUtf8(hyle::hap::format_decompiled_package_summary(*summary)));
    return result;
}

SourceResult decompileSourceFile(
    const std::shared_ptr<SessionContext>& context,
    int nodeIndex,
    std::size_t hyleId,
    const QString& name,
    std::stop_token stopToken,
    std::size_t packageId)
{
    PerformanceTrace trace(QStringLiteral("HyleDecompiler::decompileSourceFile"));

    SourceResult result;
    result.nodeIndex = nodeIndex;
    result.name = name;

    auto* packageContext = packageSession(context, packageId);
    if (!packageContext || !packageContext->session.valid()) {
        result.error = QObject::tr("Decompiler session is not available.");
        return result;
    }

    LinkedStopToken linkedStop(context->stopToken(), stopToken);
    auto package = hyle::async::sync_wait(
        packageContext->session.decompile_source_file_async(
            context->scheduler(),
            hyleId,
            {},
            linkedStop.token()));
    if (!package) {
        result.error = errorMessage("Decompile failed", package.error());
        return result;
    }

    result.diagnostics = diagnosticsText(package->diagnostics);

    if (!package->files.empty()) {
        applyDecompiledSource(result, package->files.front());
    } else {
        result.content = QObject::tr("// Hyle returned no source content.");
    }

    return result;
}

SourceBatchResult decompileSourceFiles(
    const std::shared_ptr<SessionContext>& context,
    std::vector<SourceRequest> requests)
{
    PerformanceTrace trace(QStringLiteral("HyleDecompiler::decompileSourceFiles"));

    SourceBatchResult batch;
    batch.files.reserve(requests.size());
    for (const auto& request : requests) {
        SourceResult result;
        result.nodeIndex = request.nodeIndex;
        result.name = request.name;
        batch.files.push_back(std::move(result));
    }

    if (requests.empty()) {
        return batch;
    }

    if (!context) {
        for (auto& result : batch.files) {
            result.error = QObject::tr("Decompiler session is not available.");
        }
        return batch;
    }

    std::map<std::size_t, std::vector<int>> groupedRequests;
    for (int i = 0; i < static_cast<int>(requests.size()); ++i) {
        groupedRequests[requests.at(static_cast<std::size_t>(i)).packageId].push_back(i);
    }

    std::vector<bool> assigned(requests.size(), false);
    for (const auto& [packageId, requestIndices] : groupedRequests) {
        auto* packageContext = packageSession(context, packageId);
        if (!packageContext || !packageContext->session.valid()) {
            for (int index : requestIndices) {
                batch.files.at(static_cast<std::size_t>(index)).error =
                    QObject::tr("Decompiler session is not available.");
            }
            continue;
        }

        std::vector<std::size_t> sourceFileIds;
        sourceFileIds.reserve(requestIndices.size());
        QHash<std::size_t, int> idToBatchIndex;
        QHash<QString, int> pathToBatchIndex;
        for (int index : requestIndices) {
            const auto& request = requests.at(static_cast<std::size_t>(index));
            sourceFileIds.push_back(request.hyleId);
            idToBatchIndex.insert(request.hyleId, index);
            pathToBatchIndex.insert(request.path, index);
        }

        auto package = hyle::async::sync_wait(
            packageContext->session.decompile_source_files_async(
                context->scheduler(),
                std::move(sourceFileIds),
                {},
                context->stopToken()));
        if (!package) {
            const QString error = errorMessage("Decompile failed", package.error());
            for (int index : requestIndices) {
                batch.files.at(static_cast<std::size_t>(index)).error = error;
            }
            continue;
        }

        const QString diagnostics = diagnosticsText(package->diagnostics);
        for (int index : requestIndices) {
            batch.files.at(static_cast<std::size_t>(index)).diagnostics = diagnostics;
        }

        for (const auto& source : package->files) {
            const QString path = fromUtf8(source.path);
            int batchIndex = pathToBatchIndex.value(path, -1);
            const auto foundInfo = std::ranges::find_if(
                packageContext->session.source_files(),
                [&source](const hyle::hap::decompiled_source_file_info& info) {
                    return info.path == source.path;
                });
            if (foundInfo != packageContext->session.source_files().end()) {
                batchIndex = idToBatchIndex.value(foundInfo->id, -1);
            }
            if (batchIndex >= 0 && batchIndex < static_cast<int>(batch.files.size())) {
                applyDecompiledSource(batch.files.at(static_cast<std::size_t>(batchIndex)), source);
                assigned.at(static_cast<std::size_t>(batchIndex)) = true;
            }
        }

        std::size_t fallbackPosition = 0;
        for (const auto& source : package->files) {
            while (fallbackPosition < requestIndices.size()
                   && assigned.at(static_cast<std::size_t>(requestIndices.at(fallbackPosition)))) {
                ++fallbackPosition;
            }
            if (fallbackPosition >= requestIndices.size()) {
                break;
            }
            const int batchIndex = requestIndices.at(fallbackPosition);
            applyDecompiledSource(batch.files.at(static_cast<std::size_t>(batchIndex)), source);
            assigned.at(static_cast<std::size_t>(batchIndex)) = true;
            ++fallbackPosition;
        }
    }

    for (int i = 0; i < static_cast<int>(assigned.size()); ++i) {
        if (!assigned.at(static_cast<std::size_t>(i))) {
            batch.files.at(static_cast<std::size_t>(i)).content =
                QObject::tr("// Hyle returned no source content.");
        }
    }

    return batch;
}

bool isSourceFileCached(
    const std::shared_ptr<SessionContext>& context,
    std::size_t hyleId)
{
    return isSourceFileCached(context, hyleId, 0);
}

bool isSourceFileCached(
    const std::shared_ptr<SessionContext>& context,
    std::size_t hyleId,
    std::size_t packageId)
{
    auto* packageContext = packageSession(context, packageId);
    return packageContext && packageContext->session.valid()
        && packageContext->session.is_source_file_cached(hyleId, {});
}

QString readAbcLiteralEvidence(
    const std::shared_ptr<SessionContext>& context,
    const QString& fallbackPackagePath,
    const QString& pathOrQuery,
    const QString& offsetText,
    int maxChars,
    std::stop_token stopToken)
{
    PerformanceTrace trace(QStringLiteral("HyleDecompiler::readAbcLiteralEvidence"));

    const auto offset = parseAbcOffset(offsetText);
    if (!offset) {
        return QStringLiteral(
            "# status: error\n"
            "# code: invalid_offset\n"
            "# offset: %1\n"
            "# message: Expected an offset like 0x5757 or literal@0x00005757.")
            .arg(offsetText);
    }

    const auto target = resolveAbcEvidenceTarget(context, fallbackPackagePath, pathOrQuery, stopToken);
    if (!target.error.isEmpty()) {
        return QStringLiteral("# status: error\n# code: abc_not_found\n# message: %1").arg(target.error);
    }

    auto literal = hyle::hap::read_abc_literal(
        std::filesystem::path(toUtf8Path(target.filePath)),
        *offset);
    if (!literal) {
        return QStringLiteral(
            "# status: error\n"
            "# code: read_abc_literal_failed\n"
            "# abc: %1\n"
            "# offset: %2\n"
            "# message: %3")
            .arg(target.displayPath, hexOffset(*offset), fromUtf8(literal.error().message()));
    }

    QString text = formatAbcTargetHeader(target);
    text += QStringLiteral("# literal_offset: %1\n").arg(hexOffset(literal->offset));
    text += QStringLiteral("# kind: %1\n").arg(fromUtf8(literal->kind));
    text += QStringLiteral("# size: %1\n").arg(literal->size);
    if (!literal->raw.empty()) {
        text += QStringLiteral("# raw_hex: %1\n").arg(rawHex(literal->raw));
    }
    text += QStringLiteral("\n");

    if (literal->items.empty()) {
        text += QStringLiteral("[literal has no decoded items]\n");
    } else {
        int index = 0;
        for (const auto& item : literal->items) {
            text += QStringLiteral("- item[%1]\n").arg(index++);
            text += QStringLiteral("  offset: %1\n").arg(hexOffset(item.offset));
            if (item.referenced_offset) {
                text += QStringLiteral("  referenced_offset: %1\n").arg(hexOffset(*item.referenced_offset));
            }
            text += QStringLiteral("  type: %1\n").arg(fromUtf8(item.type));
            if (!item.value.empty()) {
                text += QStringLiteral("  value: %1\n").arg(quotedPreview(fromUtf8(item.value), 2000));
            }
            if (!item.raw.empty()) {
                text += QStringLiteral("  raw_hex: %1\n").arg(rawHex(item.raw));
            }
        }
    }
    return boundedEvidenceText(text, maxChars);
}

QString searchAbcStringEvidence(
    const std::shared_ptr<SessionContext>& context,
    const QString& fallbackPackagePath,
    const QString& pathOrQuery,
    const QString& pattern,
    int minLen,
    int maxLen,
    int limit,
    int maxChars,
    std::stop_token stopToken)
{
    PerformanceTrace trace(QStringLiteral("HyleDecompiler::searchAbcStringEvidence"));

    const auto target = resolveAbcEvidenceTarget(context, fallbackPackagePath, pathOrQuery, stopToken);
    if (!target.error.isEmpty()) {
        return QStringLiteral("# status: error\n# code: abc_not_found\n# message: %1").arg(target.error);
    }

    hyle::hap::abc_string_index_options options;
    options.min_len = static_cast<std::size_t>(std::max(1, minLen));
    options.max_len = static_cast<std::size_t>(std::max(0, maxLen));
    options.pattern = toUtf8Path(pattern);
    options.limit = static_cast<std::size_t>(std::clamp(limit <= 0 ? 80 : limit, 1, 1000));
    options.printable_only = true;
    options.unique_offsets = true;

    auto parser = openParsedAbcParser(target);
    if (!parser) {
        return QStringLiteral(
            "# status: error\n"
            "# code: open_abc_failed\n"
            "# abc: %1\n"
            "# message: %2")
            .arg(target.displayPath, fromUtf8(parser.error().message()));
    }

    auto entries = pattern.trimmed().isEmpty()
        ? hyle::hap::build_abc_string_index(*parser, options)
        : hyle::hap::search_abc_strings(*parser, options);
    if (!entries) {
        return QStringLiteral(
            "# status: error\n"
            "# code: search_abc_strings_failed\n"
            "# abc: %1\n"
            "# message: %2")
            .arg(target.displayPath, fromUtf8(entries.error().message()));
    }

    QString text = formatAbcTargetHeader(target);
    text += QStringLiteral("# scope: all_strings\n");
    text += QStringLiteral("# match_count: %1\n").arg(static_cast<qulonglong>(entries->size()));
    text += QStringLiteral("# min_len: %1\n").arg(options.min_len);
    text += QStringLiteral("# max_len: %1\n").arg(options.max_len);
    if (!pattern.isEmpty()) {
        text += QStringLiteral("# pattern: %1\n").arg(pattern);
    }
    text += QStringLiteral("\n");

    int index = 0;
    for (const auto& entry : *entries) {
        text += QStringLiteral("- match[%1]\n").arg(index++);
        text += QStringLiteral("  offset: %1\n").arg(hexOffset(entry.offset));
        text += QStringLiteral("  source: %1\n").arg(fromUtf8(hyle::hap::abc_string_source_kind_name(entry.source_kind)));
        text += QStringLiteral("  length: %1\n").arg(static_cast<qulonglong>(entry.length));
        if (!entry.classification.empty()) {
            text += QStringLiteral("  classification: %1\n").arg(fromUtf8(entry.classification));
        }
        text += QStringLiteral("  value: %1\n").arg(quotedPreview(fromUtf8(entry.value), 2000));
    }
    if (entries->empty()) {
        text += QStringLiteral("[no ABC strings matched]\n");
    }
    return boundedEvidenceText(text, maxChars);
}

QString searchAbcLiteralStringEvidence(
    const std::shared_ptr<SessionContext>& context,
    const QString& fallbackPackagePath,
    const QString& pathOrQuery,
    const QString& pattern,
    int minLen,
    int maxLen,
    int limit,
    int maxChars,
    std::stop_token stopToken)
{
    PerformanceTrace trace(QStringLiteral("HyleDecompiler::searchAbcLiteralStringEvidence"));

    const auto target = resolveAbcEvidenceTarget(context, fallbackPackagePath, pathOrQuery, stopToken);
    if (!target.error.isEmpty()) {
        return QStringLiteral("# status: error\n# code: abc_not_found\n# message: %1").arg(target.error);
    }

    hyle::hap::abc_literal_string_search_options options;
    options.min_len = static_cast<std::size_t>(std::max(1, minLen));
    options.max_len = static_cast<std::size_t>(std::max(0, maxLen));
    options.pattern = toUtf8Path(pattern);
    options.limit = static_cast<std::size_t>(std::clamp(limit <= 0 ? 80 : limit, 1, 1000));
    options.printable_only = true;

    auto parser = openParsedAbcParser(target);
    if (!parser) {
        return QStringLiteral(
            "# status: error\n"
            "# code: open_abc_failed\n"
            "# abc: %1\n"
            "# message: %2")
            .arg(target.displayPath, fromUtf8(parser.error().message()));
    }

    auto matches = hyle::hap::search_abc_literal_strings(*parser, options);
    if (!matches) {
        return QStringLiteral(
            "# status: error\n"
            "# code: search_abc_literal_strings_failed\n"
            "# abc: %1\n"
            "# message: %2")
            .arg(target.displayPath, fromUtf8(matches.error().message()));
    }

    QString text = formatAbcTargetHeader(target);
    text += QStringLiteral("# scope: literal_strings\n");
    text += QStringLiteral("# match_count: %1\n").arg(static_cast<qulonglong>(matches->size()));
    text += QStringLiteral("# min_len: %1\n").arg(options.min_len);
    text += QStringLiteral("# max_len: %1\n").arg(options.max_len);
    if (!pattern.isEmpty()) {
        text += QStringLiteral("# pattern: %1\n").arg(pattern);
    }
    text += QStringLiteral("\n");

    int index = 0;
    for (const auto& match : *matches) {
        text += QStringLiteral("- match[%1]\n").arg(index++);
        text += QStringLiteral("  offset: %1\n").arg(hexOffset(match.offset));
        if (match.container_offset) {
            text += QStringLiteral("  container_offset: %1\n").arg(hexOffset(*match.container_offset));
        }
        if (match.item_offset) {
            text += QStringLiteral("  item_offset: %1\n").arg(hexOffset(*match.item_offset));
        }
        text += QStringLiteral("  type: %1\n").arg(fromUtf8(match.type));
        text += QStringLiteral("  length: %1\n").arg(static_cast<qulonglong>(match.length));
        if (!match.classification.empty()) {
            text += QStringLiteral("  classification: %1\n").arg(fromUtf8(match.classification));
        }
        if (!match.context.empty()) {
            text += QStringLiteral("  context: %1\n").arg(fromUtf8(match.context));
        }
        text += QStringLiteral("  value: %1\n").arg(quotedPreview(fromUtf8(match.value), 2000));
    }
    if (matches->empty()) {
        text += QStringLiteral("[no ABC literal strings matched]\n");
    }
    return boundedEvidenceText(text, maxChars);
}

AbcStringSearchResult searchAbcStrings(
    const std::shared_ptr<SessionContext>& context,
    const QString& fallbackPackagePath,
    const QString& pathOrQuery,
    const QString& pattern,
    int minLen,
    int maxLen,
    int limit,
    std::stop_token stopToken)
{
    PerformanceTrace trace(QStringLiteral("HyleDecompiler::searchAbcStrings"));

    AbcStringSearchResult result;
    const auto target = resolveAbcEvidenceTarget(context, fallbackPackagePath, pathOrQuery, stopToken);
    if (!target.error.isEmpty()) {
        result.error = target.error;
        return result;
    }
    result.abcPath = target.displayPath;

    hyle::hap::abc_string_index_options options;
    options.min_len = static_cast<std::size_t>(std::max(1, minLen));
    options.max_len = static_cast<std::size_t>(std::max(0, maxLen));
    options.pattern = toUtf8Path(pattern);
    options.limit = static_cast<std::size_t>(std::clamp(limit <= 0 ? 80 : limit, 1, 1000));
    options.printable_only = true;
    options.unique_offsets = true;

    auto parser = openParsedAbcParser(target);
    if (!parser) {
        result.error = errorMessage("Open ABC failed", parser.error());
        return result;
    }

    auto entries = pattern.trimmed().isEmpty()
        ? hyle::hap::build_abc_string_index(*parser, options)
        : hyle::hap::search_abc_strings(*parser, options);
    if (!entries) {
        result.error = errorMessage("Search ABC strings failed", entries.error());
        return result;
    }

    result.rows.reserve(entries->size());
    for (const auto& entry : *entries) {
        AbcStringRow row;
        row.offset = hexOffset(entry.offset);
        row.sourceKind = fromUtf8(hyle::hap::abc_string_source_kind_name(entry.source_kind));
        row.type = row.sourceKind;
        row.value = fromUtf8(entry.value);
        row.length = static_cast<int>(std::min<std::size_t>(
            entry.length,
            static_cast<std::size_t>(std::numeric_limits<int>::max())));
        row.classification = fromUtf8(entry.classification);
        result.rows.push_back(std::move(row));
    }

    return result;
}

QString readAbcTreeEvidence(
    const std::shared_ptr<SessionContext>& context,
    const QString& fallbackPackagePath,
    const QString& pathOrQuery,
    int limit,
    int maxChars,
    std::stop_token stopToken)
{
    PerformanceTrace trace(QStringLiteral("HyleDecompiler::readAbcTreeEvidence"));

    const auto target = resolveAbcEvidenceTarget(context, fallbackPackagePath, pathOrQuery, stopToken);
    if (!target.error.isEmpty()) {
        return QStringLiteral("# status: error\n# code: abc_not_found\n# message: %1").arg(target.error);
    }

    auto parser = openParsedAbcParser(target);
    if (!parser) {
        return QStringLiteral(
            "# status: error\n"
            "# code: open_abc_failed\n"
            "# abc: %1\n"
            "# message: %2")
            .arg(target.displayPath, fromUtf8(parser.error().message()));
    }

    auto tree = hyle::hap::build_abc_tree(*parser);
    if (!tree) {
        return QStringLiteral(
            "# status: error\n"
            "# code: read_abc_tree_failed\n"
            "# abc: %1\n"
            "# message: %2")
            .arg(target.displayPath, fromUtf8(tree.error().message()));
    }

    const int maxClasses = std::clamp(limit <= 0 ? 80 : limit, 1, 1000);
    QString text = formatAbcTargetHeader(target);
    text += QStringLiteral("# classes: %1\n").arg(static_cast<qulonglong>(tree->class_count));
    text += QStringLiteral("# methods: %1\n").arg(static_cast<qulonglong>(tree->method_count));
    text += QStringLiteral("# fields: %1\n").arg(static_cast<qulonglong>(tree->field_count));
    text += QStringLiteral("# code_items: %1\n").arg(static_cast<qulonglong>(tree->code_count));
    text += QStringLiteral("# strings: %1\n").arg(static_cast<qulonglong>(tree->string_count));
    text += QStringLiteral("# literals: %1\n\n").arg(static_cast<qulonglong>(tree->literal_count));

    int classIndex = 0;
    for (const auto& klass : tree->classes) {
        if (classIndex >= maxClasses) {
            text += QStringLiteral("[truncated: %1 of %2 classes shown]\n")
                .arg(maxClasses)
                .arg(static_cast<qulonglong>(tree->classes.size()));
            break;
        }
        ++classIndex;
        text += QStringLiteral("- class %1 @%2\n")
            .arg(fromUtf8(klass.name), hexOffset(klass.offset));
        if (!klass.source_file.empty()) {
            text += QStringLiteral("  source_file: %1\n").arg(fromUtf8(klass.source_file));
        }
        for (const auto& field : klass.fields) {
            text += QStringLiteral("  - field %1 @%2 type=%3\n")
                .arg(fromUtf8(field.name), hexOffset(field.offset), fromUtf8(field.type_name));
        }
        for (const auto& method : klass.methods) {
            text += QStringLiteral("  - method %1 @%2")
                .arg(fromUtf8(method.name), hexOffset(method.offset));
            if (method.code_offset) {
                text += QStringLiteral(" code=%1").arg(hexOffset(*method.code_offset));
            }
            text += QStringLiteral(" args=%1 vregs=%2 refs=%3\n")
                .arg(method.num_args)
                .arg(method.num_vregs)
                .arg(static_cast<qulonglong>(method.reference_count));
        }
    }
    return boundedEvidenceText(text, maxChars);
}

AbcXrefSearchResult findAbcXrefs(
    const std::shared_ptr<SessionContext>& context,
    const QString& fallbackPackagePath,
    const QString& pathOrQuery,
    const QString& query,
    const QString& kind,
    int limit,
    std::stop_token stopToken)
{
    PerformanceTrace trace(QStringLiteral("HyleDecompiler::findAbcXrefs"));

    AbcXrefSearchResult result;
    const auto target = resolveAbcEvidenceTarget(context, fallbackPackagePath, pathOrQuery, stopToken);
    if (!target.error.isEmpty()) {
        result.error = target.error;
        return result;
    }
    result.abcPath = target.displayPath;

    hyle::hap::abc_xref_query xrefQuery;
    xrefQuery.kind = parseBytecodeReferenceKind(kind);
    if (!kind.trimmed().isEmpty() && kind.trimmed().compare(QStringLiteral("any"), Qt::CaseInsensitive) != 0
        && !xrefQuery.kind) {
        result.error = QObject::tr("kind must be string, method, literal, or any.");
        return result;
    }
    xrefQuery.substring = true;
    if (const auto offset = parseAbcOffset(query)) {
        xrefQuery.target_offset = *offset;
    } else {
        xrefQuery.target_text = toUtf8Path(query);
    }

    auto xrefs = hyle::hap::find_abc_xrefs(
        std::filesystem::path(toUtf8Path(target.filePath)),
        xrefQuery);
    if (!xrefs) {
        result.error = errorMessage("Find ABC xrefs failed", xrefs.error());
        return result;
    }

    QHash<QString, QString> classSourceFiles;
    if (auto parser = openParsedAbcParser(target)) {
        if (auto tree = hyle::hap::build_abc_tree(*parser)) {
            for (const auto& klass : tree->classes) {
                if (!klass.source_file.empty()) {
                    classSourceFiles.insert(fromUtf8(klass.name), fromUtf8(klass.source_file));
                }
            }
        }
    }

    const int maxItems = std::clamp(limit <= 0 ? 80 : limit, 1, 1000);
    result.rows.reserve(static_cast<std::size_t>(std::min(maxItems, static_cast<int>(xrefs->size()))));
    int index = 0;
    for (const auto& xref : *xrefs) {
        if (index >= maxItems) {
            break;
        }

        AbcXrefRow row;
        row.index = index++;
        row.kind = bytecodeReferenceKindName(xref.kind);
        row.targetOffset = hexOffset(xref.target_offset);
        row.targetText = fromUtf8(xref.target_text);
        row.operandIndex = xref.index;
        row.classOffset = hexOffset(xref.class_offset);
        row.className = fromUtf8(xref.class_name);
        row.sourceFile = classSourceFiles.value(row.className);
        row.methodOffset = hexOffset(xref.method_offset);
        row.methodName = fromUtf8(xref.method_name);
        row.codeOffset = hexOffset(xref.code_offset);
        row.instructionOffset = hexOffset(xref.instruction_offset);
        result.rows.push_back(std::move(row));
    }

    return result;
}

QString findAbcXrefEvidence(
    const std::shared_ptr<SessionContext>& context,
    const QString& fallbackPackagePath,
    const QString& pathOrQuery,
    const QString& query,
    const QString& kind,
    int limit,
    int maxChars,
    std::stop_token stopToken)
{
    PerformanceTrace trace(QStringLiteral("HyleDecompiler::findAbcXrefEvidence"));

    const auto target = resolveAbcEvidenceTarget(context, fallbackPackagePath, pathOrQuery, stopToken);
    if (!target.error.isEmpty()) {
        return QStringLiteral("# status: error\n# code: abc_not_found\n# message: %1").arg(target.error);
    }

    hyle::hap::abc_xref_query xrefQuery;
    xrefQuery.kind = parseBytecodeReferenceKind(kind);
    if (!kind.trimmed().isEmpty() && kind.trimmed().compare(QStringLiteral("any"), Qt::CaseInsensitive) != 0
        && !xrefQuery.kind) {
        return QStringLiteral("# status: error\n# code: invalid_kind\n# message: kind must be string, method, literal, or any.");
    }
    xrefQuery.substring = true;
    if (const auto offset = parseAbcOffset(query)) {
        xrefQuery.target_offset = *offset;
    } else {
        xrefQuery.target_text = toUtf8Path(query);
    }

    auto xrefs = hyle::hap::find_abc_xrefs(
        std::filesystem::path(toUtf8Path(target.filePath)),
        xrefQuery);
    if (!xrefs) {
        return QStringLiteral(
            "# status: error\n"
            "# code: find_abc_xrefs_failed\n"
            "# abc: %1\n"
            "# message: %2")
            .arg(target.displayPath, fromUtf8(xrefs.error().message()));
    }

    const int maxItems = std::clamp(limit <= 0 ? 80 : limit, 1, 1000);
    QString text = formatAbcTargetHeader(target);
    text += QStringLiteral("# query: %1\n").arg(query);
    if (!kind.trimmed().isEmpty()) {
        text += QStringLiteral("# kind: %1\n").arg(kind);
    }
    text += QStringLiteral("# match_count: %1\n\n").arg(static_cast<qulonglong>(xrefs->size()));

    int index = 0;
    for (const auto& xref : *xrefs) {
        if (index >= maxItems) {
            text += QStringLiteral("[truncated: %1 of %2 xrefs shown]\n")
                .arg(maxItems)
                .arg(static_cast<qulonglong>(xrefs->size()));
            break;
        }
        text += QStringLiteral("- xref[%1]\n").arg(index++);
        text += QStringLiteral("  kind: %1\n").arg(bytecodeReferenceKindName(xref.kind));
        text += QStringLiteral("  target_offset: %1\n").arg(hexOffset(xref.target_offset));
        if (!xref.target_text.empty()) {
            text += QStringLiteral("  target_text: %1\n").arg(quotedPreview(fromUtf8(xref.target_text), 1000));
        }
        text += QStringLiteral("  class: %1 @%2\n").arg(fromUtf8(xref.class_name), hexOffset(xref.class_offset));
        text += QStringLiteral("  method: %1 @%2\n").arg(fromUtf8(xref.method_name), hexOffset(xref.method_offset));
        text += QStringLiteral("  code_offset: %1\n").arg(hexOffset(xref.code_offset));
        text += QStringLiteral("  instruction_offset: %1\n").arg(hexOffset(xref.instruction_offset));
        text += QStringLiteral("  operand_index: %1\n").arg(xref.index);
    }
    if (xrefs->empty()) {
        text += QStringLiteral("[no ABC xrefs matched]\n");
    }
    return boundedEvidenceText(text, maxChars);
}

QString findAbcCallArgumentFlowEvidence(
    const std::shared_ptr<SessionContext>& context,
    const QString& fallbackPackagePath,
    const QString& pathOrQuery,
    const QString& query,
    const QString& kind,
    int limit,
    int maxChars,
    std::stop_token stopToken)
{
    PerformanceTrace trace(QStringLiteral("HyleDecompiler::findAbcCallArgumentFlowEvidence"));

    const auto target = resolveAbcEvidenceTarget(context, fallbackPackagePath, pathOrQuery, stopToken);
    if (!target.error.isEmpty()) {
        return QStringLiteral("# status: error\n# code: abc_not_found\n# message: %1").arg(target.error);
    }

    hyle::hap::abc_call_argument_flow_query flowQuery;
    flowQuery.kind = parseBytecodeReferenceKind(kind);
    if (!kind.trimmed().isEmpty() && kind.trimmed().compare(QStringLiteral("any"), Qt::CaseInsensitive) != 0
        && !flowQuery.kind) {
        return QStringLiteral("# status: error\n# code: invalid_kind\n# message: kind must be string, method, literal, or any.");
    }
    flowQuery.substring = true;
    flowQuery.include_this_argument = true;
    if (const auto offset = parseAbcOffset(query)) {
        flowQuery.target_offset = *offset;
    } else {
        flowQuery.target_text = toUtf8Path(query);
    }

    auto flows = hyle::hap::find_abc_call_argument_flows(
        std::filesystem::path(toUtf8Path(target.filePath)),
        flowQuery);
    if (!flows) {
        return QStringLiteral(
            "# status: error\n"
            "# code: find_abc_call_argument_flows_failed\n"
            "# abc: %1\n"
            "# message: %2")
            .arg(target.displayPath, fromUtf8(flows.error().message()));
    }

    const int maxItems = std::clamp(limit <= 0 ? 80 : limit, 1, 1000);
    QString text = formatAbcTargetHeader(target);
    text += QStringLiteral("# query: %1\n").arg(query);
    if (!kind.trimmed().isEmpty()) {
        text += QStringLiteral("# kind: %1\n").arg(kind);
    }
    text += QStringLiteral("# note: conservative bytecode evidence; semantic interpretation belongs to the Agent.\n");
    text += QStringLiteral("# match_count: %1\n\n").arg(static_cast<qulonglong>(flows->size()));

    int index = 0;
    for (const auto& flow : *flows) {
        if (index >= maxItems) {
            text += QStringLiteral("[truncated: %1 of %2 flows shown]\n")
                .arg(maxItems)
                .arg(static_cast<qulonglong>(flows->size()));
            break;
        }
        text += QStringLiteral("- flow[%1]\n").arg(index++);
        text += QStringLiteral("  kind: %1\n").arg(bytecodeReferenceKindName(flow.kind));
        text += QStringLiteral("  target_offset: %1\n").arg(hexOffset(flow.target_offset));
        if (!flow.target_text.empty()) {
            text += QStringLiteral("  target_text: %1\n").arg(quotedPreview(fromUtf8(flow.target_text), 1000));
        }
        text += QStringLiteral("  class: %1 @%2\n").arg(fromUtf8(flow.class_name), hexOffset(flow.class_offset));
        text += QStringLiteral("  method: %1 @%2\n").arg(fromUtf8(flow.method_name), hexOffset(flow.method_offset));
        text += QStringLiteral("  code_offset: %1\n").arg(hexOffset(flow.code_offset));
        text += QStringLiteral("  source_instruction: %1\n").arg(hexOffset(flow.source_instruction_offset));
        if (flow.source_register) {
            text += QStringLiteral("  source_register: v%1\n").arg(*flow.source_register);
        }
        text += QStringLiteral("  call_instruction: %1\n").arg(hexOffset(flow.call_instruction_offset));
        text += QStringLiteral("  argument_index: %1\n").arg(flow.argument_index);
        text += QStringLiteral("  receiver: %1\n").arg(boolText(flow.this_argument));
    }
    if (flows->empty()) {
        text += QStringLiteral("[no ABC call argument flows matched]\n");
    }
    return boundedEvidenceText(text, maxChars);
}

DisassemblyResult disassembleSourceFileText(
    const std::shared_ptr<SessionContext>& context,
    int nodeIndex,
    std::size_t sourceFileId,
    const QString& name,
    std::stop_token stopToken,
    std::size_t packageId)
{
    PerformanceTrace trace(QStringLiteral("HyleDecompiler::disassembleSourceFileText"));

    DisassemblyResult result;
    result.nodeIndex = nodeIndex;
    result.name = name;

    auto* packageContext = packageSession(context, packageId);
    if (!packageContext || !packageContext->session.valid()) {
        result.error = QObject::tr("Decompiler session is not available.");
        return result;
    }

    LinkedStopToken linkedStop(context->stopToken(), stopToken);
    const std::string sourceName = toUtf8Path(name);
    hyle::hap::abc_disassembly_format_options options;
    options.source_name = sourceName;
    options.resolve_literals = true;
    options.literal_preview_limit = 160;
    auto text = hyle::async::sync_wait(
        packageContext->session.disassemble_source_file_text_async(
            context->scheduler(),
            sourceFileId,
            options,
            linkedStop.token()));
    if (!text) {
        result.error = errorMessage("Disassemble failed", text.error());
        return result;
    }

    result.content = normalizeSourceContent(fromUtf8(*text));
    return result;
}

} // namespace HyleDecompiler
