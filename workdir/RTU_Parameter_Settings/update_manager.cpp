#include "update_manager.h"

#include "app_metadata.h"
#include "mainwindow.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QVersionNumber>

namespace {
QByteArray normalizeSha256Text(const QString &text)
{
    QByteArray value = text.trimmed().toLatin1().toLower();
    value.replace(" ", "");
    value.replace("\r", "");
    value.replace("\n", "");
    return value;
}

QString describeNetworkError(QNetworkReply *reply)
{
    if (!reply) {
        return QStringLiteral("未知网络错误");
    }

    const QVariant statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    const QString reason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString().trimmed();
    const QUrl redirectUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    QStringList parts;
    const bool hasHttpStatus = statusCode.isValid();

    if (hasHttpStatus) {
        QString statusText = QStringLiteral("HTTP %1").arg(statusCode.toInt());
        if (!reason.isEmpty()) {
            statusText += QStringLiteral(" %1").arg(reason);
        }
        parts << statusText;
    }

    const QString networkError = reply->errorString().trimmed();
    if (!hasHttpStatus
        && !networkError.isEmpty()
        && networkError.compare(QStringLiteral("Unknown error"), Qt::CaseInsensitive) != 0) {
        parts << networkError;
    }

    if (redirectUrl.isValid()) {
        parts << QStringLiteral("重定向到：%1").arg(redirectUrl.toString());
    }

    if (parts.isEmpty()) {
        parts << QStringLiteral("网络请求失败");
    }

    parts << QStringLiteral("地址：%1").arg(reply->url().toString());
    return parts.join(QStringLiteral("，"));
}
}

UpdateManager::UpdateManager(QWidget *ownerWidget, QObject *parent)
    : QObject(parent)
    , m_ownerWidget(ownerWidget)
    , m_networkManager(new QNetworkAccessManager(this))
{
}

void UpdateManager::scheduleStartupCheck()
{
    if (!loadConfig() || !m_config.enabled || !m_config.autoCheckOnStartup) {
        return;
    }

    QTimer::singleShot(1500, this, [this]() {
        checkForUpdates(false);
    });
}

void UpdateManager::checkForUpdates(bool manual)
{
    if (!loadConfig()) {
        if (manual) {
            showInfoMessage(QStringLiteral("检查更新"),
                            QStringLiteral("未找到更新配置文件。\r\n已尝试路径：\r\n%1")
                                .arg(updateConfigCandidates().join(QStringLiteral("\r\n"))));
        }
        return;
    }

    if (!m_config.enabled) {
        if (manual) {
            showInfoMessage(QStringLiteral("检查更新"), QStringLiteral("在线升级已在配置文件中禁用。"));
        }
        return;
    }

    if (!m_config.manifestUrl.isValid()) {
        if (manual) {
            showInfoMessage(QStringLiteral("检查更新"),
                            QStringLiteral("升级清单地址无效，请检查：\r\n%1").arg(m_loadedConfigPath));
        }
        return;
    }

    if (m_manifestReply || m_packageReply) {
        if (manual) {
            showInfoMessage(QStringLiteral("检查更新"), QStringLiteral("当前已有更新任务在执行。"));
        }
        return;
    }

    m_manualCheck = manual;
    appendLog(QStringLiteral("[更新] 使用配置：%1").arg(m_loadedConfigPath));
    appendLog(QStringLiteral("[更新] 开始检查版本：%1").arg(m_config.manifestUrl.toString()));

    QNetworkRequest request(m_config.manifestUrl);
    request.setTransferTimeout(m_config.requestTimeoutMs);
    m_manifestReply = m_networkManager->get(request);
    connect(m_manifestReply, &QNetworkReply::finished, this, &UpdateManager::handleManifestReply);
}

