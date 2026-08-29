#pragma once

#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QUrl>
#include <QVariantMap>

#include <memory>

class Backend final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl modelUrl READ modelUrl NOTIFY modelUrlChanged)
    Q_PROPERTY(QString displayName READ displayName NOTIFY fileInfoChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileInfoChanged)
    Q_PROPERTY(QString fileType READ fileType NOTIFY fileInfoChanged)
    Q_PROPERTY(QString fileSize READ fileSize NOTIFY fileInfoChanged)
    Q_PROPERTY(QString sourcePath READ sourcePath NOTIFY fileInfoChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool modelStatsAvailable READ modelStatsAvailable NOTIFY modelStatsChanged)
    Q_PROPERTY(qulonglong meshCount READ meshCount NOTIFY modelStatsChanged)
    Q_PROPERTY(qulonglong vertexCount READ vertexCount NOTIFY modelStatsChanged)
    Q_PROPERTY(qulonglong triangleCount READ triangleCount NOTIFY modelStatsChanged)

public:
    explicit Backend(bool blendPreviewEnabled = false,
                     QObject *parent = nullptr);
    ~Backend() override;

    QUrl modelUrl() const { return m_modelUrl; }
    QString displayName() const { return m_displayName; }
    QString fileName() const { return m_fileName; }
    QString fileType() const { return m_fileType; }
    QString fileSize() const { return m_fileSize; }
    QString sourcePath() const { return m_sourcePath; }
    QString statusText() const { return m_statusText; }
    QString errorMessage() const { return m_errorMessage; }
    bool busy() const { return m_busy; }
    bool modelStatsAvailable() const { return m_modelStatsAvailable; }
    qulonglong meshCount() const { return m_meshCount; }
    qulonglong vertexCount() const { return m_vertexCount; }
    qulonglong triangleCount() const { return m_triangleCount; }

    Q_INVOKABLE void openFile(const QUrl &url);
    Q_INVOKABLE void openPath(const QString &path);
    Q_INVOKABLE bool acceptsUrl(const QUrl &url) const;
    Q_INVOKABLE QVariantMap modelBounds() const;
    Q_INVOKABLE void openFullViewer();
    Q_INVOKABLE void closePreview();
    Q_INVOKABLE void navigatePreview(int direction);
    Q_INVOKABLE void clearError();
    Q_INVOKABLE QVariantMap windowGeometry() const;
    Q_INVOKABLE void saveWindowGeometry(int x, int y, int width, int height,
                                        bool maximized);

signals:
    void modelUrlChanged();
    void fileInfoChanged();
    void statusTextChanged();
    void errorMessageChanged();
    void busyChanged();
    void modelStatsChanged();

private:
    bool isSupportedExtension(const QString &suffix) const;
    void startFbxConversion(const QString &sourcePath);
    void startBlendConversion(const QString &sourcePath);
    void cancelConversion();
    void startModelStatistics(const QString &sourcePath);
    void cancelModelStatistics();
    void resetModelStatistics();
    void setModelUrl(const QUrl &url);
    void setBusy(bool busy);
    void setStatusText(const QString &text);
    void setError(const QString &message);
    void updateFileInfo(const QString &path, const QString &typeLabel);

    QUrl m_modelUrl;
    QString m_displayName;
    QString m_fileName;
    QString m_fileType;
    QString m_fileSize;
    QString m_sourcePath;
    QString m_statusText = QStringLiteral("Ready");
    QString m_errorMessage;
    bool m_blendPreviewEnabled = false;
    bool m_busy = false;
    bool m_modelStatsAvailable = false;
    qulonglong m_meshCount = 0;
    qulonglong m_vertexCount = 0;
    qulonglong m_triangleCount = 0;

    QProcess *m_converter = nullptr;
    QProcess *m_statsProbe = nullptr;
    std::unique_ptr<QTemporaryDir> m_conversionDirectory;
};
