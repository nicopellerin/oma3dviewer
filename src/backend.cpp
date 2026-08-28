#include "backend.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

namespace {
constexpr auto geometryGroup = "window";
}

Backend::Backend(QObject *parent)
    : QObject(parent) {}

Backend::~Backend() {
    cancelModelStatistics();
    cancelConversion();
}

bool Backend::isSupportedExtension(const QString &suffix) {
    const QString normalized = suffix.toLower();
    return normalized == QStringLiteral("glb")
        || normalized == QStringLiteral("gltf")
        || normalized == QStringLiteral("obj")
        || normalized == QStringLiteral("fbx");
}

bool Backend::acceptsUrl(const QUrl &url) const {
    if (!url.isLocalFile())
        return false;
    return isSupportedExtension(QFileInfo(url.toLocalFile()).suffix());
}

QVariantMap Backend::modelBounds() const {
    QVariantMap result{{QStringLiteral("valid"), false}};
    if (m_sourcePath.isEmpty())
        return result;

    const QString assimp = QStandardPaths::findExecutable(QStringLiteral("assimp"));
    if (assimp.isEmpty())
        return result;

    QProcess probe;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    probe.setProcessEnvironment(environment);
    probe.setProcessChannelMode(QProcess::MergedChannels);
    probe.start(assimp, {QStringLiteral("info"), m_sourcePath});
    if (!probe.waitForStarted(2000) || !probe.waitForFinished(15000)) {
        probe.kill();
        probe.waitForFinished(500);
        return result;
    }

    const QString output = QString::fromUtf8(probe.readAll());
    static const QRegularExpression pointPattern(
        QStringLiteral(
            R"((Minimum|Maximum) point\s+\(\s*([-+0-9.eE]+)\s+([-+0-9.eE]+)\s+([-+0-9.eE]+)\s*\))"));

    bool hasMinimum = false;
    bool hasMaximum = false;
    auto matches = pointPattern.globalMatch(output);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        const QString prefix = match.captured(1).toLower();
        result.insert(prefix + QStringLiteral("X"), match.captured(2).toDouble());
        result.insert(prefix + QStringLiteral("Y"), match.captured(3).toDouble());
        result.insert(prefix + QStringLiteral("Z"), match.captured(4).toDouble());
        hasMinimum = hasMinimum || prefix == QStringLiteral("minimum");
        hasMaximum = hasMaximum || prefix == QStringLiteral("maximum");
    }

    result.insert(QStringLiteral("valid"), hasMinimum && hasMaximum);
    return result;
}

void Backend::openFullViewer() {
    if (m_sourcePath.isEmpty())
        return;

    if (!QProcess::startDetached(QCoreApplication::applicationFilePath(),
                                 {m_sourcePath})) {
        setError(QStringLiteral("Could not open the full Omaviewer window."));
        return;
    }
    closePreview();
}

void Backend::closePreview() {
    const QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.gnome.NautilusPreviewer"),
        QStringLiteral("/org/gnome/NautilusPreviewer"),
        QStringLiteral("org.gnome.NautilusPreviewer2"),
        QStringLiteral("Close"));
    QDBusConnection::sessionBus().send(message);
    QTimer::singleShot(0, QCoreApplication::instance(),
                       &QCoreApplication::quit);
}

void Backend::navigatePreview(int direction) {
    if (direction < 0)
        return;

    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.gnome.NautilusPreviewer"),
        QStringLiteral("/org/gnome/NautilusPreviewer/OmaviewerBridge"),
        QStringLiteral("io.nicopellerin.OmaviewerBridge"),
        QStringLiteral("Select"));
    message << QVariant::fromValue(static_cast<quint32>(direction));
    QDBusConnection::sessionBus().send(message);
}

void Backend::openFile(const QUrl &url) {
    if (!url.isLocalFile()) {
        setError(QStringLiteral("Omaviewer can only open local model files."));
        return;
    }
    openPath(url.toLocalFile());
}