void UpdateManager::handleManifestReply()
{
    if (!m_manifestReply) {
        return;
    }

    const bool manual = m_manualCheck;
    QByteArray data;
    QString errorText;

    if (m_manifestReply->error() != QNetworkReply::NoError) {
        errorText = describeNetworkError(m_manifestReply);
    } else {
        data = m_manifestReply->readAll();
    }

    m_manifestReply->deleteLater();
    m_manifestReply = nullptr;

    if (!errorText.isEmpty()) {
        appendLog(QStringLiteral("[更新] 读取升级清单失败：%1").arg(errorText));
        if (manual) {
            showInfoMessage(QStringLiteral("检查更新"), QStringLiteral("读取升级清单失败：%1").arg(errorText));
        }
        return;
    }

    UpdateManifest manifest;
    if (!parseManifest(data, manifest, errorText)) {
        appendLog(QStringLiteral("[更新] 解析升级清单失败：%1").arg(errorText));
        if (manual) {
            showInfoMessage(QStringLiteral("检查更新"), QStringLiteral("解析升级清单失败：%1").arg(errorText));
        }
        return;
    }

    if (manifest.packageUrl.isRelative()) {
        manifest.packageUrl = m_config.manifestUrl.resolved(manifest.packageUrl);
    }

    if (!isNewerVersion(manifest.version)) {
        appendLog(QStringLiteral("[更新] 当前已是最新版本：%1").arg(QCoreApplication::applicationVersion()));
        if (manual) {
            showInfoMessage(QStringLiteral("检查更新"),
                            QStringLiteral("当前已是最新版本：%1").arg(QCoreApplication::applicationVersion()));
        }
        return;
    }

    QString prompt = QStringLiteral("检测到新版本 %1，当前版本 %2。\r\n是否立即下载并更新？")
                         .arg(manifest.version, QCoreApplication::applicationVersion());
    if (!manifest.notes.trimmed().isEmpty()) {
        prompt += QStringLiteral("\r\n\r\n更新说明：\r\n%1").arg(manifest.notes.trimmed());
    }

    if (QMessageBox::question(m_ownerWidget, QStringLiteral("发现新版本"), prompt) != QMessageBox::Yes) {
        appendLog(QStringLiteral("[更新] 用户取消下载版本 %1").arg(manifest.version));
        return;
    }

    downloadPackage(manifest);
}

void UpdateManager::handlePackageProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal <= 0) {
        return;
    }

    appendLog(QStringLiteral("[更新] 下载进度：%1 / %2 KB")
                  .arg(bytesReceived / 1024)
                  .arg(bytesTotal / 1024));
}

void UpdateManager::handlePackageReply()
{
    if (!m_packageReply) {
        return;
    }

    QString errorText;
    QByteArray data;
    if (m_packageReply->error() != QNetworkReply::NoError) {
        errorText = describeNetworkError(m_packageReply);
    } else {
        data = m_packageReply->readAll();
    }

    m_packageReply->deleteLater();
    m_packageReply = nullptr;

    if (!errorText.isEmpty()) {
        appendLog(QStringLiteral("[更新] 下载更新包失败：%1").arg(errorText));
        showInfoMessage(QStringLiteral("在线升级"), QStringLiteral("下载更新包失败：%1").arg(errorText));
        resetState();
        return;
    }

    QSaveFile file(m_downloadedPackagePath);
    if (!file.open(QIODevice::WriteOnly)) {
        showInfoMessage(QStringLiteral("在线升级"),
                        QStringLiteral("无法写入更新包：%1").arg(QDir::toNativeSeparators(m_downloadedPackagePath)));
        resetState();
        return;
    }
    file.write(data);
    if (!file.commit()) {
        showInfoMessage(QStringLiteral("在线升级"),
                        QStringLiteral("保存更新包失败：%1").arg(QDir::toNativeSeparators(m_downloadedPackagePath)));
        resetState();
        return;
    }

    if (!verifyPackageHash(m_downloadedPackagePath, m_pendingManifest.sha256)) {
        appendLog(QStringLiteral("[更新] 更新包校验失败：%1").arg(m_downloadedPackagePath));
        QFile::remove(m_downloadedPackagePath);
        showInfoMessage(QStringLiteral("在线升级"), QStringLiteral("更新包校验失败，已终止升级。"));
        resetState();
        return;
    }

    appendLog(QStringLiteral("[更新] 更新包下载完成：%1").arg(m_downloadedPackagePath));
    if (!launchUpdater(m_downloadedPackagePath, m_pendingManifest.version)) {
        showInfoMessage(QStringLiteral("在线升级"),
                        QStringLiteral("启动升级器失败，请检查发布目录中的 %1 是否存在。")
                            .arg(APP_UPDATER_EXE_LITERAL));
        resetState();
        return;
    }

    QMessageBox::information(m_ownerWidget,
                             QStringLiteral("在线升级"),
                             QStringLiteral("更新包已下载完成，程序即将退出并启动升级器。"));
    QCoreApplication::quit();
}

