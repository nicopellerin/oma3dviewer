#include "backend.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QLocale>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

namespace {
constexpr auto geometryGroup = "window";

void stopProcess(QProcess *&process) {
    if (!process)
        return;
    process->disconnect();
    if (process->state() != QProcess::NotRunning) {
        process->kill();
        process->waitForFinished(500);
    }
    delete process;
    process = nullptr;
}
}

Backend::Backend(bool blendPreviewEnabled, QObject *parent)
    : QObject(parent), m_blendPreviewEnabled(blendPreviewEnabled) {}

Backend::~Backend() {
    cancelModelStatistics();
    cancelConversion();
}

QStringList Backend::supportedExtensions() const {
    QStringList extensions{QStringLiteral("glb"), QStringLiteral("gltf"),
                           QStringLiteral("obj"), QStringLiteral("fbx")};
    if (m_blendPreviewEnabled)
        extensions.append(QStringLiteral("blend"));
    return extensions;
}

bool Backend::isSupportedExtension(const QString &suffix) const {
    return supportedExtensions().contains(suffix.toLower());
}

bool Backend::acceptsUrl(const QUrl &url) const {
    if (!url.isLocalFile())
        return false;
    return isSupportedExtension(QFileInfo(url.toLocalFile()).suffix());
}

void Backend::openFullViewer() {
    if (m_sourcePath.isEmpty())
        return;

    if (!QProcess::startDetached(QCoreApplication::applicationFilePath(),
                                 {m_sourcePath})) {
        setError(QStringLiteral("Could not open the full Oma3DViewer window."));
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
        QStringLiteral("/org/gnome/NautilusPreviewer/Oma3dviewerBridge"),
        QStringLiteral("io.nicopellerin.Oma3dviewerBridge"),
        QStringLiteral("Select"));
    message << QVariant::fromValue(static_cast<quint32>(direction));
    QDBusConnection::sessionBus().send(message);
}

void Backend::openFile(const QUrl &url) {
    if (!url.isLocalFile()) {
        setError(QStringLiteral("Oma3DViewer can only open local model files."));
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
        QStringList labels;
        for (const QString &extension : supportedExtensions())
            labels.append(extension.toUpper());
        const QString last = labels.takeLast();
        setError(QStringLiteral("Unsupported model format. %1 a %2, or %3 file.")
            .arg(m_blendPreviewEnabled ? QStringLiteral("Preview")
                                       : QStringLiteral("Open"),
                 labels.join(QStringLiteral(", ")), last));
        return;
    }

    cancelModelStatistics();
    cancelConversion();
    resetModelStatistics();
    clearError();
    setModelUrl({});
    m_canOpenFullViewer = suffix != QStringLiteral("blend");

    if (suffix == QStringLiteral("fbx")) {
        updateFileInfo(info.absoluteFilePath(), QStringLiteral("FBX"));
        startFbxConversion(info.absoluteFilePath());
        return;
    }

    if (suffix == QStringLiteral("blend")) {
        updateFileInfo(info.absoluteFilePath(), QStringLiteral("BLEND"));
        startBlendConversion(info.absoluteFilePath());
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

        static const QRegularExpression countPattern(
            QStringLiteral(R"(^\s*(Meshes|Vertices|Faces):\s*(\d+)\s*$)"),
            QRegularExpression::MultilineOption);
        static const QRegularExpression triangleType(
            QStringLiteral(R"(^\s*Primitive Types:\s*triangles\s*$)"),
            QRegularExpression::MultilineOption
                | QRegularExpression::CaseInsensitiveOption);

        QHash<QString, qulonglong> counts;
        auto matches = countPattern.globalMatch(output);
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            counts.insert(match.captured(1), match.captured(2).toULongLong());
        }
        if (counts.size() != 3 || !triangleType.match(output).hasMatch())
            return;

        m_meshCount = counts.value(QStringLiteral("Meshes"));
        m_vertexCount = counts.value(QStringLiteral("Vertices"));
        m_triangleCount = counts.value(QStringLiteral("Faces"));
        m_modelStatsAvailable = true;
        emit modelStatsChanged();
    });

    probe->start(assimp, {
        QStringLiteral("info"), sourcePath, QStringLiteral("--silent")
    });
}

void Backend::cancelModelStatistics() {
    stopProcess(m_statsProbe);
}

void Backend::resetModelStatistics() {
    m_modelStatsAvailable = false;
    m_meshCount = 0;
    m_vertexCount = 0;
    m_triangleCount = 0;
    emit modelStatsChanged();
}

QString Backend::createConversionOutput(const QString &formatLabel) {
    m_conversionDirectory = std::make_unique<QTemporaryDir>(
        QDir::tempPath() + QStringLiteral("/oma3dviewer-XXXXXX"));
    if (!m_conversionDirectory->isValid()) {
        setError(QStringLiteral(
            "Could not create a temporary directory for %1 conversion.")
            .arg(formatLabel));
        setStatusText(QStringLiteral("Conversion failed"));
        m_conversionDirectory.reset();
        return {};
    }
    return m_conversionDirectory->filePath(QStringLiteral("model.glb"));
}

