#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QTimer>

#include <memory>

#include "backend.h"
#include "systemtheme.h"

namespace {
QString previewSocketPath() {
  QString directory =
      QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
  if (directory.isEmpty())
    directory = QDir::tempPath();
  return QDir(directory).filePath(QStringLiteral("oma3dviewer-preview.sock"));
}

QByteArray previewCommand(const QString &command, const QString &path = {}) {
  QJsonObject object{{QStringLiteral("command"), command}};
  if (!path.isEmpty())
    object.insert(QStringLiteral("path"), path);
  return QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
}

bool sendPreviewCommand(const QString &command, const QString &path = {}) {
  QLocalSocket socket;
  socket.connectToServer(previewSocketPath());
  if (!socket.waitForConnected(350))
    return false;

  const QByteArray payload = previewCommand(command, path);
  if (socket.write(payload) != payload.size())
    return false;
  if (socket.bytesToWrite() > 0 && !socket.waitForBytesWritten(500))
    return false;

  socket.disconnectFromServer();
  if (socket.state() != QLocalSocket::UnconnectedState)
    socket.waitForDisconnected(200);
  return true;
}
} // namespace

int main(int argc, char *argv[]) {
  QGuiApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("oma3dviewer"));
  app.setApplicationDisplayName(QStringLiteral("oma3dviewer"));
  app.setApplicationVersion(QStringLiteral("0.1.0"));
  app.setOrganizationName(QStringLiteral("nicopellerin"));
  app.setOrganizationDomain(QStringLiteral("nicopellerin.io"));
  app.setDesktopFileName(QStringLiteral("oma3dviewer"));
  app.setWindowIcon(QIcon::fromTheme(QStringLiteral("oma3dviewer")));

  const int regularFontId = QFontDatabase::addApplicationFont(
      QStringLiteral(":/fonts/JetBrainsMonoNerdFont-Regular.ttf"));
  const int boldFontId = QFontDatabase::addApplicationFont(
      QStringLiteral(":/fonts/JetBrainsMonoNerdFont-Bold.ttf"));
  const int extraBoldFontId = QFontDatabase::addApplicationFont(
      QStringLiteral(":/fonts/JetBrainsMonoNerdFont-ExtraBold.ttf"));
  if (regularFontId >= 0 && boldFontId >= 0 && extraBoldFontId >= 0) {
    const QStringList families =
        QFontDatabase::applicationFontFamilies(regularFontId);
    if (!families.isEmpty()) {
      QFont applicationFont = app.font();
      applicationFont.setFamily(families.constFirst());
      applicationFont.setStyleHint(QFont::Monospace);
      app.setFont(applicationFont);
    }
  } else {
    qWarning() << "Could not load the bundled JetBrains Mono Nerd Font.";
  }

  QQuickStyle::setStyle(QStringLiteral("Material"));

  QCommandLineParser parser;
  parser.setApplicationDescription(
      QStringLiteral("A focused 3D model viewer for Omarchy."));
  parser.addHelpOption();
  parser.addVersionOption();
  QCommandLineOption previewOption(
      QStringList{QStringLiteral("preview")},
      QStringLiteral("Show the model in the compact live preview window."));
  QCommandLineOption previewCloseOption(
      QStringList{QStringLiteral("preview-close")},
      QStringLiteral("Close the live preview window."));
  parser.addOption(previewOption);
  parser.addOption(previewCloseOption);
  parser.addPositionalArgument(
      QStringLiteral("file"),
      QStringLiteral("A GLB, glTF, OBJ, or FBX model to open."));
  parser.process(app);

  const bool previewMode = parser.isSet(previewOption);
  const QStringList positional = parser.positionalArguments();
  if (parser.isSet(previewCloseOption)) {
    sendPreviewCommand(QStringLiteral("close"));
    return 0;
  }

  if (previewMode && positional.isEmpty()) {
    qCritical() << "Preview mode requires an input model.";
    return -1;
  }

  const QString sourcePath = positional.isEmpty()
                                 ? QString{}
                                 : QFileInfo(positional.constFirst())
                                       .absoluteFilePath();
  if (previewMode &&
      sendPreviewCommand(QStringLiteral("open"), sourcePath)) {
    return 0;
  }

  Backend backend(previewMode);
  SystemTheme systemTheme;
  if (!sourcePath.isEmpty())
    backend.openPath(sourcePath);

  std::unique_ptr<QLocalServer> previewServer;
  if (previewMode) {
    const QString socketPath = previewSocketPath();
    QLocalServer::removeServer(socketPath);
    previewServer = std::make_unique<QLocalServer>();
    if (!previewServer->listen(socketPath)) {
      qCritical() << "Could not start the Oma3DViewer preview bridge:"
                  << previewServer->errorString();
      return -1;
    }

    const auto handlePayload = [&backend, &app](const QByteArray &payload) {
      QJsonParseError parseError;
      const QJsonDocument document =
          QJsonDocument::fromJson(payload, &parseError);
      if (parseError.error != QJsonParseError::NoError ||
          !document.isObject()) {
        qWarning() << "Ignored malformed preview command:"
                   << parseError.errorString();
        return;
      }

      const QJsonObject object = document.object();
      const QString command = object.value(QStringLiteral("command")).toString();
      if (command == QStringLiteral("close")) {
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
      } else if (command == QStringLiteral("open")) {
        backend.openPath(object.value(QStringLiteral("path")).toString());
      }
    };

    QObject::connect(previewServer.get(), &QLocalServer::newConnection, &app,
                     [&previewServer, handlePayload]() {
      while (previewServer->hasPendingConnections()) {
        QLocalSocket *socket = previewServer->nextPendingConnection();
        const auto consume = [socket, handlePayload]() {
          QByteArray buffer =
              socket->property("oma3dviewerPreviewBuffer").toByteArray();
          buffer.append(socket->readAll());

          qsizetype newline = -1;
          while ((newline = buffer.indexOf('\n')) >= 0) {
            const QByteArray line = buffer.left(newline);
            buffer.remove(0, newline + 1);
            if (!line.isEmpty())
              handlePayload(line);
          }
          socket->setProperty("oma3dviewerPreviewBuffer", buffer);
        };
        QObject::connect(socket, &QLocalSocket::readyRead, socket, consume);
        QObject::connect(socket, &QLocalSocket::disconnected, socket,
                         &QObject::deleteLater);
        QTimer::singleShot(0, socket, consume);
      }
    });

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &app,
                     [&previewServer, socketPath]() {
      previewServer->close();
      QLocalServer::removeServer(socketPath);
    });
  }

  QQmlApplicationEngine engine;
  QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app,
                   [](const QList<QQmlError> &warnings) {
                     for (const QQmlError &warning : warnings)
                       qWarning().noquote() << warning.toString();
                   });
  engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
  engine.rootContext()->setContextProperty(QStringLiteral("systemTheme"),
                                           &systemTheme);
  engine.load(QUrl(previewMode ? QStringLiteral("qrc:/Preview.qml")
                               : QStringLiteral("qrc:/Main.qml")));

  if (engine.rootObjects().isEmpty()) {
    qCritical() << "Could not load the Oma3DViewer interface; resource available:"
                << QFile::exists(QStringLiteral(":/Main.qml"));
    return -1;
  }

  return app.exec();
}