bool UpdateManager::loadConfig()
{
    m_loadedConfigPath.clear();
    const QStringList candidates = updateConfigCandidates();
    QString configPath;
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            configPath = candidate;
            break;
        }
    }

    if (configPath.isEmpty()) {
        return false;
    }

    m_loadedConfigPath = QDir::toNativeSeparators(configPath);
    QSettings settings(configPath, QSettings::IniFormat);

    m_config.enabled = settings.value(QStringLiteral("Update/Enabled"), true).toBool();
    m_config.autoCheckOnStartup = settings.value(QStringLiteral("Update/AutoCheckOnStartup"), true).toBool();
    m_config.requestTimeoutMs = settings.value(QStringLiteral("Update/RequestTimeoutMs"), 15000).toInt();
    m_config.updaterExecutableName = settings.value(QStringLiteral("Update/UpdaterExecutable"),
                                                    APP_UPDATER_EXE_LITERAL).toString().trimmed();

    const QString manifestValue = settings.value(QStringLiteral("Update/ManifestUrl")).toString().trimmed();
    if (manifestValue.isEmpty()) {
        m_config.manifestUrl = QUrl();
    } else {
        m_config.manifestUrl = QUrl::fromUserInput(manifestValue,
                                                   QFileInfo(configPath).absolutePath(),
                                                   QUrl::AssumeLocalFile);
    }
    return true;
}

void UpdateManager::resetState()
{
    m_pendingManifest = UpdateManifest();
    m_downloadedPackagePath.clear();
}

void UpdateManager::showInfoMessage(const QString &title, const QString &text) const
{
    QMessageBox::information(m_ownerWidget, title, text);
}

void UpdateManager::appendLog(const QString &text) const
{
    if (MainWindow *window = qobject_cast<MainWindow *>(m_ownerWidget)) {
        window->appendSideLog(text + QStringLiteral("\r\n"));
    }
}

bool UpdateManager::parseManifest(const QByteArray &data, UpdateManifest &manifest, QString &errorText) const
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        errorText = parseError.errorString();
        return false;
    }

    const QJsonObject obj = doc.object();
    manifest.version = obj.value(QStringLiteral("version")).toString().trimmed();
    manifest.packageUrl = QUrl(obj.value(QStringLiteral("url")).toString().trimmed());
    manifest.sha256 = normalizeSha256Text(obj.value(QStringLiteral("sha256")).toString());
    manifest.notes = obj.value(QStringLiteral("notes")).toString().trimmed();

    if (manifest.version.isEmpty()) {
        errorText = QStringLiteral("缺少 version 字段");
        return false;
    }
    if (!manifest.packageUrl.isValid()) {
        errorText = QStringLiteral("缺少有效的 url 字段");
        return false;
    }
    return true;
}

bool UpdateManager::isNewerVersion(const QString &candidateVersion) const
{
    const QVersionNumber current = QVersionNumber::fromString(QCoreApplication::applicationVersion());
    const QVersionNumber candidate = QVersionNumber::fromString(candidateVersion);
    return QVersionNumber::compare(candidate, current) > 0;
}

