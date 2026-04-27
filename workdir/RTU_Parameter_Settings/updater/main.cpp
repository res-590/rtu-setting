#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTextStream>
#include <QThread>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
QString escapePsLiteral(const QString &text)
{
    QString escaped = text;
    escaped.replace(QLatin1Char('\''), QStringLiteral("''"));
    return escaped;
}

void logLine(const QString &text)
{
    QTextStream out(stdout);
    out << text << Qt::endl;
}

bool waitForProcessExit(qint64 pid, int timeoutMs)
{
#ifdef Q_OS_WIN
    HANDLE handle = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
    if (handle == nullptr) {
        return true;
    }

    const DWORD result = WaitForSingleObject(handle, static_cast<DWORD>(timeoutMs));
    CloseHandle(handle);
    return result == WAIT_OBJECT_0;
#else
    Q_UNUSED(pid)
    QThread::msleep(timeoutMs);
    return true;
#endif
}

bool runPowerShell(const QString &command)
{
    QProcess process;
    process.start(QStringLiteral("powershell.exe"),
                  QStringList()
                      << QStringLiteral("-NoProfile")
                      << QStringLiteral("-ExecutionPolicy") << QStringLiteral("Bypass")
                      << QStringLiteral("-Command") << command);
    if (!process.waitForFinished(-1)) {
        return false;
    }
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

QString unwrapSingleRootDirectory(const QString &path)
{
    QDir dir(path);
    const QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    if (entries.size() == 1 && entries.first().isDir()) {
        return entries.first().absoluteFilePath();
    }
    return path;
}

bool shouldPreserveFile(const QFileInfo &destinationInfo)
{
    const QString suffix = destinationInfo.suffix().toLower();
    return suffix == QStringLiteral("ini");
}

bool copyRecursively(const QString &sourcePath, const QString &targetPath)
{
    const QFileInfo sourceInfo(sourcePath);
    if (sourceInfo.isDir()) {
        QDir targetDir(targetPath);
        if (!targetDir.exists() && !QDir().mkpath(targetPath)) {
            return false;
        }

        QDir sourceDir(sourcePath);
        const QFileInfoList entries = sourceDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
        for (const QFileInfo &entry : entries) {
            const QString nextSource = entry.absoluteFilePath();
            const QString nextTarget = targetDir.filePath(entry.fileName());
            if (!copyRecursively(nextSource, nextTarget)) {
                return false;
            }
        }
        return true;
    }

    const QFileInfo targetInfo(targetPath);
    if (targetInfo.exists()) {
        if (shouldPreserveFile(targetInfo)) {
            return true;
        }
        if (!QFile::remove(targetPath)) {
            return false;
        }
    }
    return QFile::copy(sourcePath, targetPath);
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();

    QCommandLineOption zipOption(QStringLiteral("zip"), QStringLiteral("更新包 zip 路径"), QStringLiteral("zip"));
    QCommandLineOption targetDirOption(QStringLiteral("target-dir"), QStringLiteral("目标安装目录"), QStringLiteral("dir"));
    QCommandLineOption exeNameOption(QStringLiteral("exe-name"), QStringLiteral("主程序 exe 名称"), QStringLiteral("exe"));
    QCommandLineOption pidOption(QStringLiteral("pid"), QStringLiteral("主程序进程 ID"), QStringLiteral("pid"));
    QCommandLineOption versionOption(QStringLiteral("version"), QStringLiteral("目标版本"), QStringLiteral("version"));

    parser.addOption(zipOption);
    parser.addOption(targetDirOption);
    parser.addOption(exeNameOption);
    parser.addOption(pidOption);
    parser.addOption(versionOption);
    parser.process(app);

    const QString zipPath = parser.value(zipOption);
    const QString targetDir = parser.value(targetDirOption);
    const QString exeName = parser.value(exeNameOption);
    const qint64 pid = parser.value(pidOption).toLongLong();
    const QString targetVersion = parser.value(versionOption);

    if (zipPath.isEmpty() || targetDir.isEmpty() || exeName.isEmpty() || pid <= 0) {
        logLine(QStringLiteral("缺少必需参数。"));
        return 1;
    }

    logLine(QStringLiteral("等待主程序退出..."));
    if (!waitForProcessExit(pid, 120000)) {
        logLine(QStringLiteral("等待主程序退出超时。"));
        return 2;
    }

    const QString tempRoot = QDir::temp().filePath(QStringLiteral("RTU_Updater_Work"));
    const QString extractDir = QDir(tempRoot).filePath(QStringLiteral("extract"));
    QDir(tempRoot).removeRecursively();
    QDir().mkpath(extractDir);

    const QString extractCommand =
        QStringLiteral("Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force")
            .arg(escapePsLiteral(QDir::toNativeSeparators(zipPath)),
                 escapePsLiteral(QDir::toNativeSeparators(extractDir)));
    logLine(QStringLiteral("解压更新包..."));
    if (!runPowerShell(extractCommand)) {
        logLine(QStringLiteral("解压更新包失败。"));
        return 3;
    }

    const QString payloadRoot = unwrapSingleRootDirectory(extractDir);
    logLine(QStringLiteral("覆盖安装目录：%1").arg(targetDir));
    if (!copyRecursively(payloadRoot, targetDir)) {
        logLine(QStringLiteral("复制新版本文件失败。"));
        return 4;
    }

    const QString versionFilePath = QDir(targetDir).filePath(QStringLiteral("version.txt"));
    QFile versionFile(versionFilePath);
    if (versionFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream stream(&versionFile);
        stream << targetVersion << Qt::endl;
    }

    const QString exePath = QDir(targetDir).filePath(exeName);
    logLine(QStringLiteral("重启主程序：%1").arg(exePath));
    if (!QProcess::startDetached(exePath, QStringList(), targetDir)) {
        logLine(QStringLiteral("重启主程序失败。"));
        return 5;
    }

    logLine(QStringLiteral("升级完成。"));
    return 0;
}
