#ifndef UPDATE_MANAGER_H
#define UPDATE_MANAGER_H

#include <QObject>
#include <QByteArray>
#include <QPointer>
#include <QStringList>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QWidget;

class UpdateManager : public QObject
{
    Q_OBJECT

public:
    explicit UpdateManager(QWidget *ownerWidget, QObject *parent = nullptr);

    void scheduleStartupCheck();
    void checkForUpdates(bool manual);

private slots:
    void handleManifestReply();
    void handlePackageProgress(qint64 bytesReceived, qint64 bytesTotal);
    void handlePackageReply();

private:
    struct UpdateConfig {
        bool enabled = false;
        bool autoCheckOnStartup = true;
        int requestTimeoutMs = 15000;
        QUrl manifestUrl;
        QString updaterExecutableName;
    };

    struct UpdateManifest {
        QString version;
        QUrl packageUrl;
        QByteArray sha256;
        QString notes;
    };

    bool loadConfig();
    void resetState();
    void showInfoMessage(const QString &title, const QString &text) const;
    void appendLog(const QString &text) const;
    bool parseManifest(const QByteArray &data, UpdateManifest &manifest, QString &errorText) const;
    bool isNewerVersion(const QString &candidateVersion) const;
    void downloadPackage(const UpdateManifest &manifest);
    bool verifyPackageHash(const QString &filePath, const QByteArray &expectedSha256) const;
    bool launchUpdater(const QString &zipPath, const QString &targetVersion);
    QString updateConfigPath() const;
    QStringList updateConfigCandidates() const;

    QWidget *m_ownerWidget;
    QNetworkAccessManager *m_networkManager;
    QPointer<QNetworkReply> m_manifestReply;
    QPointer<QNetworkReply> m_packageReply;
    UpdateConfig m_config;
    UpdateManifest m_pendingManifest;
    QString m_downloadedPackagePath;
    QString m_loadedConfigPath;
    bool m_manualCheck = false;
};

#endif // UPDATE_MANAGER_H
