#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QQmlParserStatus>

// A KWin plugin (Qt plugin directory kwin/plugins), switched on and off:
// `<id>Enabled` in the group Plugins of kwinrc keeps it for the next login,
// and KWin loads or unloads it right away.
class KWinPlugin : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    QML_ELEMENT

    Q_PROPERTY(QString pluginId MEMBER m_id NOTIFY pluginIdChanged)
    Q_PROPERTY(QString name READ name NOTIFY pluginIdChanged)
    Q_PROPERTY(QString description READ description NOTIFY pluginIdChanged)
    // the plugin's file is where KWin looks for it
    Q_PROPERTY(bool installed READ installed NOTIFY pluginIdChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    // KWin has it loaded now
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)

public:
    explicit KWinPlugin(QObject *parent = nullptr);

    QString name() const;
    QString description() const;
    bool installed() const;
    bool enabled() const;
    void setEnabled(bool enabled);
    bool loaded() const
    {
        return m_loaded;
    }

    Q_INVOKABLE void refresh();

    void classBegin() override
    {
    }
    void componentComplete() override;

Q_SIGNALS:
    void pluginIdChanged();
    void enabledChanged();
    void loadedChanged();

private:
    void setLoaded(bool loaded);

    QString m_id;
    bool m_loaded = false;
};