void UpdateManager::downloadPackage(const UpdateManifest &manifest)
{
    m_pendingManifest = manifest;

    const QString tempRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString downloadDir = QDir(tempRoot).filePath(APP_PRODUCT_ID_LITERAL + QStringLiteral("/updates"));
    QDir().mkpath(downloadDir);

    const QString zipFileName = QStringLiteral("%1-%2.zip")
                                    .arg(APP_PRODUCT_ID_LITERAL, manifest.version);
    m_downloadedPackagePath = QDir(downloadDir).filePath(zipFileName);

    appendLog(QStringLiteral("[更新] 开始下载版本 %1：%2")
                  .arg(manifest.version, manifest.packageUrl.toString()));

    QNetworkRequest request(manifest.packageUrl);
    request.setTransferTimeout(m_config.requestTimeoutMs);
    m_packageReply = m_networkManager->get(request);
    connect(m_packageReply, &QNetworkReply::downloadProgress,
            this, &UpdateManager::handlePackageProgress);
    connect(m_packageReply, &QNetworkReply::finished,
            this, &UpdateManager::handlePackageReply);
}

bool UpdateManager::verifyPackageHash(const QString &filePath, const QByteArray &expectedSha256) const
{
    if (expectedSha256.isEmpty()) {
        return true;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        hash.addData(file.read(1024 * 256));
    }

    return hash.result().toHex().toLower() == expectedSha256.toLower();
}

bool UpdateManager::launchUpdater(const QString &zipPath, const QString &targetVersion)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString sourceUpdater = QDir(appDir).filePath(m_config.updaterExecutableName.isEmpty()
                                                            ? APP_UPDATER_EXE_LITERAL
                                                            : m_config.updaterExecutableName);
    if (!QFileInfo::exists(sourceUpdater)) {
        appendLog(QStringLiteral("[更新] 未找到升级器：%1").arg(sourceUpdater));
        return false;
    }

    const QString tempUpdaterDir = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                       .filePath(APP_PRODUCT_ID_LITERAL + QStringLiteral("/updater"));
    QDir().mkpath(tempUpdaterDir);
    const QString tempUpdaterPath = QDir(tempUpdaterDir).filePath(APP_UPDATER_EXE_LITERAL);
    QFile::remove(tempUpdaterPath);
    if (!QFile::copy(sourceUpdater, tempUpdaterPath)) {
        appendLog(QStringLiteral("[更新] 复制升级器失败：%1").arg(tempUpdaterPath));
        return false;
    }

    QStringList args;
    args << QStringLiteral("--zip") << QDir::toNativeSeparators(zipPath)
         << QStringLiteral("--target-dir") << QDir::toNativeSeparators(appDir)
         << QStringLiteral("--exe-name") << QFileInfo(QCoreApplication::applicationFilePath()).fileName()
         << QStringLiteral("--pid") << QString::number(QCoreApplication::applicationPid())
         << QStringLiteral("--version") << targetVersion;

    appendLog(QStringLiteral("[更新] 启动升级器：%1").arg(tempUpdaterPath));
    return QProcess::startDetached(tempUpdaterPath, args, tempUpdaterDir);
}

QString UpdateManager::updateConfigPath() const
{
    const QStringList candidates = updateConfigCandidates();
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return candidates.value(0);
}

QStringList UpdateManager::updateConfigCandidates() const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString currentDir = QDir::currentPath();
    QStringList candidates;

    candidates << QDir(appDir).filePath(QStringLiteral("config/%1").arg(APP_UPDATE_CONFIG_NAME_LITERAL))
               << QDir(appDir).filePath(APP_UPDATE_CONFIG_NAME_LITERAL)
               << QDir(currentDir).filePath(QStringLiteral("config/%1").arg(APP_UPDATE_CONFIG_NAME_LITERAL))
               << QDir(currentDir).filePath(APP_UPDATE_CONFIG_NAME_LITERAL);

    QDir parentDir(appDir);
    for (int depth = 0; depth < 4; ++depth) {
        parentDir.cdUp();
        candidates << parentDir.filePath(QStringLiteral("config/%1").arg(APP_UPDATE_CONFIG_NAME_LITERAL))
                   << parentDir.filePath(APP_UPDATE_CONFIG_NAME_LITERAL)
                   << parentDir.filePath(QStringLiteral("deploy/%1").arg(APP_UPDATE_CONFIG_NAME_LITERAL))
                   << parentDir.filePath(QStringLiteral("deploy/update_config.ini.example"));
    }

    candidates.removeDuplicates();
    return candidates;
}
