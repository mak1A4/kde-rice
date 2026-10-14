#pragma once

#include <KConfigWatcher>
#include <KSharedConfig>

#include <QObject>
#include <QQmlEngine>
#include <QQmlParserStatus>
#include <QVariant>

// One entry of a KConfig file, read and written from QML:
//
//     ConfigEntry { file: "kwinrc"; group: "KDERiceRoundCorners"; key: "Radius"; defaultValue: 5; kwin: true }
//
// `value` has the type of `defaultValue`, and follows changes other programs
// make to the file. Setting it back to the default removes the entry. With
// `kwin`, KWin is told to read its configuration again after each write.
class ConfigEntry : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    QML_ELEMENT

    Q_PROPERTY(QString file MEMBER m_file NOTIFY sourceChanged)
    Q_PROPERTY(QString group MEMBER m_group NOTIFY sourceChanged)
    Q_PROPERTY(QString key MEMBER m_key NOTIFY sourceChanged)
    Q_PROPERTY(QVariant defaultValue MEMBER m_default NOTIFY sourceChanged)
    Q_PROPERTY(bool kwin MEMBER m_kwin NOTIFY kwinChanged)
    Q_PROPERTY(QVariant value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(bool isDefault READ isDefault NOTIFY valueChanged)

public:
    explicit ConfigEntry(QObject *parent = nullptr);

    QVariant value() const;
    void setValue(const QVariant &value);
    bool isDefault() const;

    void classBegin() override
    {
    }
    void componentComplete() override;

Q_SIGNALS:
    void sourceChanged();
    void kwinChanged();
    void valueChanged();

private:
    void open();
    void read();
    KConfigGroup configGroup() const;

    QString m_file;
    QString m_group;
    QString m_key;
    QVariant m_default;
    bool m_kwin = false;
    QVariant m_value;
    bool m_complete = false;
    KSharedConfigPtr m_config;
    KConfigWatcher::Ptr m_watcher;
};