void Backend::openPath(const QString &path) {
    const QFileInfo info(QDir::cleanPath(path));
    if (!info.exists() || !info.isFile()) {
        setError(QStringLiteral("The selected model does not exist."));
        return;
    }

    const QString suffix = info.suffix().toLower();
    if (!isSupportedExtension(suffix)) {
        setError(QStringLiteral("Unsupported model format. Open a GLB, glTF, OBJ, or FBX file."));
        return;
    }

    cancelConversion();
    cancelModelStatistics();
    resetModelStatistics();
    clearError();
    setModelUrl({});

    if (suffix == QStringLiteral("fbx")) {
        updateFileInfo(info.absoluteFilePath(), QStringLiteral("FBX"));
        startModelStatistics(info.absoluteFilePath());
        startFbxConversion(info.absoluteFilePath());
        return;
    }

    updateFileInfo(info.absoluteFilePath(), suffix.toUpper());
    startModelStatistics(info.absoluteFilePath());
    setModelUrl(QUrl::fromLocalFile(info.absoluteFilePath()));
    setStatusText(QStringLiteral("Loaded"));
}

void Backend::startModelStatistics(const QString &sourcePath) {
    const QString assimp = QStandardPaths::findExecutable(QStringLiteral("assimp"));
    if (assimp.isEmpty())
        return;

    auto *probe = new QProcess(this);
    m_statsProbe = probe;

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    probe->setProcessEnvironment(environment);
    probe->setProcessChannelMode(QProcess::MergedChannels);

    connect(probe, &QProcess::errorOccurred, this,
            [this, probe](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || m_statsProbe != probe)
            return;

        m_statsProbe = nullptr;
        probe->deleteLater();
    });

    connect(probe,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this, probe](int exitCode, QProcess::ExitStatus exitStatus) {
        if (m_statsProbe != probe) {
            probe->deleteLater();
            return;
        }
        m_statsProbe = nullptr;

        const QString output = QString::fromUtf8(probe->readAll());
        probe->deleteLater();
        if (exitStatus != QProcess::NormalExit || exitCode != 0)
            return;

        const auto readCount = [&output](const QString &label,
                                         qulonglong *value) {
            const QRegularExpression expression(
                QStringLiteral(R"(^\s*%1:\s*(\d+)\s*$)")
                    .arg(QRegularExpression::escape(label)),
                QRegularExpression::MultilineOption);
            const QRegularExpressionMatch match = expression.match(output);
            if (!match.hasMatch())
                return false;

            bool valid = false;
            const qulonglong parsed = match.captured(1).toULongLong(&valid);
            if (valid)
                *value = parsed;
            return valid;
        };

        qulonglong meshes = 0;
        qulonglong vertices = 0;
        qulonglong triangles = 0;
        if (!readCount(QStringLiteral("Meshes"), &meshes)
            || !readCount(QStringLiteral("Vertices"), &vertices)
            || !readCount(QStringLiteral("Faces"), &triangles)) {
            return;
        }

        const QRegularExpression triangleType(
            QStringLiteral(R"(^\s*Primitive Types:\s*triangles\s*$)"),
            QRegularExpression::MultilineOption
                | QRegularExpression::CaseInsensitiveOption);
        if (!triangleType.match(output).hasMatch())
            return;

        m_meshCount = meshes;
        m_vertexCount = vertices;
        m_triangleCount = triangles;
        m_modelStatsAvailable = true;
        emit modelStatsChanged();
    });

    probe->start(assimp, {
        QStringLiteral("info"), sourcePath, QStringLiteral("--silent")
    });
}

void Backend::cancelModelStatistics() {
    if (!m_statsProbe)
        return;

    disconnect(m_statsProbe, nullptr, this, nullptr);
    if (m_statsProbe->state() != QProcess::NotRunning) {
        m_statsProbe->kill();
        m_statsProbe->waitForFinished(500);
    }
    delete m_statsProbe;
    m_statsProbe = nullptr;
}

void Backend::resetModelStatistics() {
    if (!m_modelStatsAvailable && m_meshCount == 0
        && m_vertexCount == 0 && m_triangleCount == 0) {
        return;
    }

    m_modelStatsAvailable = false;
    m_meshCount = 0;
    m_vertexCount = 0;
    m_triangleCount = 0;
    emit modelStatsChanged();
}

