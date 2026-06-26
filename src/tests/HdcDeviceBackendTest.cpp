#include "device/HdcDeviceBackend.h"
#include "device/InstallablePackageResolver.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>

namespace {

int fail(const QString& message)
{
    QTextStream(stderr) << "FAIL: " << message << '\n';
    return 1;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    const QList<HdcDeviceTarget> targets = HdcDeviceBackend::parseTargets(QStringLiteral(
        "127.0.0.1:5555\tConnected\n"
        "ABCDEF123456 unauthorized\n"
        "Empty\n"));
    if (targets.size() != 2) {
        return fail(QStringLiteral("target parser should ignore Empty and keep real lines"));
    }
    if (targets.at(0).id != QStringLiteral("127.0.0.1:5555")
        || targets.at(0).state != QStringLiteral("Connected")) {
        return fail(QStringLiteral("first target was parsed incorrectly"));
    }
    if (targets.at(1).id != QStringLiteral("ABCDEF123456")
        || targets.at(1).state != QStringLiteral("unauthorized")) {
        return fail(QStringLiteral("second target was parsed incorrectly"));
    }
    if (!HdcDeviceBackend::parseTargets(QStringLiteral("[Empty]\n")).isEmpty()
        || !HdcDeviceBackend::parseTargets(QStringLiteral("[empty]\n")).isEmpty()) {
        return fail(QStringLiteral("target parser should ignore bracketed Empty sentinel output"));
    }

    HdcDeviceBackend backend;
    const CommandRequest listRequest = backend.listTargetsRequest();
    if (listRequest.program.trimmed().isEmpty()
        || listRequest.arguments != QStringList({ QStringLiteral("list"), QStringLiteral("targets") })) {
        return fail(QStringLiteral("list targets argv is not stable"));
    }

    const CommandRequest installRequest = backend.installRequest(
        QStringLiteral("D:/samples/app with space.hap"),
        QStringLiteral("target-1"));
    const QString nativeInstallPath =
        QDir::toNativeSeparators(QFileInfo(QStringLiteral("D:/samples/app with space.hap")).absoluteFilePath());
    if (installRequest.arguments != QStringList({
            QStringLiteral("-t"),
            QStringLiteral("target-1"),
            QStringLiteral("install"),
            QStringLiteral("-r"),
            nativeInstallPath })) {
        return fail(QStringLiteral("install argv should replace existing packages and preserve target/package arguments"));
    }

    const CommandRequest tempInstallRequest = backend.installRequest(
        QStringLiteral("C:/Users/example/AppData/Local/Temp/ReArk/000-entry-default.hap"),
        {});
    const QString nativeTempInstallPath =
        QDir::toNativeSeparators(QFileInfo(QStringLiteral("C:/Users/example/AppData/Local/Temp/ReArk/000-entry-default.hap")).absoluteFilePath());
    if (tempInstallRequest.arguments.last() != nativeTempInstallPath
        || tempInstallRequest.arguments.last().contains(QLatin1Char('/'))) {
        return fail(QStringLiteral("install argv should pass hdc a native absolute host path"));
    }

    const CommandRequest startRequest = backend.startAbilityRequest(
        QStringLiteral("com.example.app"),
        QStringLiteral("EntryAbility"),
        QStringLiteral("entry"),
        QStringLiteral("target-1"));
    if (startRequest.arguments != QStringList({
            QStringLiteral("-t"),
            QStringLiteral("target-1"),
            QStringLiteral("shell"),
            QStringLiteral("aa"),
            QStringLiteral("start"),
            QStringLiteral("-b"),
            QStringLiteral("com.example.app"),
            QStringLiteral("-a"),
            QStringLiteral("EntryAbility"),
            QStringLiteral("-m"),
            QStringLiteral("entry"),
            QStringLiteral("-W") })) {
        return fail(QStringLiteral("aa start argv changed unexpectedly"));
    }

    const CommandRequest missionRequest = backend.missionListRequest(QStringLiteral("target-1"));
    if (missionRequest.arguments != QStringList({
            QStringLiteral("-t"),
            QStringLiteral("target-1"),
            QStringLiteral("shell"),
            QStringLiteral("aa"),
            QStringLiteral("dump"),
            QStringLiteral("-l") })) {
        return fail(QStringLiteral("aa mission dump argv changed unexpectedly"));
    }

    const CommandRequest processRequest = backend.processListRequest(QStringLiteral("target-1"));
    if (processRequest.arguments != QStringList({
            QStringLiteral("-t"),
            QStringLiteral("target-1"),
            QStringLiteral("shell"),
            QStringLiteral("ps"),
            QStringLiteral("-ef") })) {
        return fail(QStringLiteral("process list argv changed unexpectedly"));
    }

    const CommandRequest allHilogRequest = backend.hilogRequest(QStringLiteral("target-1"));
    if (allHilogRequest.arguments != QStringList({
            QStringLiteral("-t"),
            QStringLiteral("target-1"),
            QStringLiteral("shell"),
            QStringLiteral("hilog"),
            QStringLiteral("-x") })) {
        return fail(QStringLiteral("all-level hilog argv changed unexpectedly"));
    }

    const CommandRequest errorHilogRequest = backend.hilogRequest(
        QStringLiteral("target-1"),
        QStringLiteral("Error"));
    if (errorHilogRequest.arguments != QStringList({
            QStringLiteral("-t"),
            QStringLiteral("target-1"),
            QStringLiteral("shell"),
            QStringLiteral("hilog"),
            QStringLiteral("-x"),
            QStringLiteral("-L"),
            QStringLiteral("E") })) {
        return fail(QStringLiteral("level-filtered hilog argv changed unexpectedly"));
    }

    const CommandRequest screenshotRequest = backend.screenshotCaptureRequest(
        QStringLiteral("/data/local/tmp/reark-screenshot.jpeg"),
        QStringLiteral("target-1"));
    if (screenshotRequest.arguments != QStringList({
            QStringLiteral("-t"),
            QStringLiteral("target-1"),
            QStringLiteral("shell"),
            QStringLiteral("snapshot_display"),
            QStringLiteral("-f"),
            QStringLiteral("/data/local/tmp/reark-screenshot.jpeg") })) {
        return fail(QStringLiteral("snapshot_display argv should use a device-accepted jpeg path"));
    }

    const CommandRequest receiveRequest = backend.receiveFileRequest(
        QStringLiteral("/data/local/tmp/reark-screenshot.jpeg"),
        QStringLiteral("C:/Users/example/AppData/Local/Temp/ReArk/reark-screenshot.jpeg"),
        QStringLiteral("target-1"));
    if (receiveRequest.arguments.last() != QDir::toNativeSeparators(QStringLiteral("C:/Users/example/AppData/Local/Temp/ReArk/reark-screenshot.jpeg"))) {
        return fail(QStringLiteral("file recv local path should use native separators"));
    }

    const CommandRequest forwardRequest = backend.forwardTcpRequest(
        19289,
        19289,
        QStringLiteral("target-1"));
    if (forwardRequest.arguments != QStringList({
            QStringLiteral("-t"),
            QStringLiteral("target-1"),
            QStringLiteral("fport"),
            QStringLiteral("tcp:19289"),
            QStringLiteral("tcp:19289") })) {
        return fail(QStringLiteral("fport argv should map host and device stream ports"));
    }

    const QString bundledPath =
        HdcDeviceBackend::bundledHdcPathForApplicationDir(QStringLiteral("D:/ReArk/bin"));
    if (bundledPath != QStringLiteral("D:/ReArk/bin/plugin/hdc/hdc.exe")) {
        return fail(QStringLiteral("bundled HDC path should use the app-local plugin path only"));
    }

    const QString filtered = HdcDeviceBackend::filterHilog(
        QStringLiteral("one\nBridge: ready\ntwo\nBridge: done\n"),
        QStringLiteral("bridge"),
        1);
    if (filtered != QStringLiteral("Bridge: done")) {
        return fail(QStringLiteral("hilog filtering should be case-insensitive and keep the newest bounded lines"));
    }

    const QString startingMissionDump = QStringLiteral(
        "Mission ID #66  mission name #[#com.example.arks:entry:EntryAbility]\n"
        "  AbilityRecord ID #7490\n"
        "    app name [com.example.arks]\n"
        "    bundle name [com.example.arks]\n"
        "    state #INITIAL\n"
        "    app state #BEGIN\n"
        "    ready #0  window attached #0\n");
    if (!HdcDeviceBackend::missionDumpHasBundleRecord(startingMissionDump, QStringLiteral("com.example.arks"))) {
        return fail(QStringLiteral("mission parser should detect bundle records"));
    }
    if (HdcDeviceBackend::missionDumpShowsVisibleBundle(startingMissionDump, QStringLiteral("com.example.arks"))) {
        return fail(QStringLiteral("INITIAL/BEGIN mission records must not be treated as visible launches"));
    }

    const QString visibleMissionDump = QStringLiteral(
        "Mission ID #72  mission name #[#com.example.arks:entry:EntryAbility]\n"
        "  AbilityRecord ID #7500\n"
        "    app name [com.example.arks]\n"
        "    bundle name [com.example.arks]\n"
        "    state #FOREGROUND\n"
        "    app state #FOREGROUND\n"
        "    ready #1  window attached #1\n");
    if (!HdcDeviceBackend::missionDumpShowsVisibleBundle(visibleMissionDump, QStringLiteral("com.example.arks"))) {
        return fail(QStringLiteral("ready attached mission records should be treated as visible launches"));
    }

    CommandResult result;
    result.program = QStringLiteral("hdc");
    result.arguments = { QStringLiteral("install"), QStringLiteral("D:/samples/app with space.hap") };
    result.started = true;
    result.exitCode = 0;
    result.processError = QProcess::UnknownError;
    const QString commandLine = result.commandLine();
    if (!commandLine.contains(QStringLiteral("\"D:/samples/app with space.hap\""))) {
        return fail(QStringLiteral("command display should quote arguments with spaces"));
    }
    if (!HdcDeviceBackend::installSucceeded(result)) {
        return fail(QStringLiteral("clean hdc install result should be accepted when the process succeeded"));
    }

    result.standardOutput = QStringLiteral("[Info]App install path:D:\\sample.hap msg:error: failed to install bundle. code:9568320 error: no signature file.\nAppMod finish\n");
    if (HdcDeviceBackend::installSucceeded(result)
        || !HdcDeviceBackend::installOutputReportsFailure(result)
        || !HdcDeviceBackend::resultSummary(result).contains(QStringLiteral("hdc_reported_failure"))) {
        return fail(QStringLiteral("hdc install failures reported on stdout must not be treated as success"));
    }

    result.standardOutput = QStringLiteral("[Info]App install path:D:\\sample.hap msg:error: failed to install bundle. code:9568329 error: verify signature failed.\nAppMod finish\n");
    if (HdcDeviceBackend::installSucceeded(result)) {
        return fail(QStringLiteral("signature verification failures must not be treated as successful installs"));
    }

    result.standardOutput = QStringLiteral(
        "error: failed to start ability.\n"
        "Error Code:10104001  Error Message:The specified ability does not exist\n"
        "Error cause: The specified Ability is not installed\n");
    if (HdcDeviceBackend::startSucceeded(result)
        || !HdcDeviceBackend::startOutputReportsFailure(result)
        || !HdcDeviceBackend::resultSummary(result).contains(QStringLiteral("hdc_reported_failure"))) {
        return fail(QStringLiteral("aa start failures reported on stdout must not be treated as success"));
    }

    result.standardOutput = QStringLiteral(
        "StartMode: Hot\n"
        "BundleName: com.example.arks\n"
        "AbilityName: EntryAbility\n"
        "ModuleName: entry\n"
        "start ability successfully.\n");
    if (!HdcDeviceBackend::startSucceeded(result)) {
        return fail(QStringLiteral("successful aa start output should be accepted"));
    }

    QTemporaryDir packageDir;
    if (!packageDir.isValid()) {
        return fail(QStringLiteral("temporary package dir should be available"));
    }

    const QString directHap = packageDir.filePath(QStringLiteral("direct.hap"));
    const QString entryHap = packageDir.filePath(QStringLiteral("000-entry-default.hap"));
    const QString featureHap = packageDir.filePath(QStringLiteral("001-feature.hap"));
    for (const QString& path : { directHap, entryHap, featureHap }) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            return fail(QStringLiteral("test HAP file should be writable: %1").arg(path));
        }
        file.write("hap");
    }

    InstallablePackageSelection selection = InstallablePackageResolver::select(
        {},
        directHap);
    if (!selection.ok || selection.path != directHap) {
        return fail(QStringLiteral("current .hap should be directly installable"));
    }

    selection = InstallablePackageResolver::select(
        {
            { entryHap, QStringLiteral("entry-default.hap") }
        },
        QStringLiteral("D:/samples/sample.app"));
    if (!selection.ok || selection.path != entryHap || selection.displayName != QStringLiteral("entry-default.hap")) {
        return fail(QStringLiteral("single inner HAP should be selected for an APP package"));
    }

    selection = InstallablePackageResolver::select(
        {
            { entryHap, QStringLiteral("entry-default.hap") },
            { featureHap, QStringLiteral("feature.hap") }
        },
        QStringLiteral("D:/samples/sample.app"));
    if (selection.ok || !selection.diagnostic.contains(QStringLiteral("multiple HAP modules"))) {
        return fail(QStringLiteral("multi-HAP APP should require an explicit module selector"));
    }

    selection = InstallablePackageResolver::select(
        {
            { entryHap, QStringLiteral("entry-default.hap") },
            { featureHap, QStringLiteral("feature.hap") }
        },
        QStringLiteral("D:/samples/sample.app"),
        QStringLiteral("feature"));
    if (!selection.ok || selection.path != featureHap) {
        return fail(QStringLiteral("module selector should choose the matching HAP"));
    }

    selection = InstallablePackageResolver::select(
        {
            { packageDir.filePath(QStringLiteral("missing.hap")), QStringLiteral("missing.hap") }
        },
        QStringLiteral("D:/samples/sample.app"));
    if (selection.ok || !selection.diagnostic.contains(QStringLiteral("not available on disk"))) {
        return fail(QStringLiteral("missing selected HAP should be reported before hdc install"));
    }

    QTextStream(stdout) << "HDC device backend tests passed\n";
    return 0;
}