void Backend::runConversion(const QString &program, const QStringList &arguments,
                            const QProcessEnvironment &environment,
                            const QString &formatLabel, const QString &toolName) {
    const QString outputPath =
        m_conversionDirectory->filePath(QStringLiteral("model.glb"));
    m_converter = new QProcess(this);
    m_converter->setProcessChannelMode(QProcess::MergedChannels);
    m_converter->setProcessEnvironment(environment);

    connect(m_converter, &QProcess::errorOccurred, this,
            [this, toolName](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            setBusy(false);
            setStatusText(QStringLiteral("Conversion failed"));
            setError(QStringLiteral("%1 could not be started.").arg(toolName));
        }
    });

    connect(m_converter,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this, outputPath, formatLabel, toolName](
                int exitCode, QProcess::ExitStatus exitStatus) {
        const QString processOutput = QString::fromUtf8(m_converter->readAll()).trimmed();
        setBusy(false);

        if (exitStatus == QProcess::NormalExit && exitCode == 0
            && QFileInfo::exists(outputPath)) {
            m_fileType = formatLabel + QStringLiteral(" · converted");
            emit fileInfoChanged();
            startModelStatistics(outputPath);
            setModelUrl(QUrl::fromLocalFile(outputPath));
            setStatusText(QStringLiteral("Loaded"));
        } else {
            setStatusText(QStringLiteral("Conversion failed"));
            const QString detail = processOutput.isEmpty()
                ? QStringLiteral("%1 could not convert this %2 file.")
                      .arg(toolName, formatLabel)
                : processOutput;
            setError(detail);
        }

        m_converter->deleteLater();
        m_converter = nullptr;
    });

    setBusy(true);
    setStatusText(QStringLiteral("Converting %1…").arg(formatLabel));
    m_converter->start(program, arguments);
}

void Backend::startFbxConversion(const QString &sourcePath) {
    const QString assimp = QStandardPaths::findExecutable(QStringLiteral("assimp"));
    if (assimp.isEmpty()) {
        setError(QStringLiteral("FBX support requires Assimp. Install it with: omarchy pkg add assimp"));
        setStatusText(QStringLiteral("FBX unavailable"));
        return;
    }

    const QString outputPath = createConversionOutput(QStringLiteral("FBX"));
    if (outputPath.isEmpty())
        return;

    runConversion(assimp,
                  {QStringLiteral("export"), sourcePath, outputPath,
                   QStringLiteral("-fglb2")},
                  QProcessEnvironment::systemEnvironment(),
                  QStringLiteral("FBX"), QStringLiteral("Assimp"));
}

void Backend::startBlendConversion(const QString &sourcePath) {
    const QString blender = QStandardPaths::findExecutable(QStringLiteral("blender"));
    if (blender.isEmpty()) {
        setError(QStringLiteral(
            "Blender previews require Blender. Install it with: omarchy pkg add blender"));
        setStatusText(QStringLiteral("BLEND preview unavailable"));
        return;
    }

    const QString outputPath = createConversionOutput(QStringLiteral("BLEND"));
    if (outputPath.isEmpty())
        return;

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    const QString blenderDirectory = QFileInfo(blender).absolutePath();
    const QString inheritedPath = environment.value(QStringLiteral("PATH"));
    environment.insert(QStringLiteral("PATH"), inheritedPath.isEmpty()
        ? blenderDirectory
        : blenderDirectory + QDir::listSeparator() + inheritedPath);
    environment.remove(QStringLiteral("PYTHONHOME"));
    environment.remove(QStringLiteral("PYTHONPATH"));
    environment.remove(QStringLiteral("PYTHONUSERBASE"));
    environment.remove(QStringLiteral("VIRTUAL_ENV"));
    // Distribution builds of Blender use the Python installed alongside
    // them. Pin that prefix so a Mise/pyenv Python earlier in PATH cannot
    // supply an incompatible standard library to Blender's embedded Python.
    QDir blenderPrefix(blenderDirectory);
    if (blenderPrefix.dirName() == QStringLiteral("bin")
        && blenderPrefix.cdUp()
        && QFileInfo(blenderPrefix.filePath(QStringLiteral("bin/python3")))
               .isExecutable()) {
        environment.insert(QStringLiteral("BLENDER_SYSTEM_PYTHON"),
                           blenderPrefix.absolutePath());
    }
    environment.insert(QStringLiteral("ALSOFT_DRIVERS"), QStringLiteral("null"));
    environment.insert(QStringLiteral("OMA3DVIEWER_BLEND_OUTPUT"), outputPath);

    const QString exportScript = QStringLiteral(
        "import bpy, os\n"
        "result = bpy.ops.export_scene.gltf("
            "filepath=os.environ['OMA3DVIEWER_BLEND_OUTPUT'], export_format='GLB')\n"
        "if 'FINISHED' not in result:\n"
        "    raise RuntimeError('glTF export failed: ' + repr(result))\n");

    runConversion(blender, {
        QStringLiteral("--background"),
        QStringLiteral("--factory-startup"),
        QStringLiteral("--disable-autoexec"),
        QStringLiteral("--offline-mode"),
        sourcePath,
        QStringLiteral("-noaudio"),
        QStringLiteral("--python-exit-code"), QStringLiteral("1"),
        QStringLiteral("--python-expr"), exportScript
    }, environment, QStringLiteral("BLEND"), QStringLiteral("Blender"));
}

void Backend::cancelConversion() {
    stopProcess(m_converter);
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
    result.insert(QStringLiteral("x"), settings.value(QStringLiteral("x")));
    result.insert(QStringLiteral("y"), settings.value(QStringLiteral("y")));
    result.insert(QStringLiteral("width"), settings.value(QStringLiteral("width")));
    result.insert(QStringLiteral("height"), settings.value(QStringLiteral("height")));
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