void Backend::startFbxConversion(const QString &sourcePath) {
    const QString assimp = QStandardPaths::findExecutable(QStringLiteral("assimp"));
    if (assimp.isEmpty()) {
        setError(QStringLiteral("FBX support requires Assimp. Install it with: omarchy pkg add assimp"));
        setStatusText(QStringLiteral("FBX unavailable"));
        return;
    }

    m_conversionDirectory = std::make_unique<QTemporaryDir>(
        QDir::tempPath() + QStringLiteral("/omaviewer-XXXXXX"));
    if (!m_conversionDirectory->isValid()) {
        setError(QStringLiteral("Could not create a temporary directory for FBX conversion."));
        setStatusText(QStringLiteral("Conversion failed"));
        m_conversionDirectory.reset();
        return;
    }

    const QString outputPath = m_conversionDirectory->filePath(QStringLiteral("model.glb"));
    m_converter = new QProcess(this);
    m_converter->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_converter, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            setBusy(false);
            setStatusText(QStringLiteral("Conversion failed"));
            setError(QStringLiteral("Assimp could not be started."));
        }
    });

    connect(m_converter,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this, outputPath](int exitCode, QProcess::ExitStatus exitStatus) {
        const QString processOutput = QString::fromUtf8(m_converter->readAll()).trimmed();
        setBusy(false);

        if (exitStatus == QProcess::NormalExit && exitCode == 0
            && QFileInfo::exists(outputPath)) {
            m_fileType = QStringLiteral("FBX · converted");
            emit fileInfoChanged();
            setModelUrl(QUrl::fromLocalFile(outputPath));
            setStatusText(QStringLiteral("Loaded"));
        } else {
            setStatusText(QStringLiteral("Conversion failed"));
            const QString detail = processOutput.isEmpty()
                ? QStringLiteral("Assimp could not convert this FBX file.")
                : processOutput;
            setError(detail);
        }

        m_converter->deleteLater();
        m_converter = nullptr;
    });

    setBusy(true);
    setStatusText(QStringLiteral("Converting FBX…"));
    m_converter->start(assimp, {
        QStringLiteral("export"), sourcePath, outputPath, QStringLiteral("-fglb2")
    });
}

void Backend::cancelConversion() {
    if (m_converter) {
        disconnect(m_converter, nullptr, this, nullptr);
        if (m_converter->state() != QProcess::NotRunning) {
            m_converter->kill();
            m_converter->waitForFinished(500);
        }
        delete m_converter;
        m_converter = nullptr;
    }
    m_conversionDirectory.reset();
    setBusy(false);
}

void Backend::setModelUrl(const QUrl &url) {
    if (m_modelUrl == url)
        return;
    m_modelUrl = url;
    emit modelUrlChanged();
}

void Backend::setBusy(bool busy) {
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void Backend::setStatusText(const QString &text) {
    if (m_statusText == text)
        return;
    m_statusText = text;
    emit statusTextChanged();
}

void Backend::setError(const QString &message) {
    if (m_errorMessage == message)
        return;
    m_errorMessage = message;
    emit errorMessageChanged();
}

void Backend::clearError() {
    setError({});
}

void Backend::updateFileInfo(const QString &path, const QString &typeLabel) {
    const QFileInfo info(path);
    m_sourcePath = info.absoluteFilePath();
    m_displayName = info.completeBaseName();
    m_fileName = info.fileName();
    m_fileType = typeLabel;
    m_fileSize = QLocale().formattedDataSize(info.size(), 1,
                                             QLocale::DataSizeTraditionalFormat);
    emit fileInfoChanged();
}

QVariantMap Backend::windowGeometry() const {
    QSettings settings;
    settings.beginGroup(QString::fromLatin1(geometryGroup));
    QVariantMap result;
    result.insert(QStringLiteral("valid"), settings.contains(QStringLiteral("width")));
    result.insert(QStringLiteral("x"), settings.value(QStringLiteral("x"), 80));
    result.insert(QStringLiteral("y"), settings.value(QStringLiteral("y"), 80));
    result.insert(QStringLiteral("width"), settings.value(QStringLiteral("width"), 1100));
    result.insert(QStringLiteral("height"), settings.value(QStringLiteral("height"), 720));
    result.insert(QStringLiteral("maximized"), settings.value(QStringLiteral("maximized"), false));
    settings.endGroup();
    return result;
}

void Backend::saveWindowGeometry(int x, int y, int width, int height,
                                 bool maximized) {
    QSettings settings;
    settings.beginGroup(QString::fromLatin1(geometryGroup));
    settings.setValue(QStringLiteral("x"), x);
    settings.setValue(QStringLiteral("y"), y);
    settings.setValue(QStringLiteral("width"), width);
    settings.setValue(QStringLiteral("height"), height);
    settings.setValue(QStringLiteral("maximized"), maximized);
    settings.endGroup();
}
