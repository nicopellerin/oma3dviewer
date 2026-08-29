#pragma once

#include <QByteArray>
#include <QColor>
#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>

class SystemTheme final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool darkMode READ darkMode NOTIFY themeChanged)
    Q_PROPERTY(QColor pageColor READ pageColor NOTIFY themeChanged)
    Q_PROPERTY(QColor stageColor READ stageColor NOTIFY themeChanged)
    Q_PROPERTY(QColor inkColor READ inkColor NOTIFY themeChanged)
    Q_PROPERTY(QColor mutedColor READ mutedColor NOTIFY themeChanged)
    Q_PROPERTY(QColor accentColor READ accentColor NOTIFY themeChanged)
    Q_PROPERTY(QColor errorColor READ errorColor NOTIFY themeChanged)

public:
    explicit SystemTheme(QObject *parent = nullptr);

    bool darkMode() const { return m_darkMode; }
    QColor pageColor() const { return m_pageColor; }
    QColor stageColor() const { return m_stageColor; }
    QColor inkColor() const { return m_inkColor; }
    QColor mutedColor() const { return m_mutedColor; }
    QColor accentColor() const { return m_accentColor; }
    QColor errorColor() const { return m_errorColor; }

    Q_INVOKABLE QColor mix(const QColor &base, const QColor &tint,
                           qreal amount) const;

signals:
    void themeChanged();

private:
    void refresh();
    void reload();
    void watchThemePaths();

    QByteArray m_lastContents;
    bool m_darkMode = true;
    QColor m_pageColor = QColor(QStringLiteral("#1a1b26"));
    QColor m_stageColor = QColor(QStringLiteral("#13141c"));
    QColor m_inkColor = QColor(QStringLiteral("#a9b1d6"));
    QColor m_mutedColor = QColor(QStringLiteral("#565f89"));
    QColor m_accentColor = QColor(QStringLiteral("#7aa2f7"));
    QColor m_errorColor = QColor(QStringLiteral("#f7768e"));
    QFileSystemWatcher m_watcher;
    QTimer m_pollTimer;
};
