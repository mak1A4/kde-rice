#include "configentry.h"
#include "kwin.h"

ConfigEntry::ConfigEntry(QObject *parent)
    : QObject(parent)
{
    connect(this, &ConfigEntry::sourceChanged, this, [this] {
        if (m_complete) {
            open();
        }
    });
}

void ConfigEntry::componentComplete()
{
    m_complete = true;
    open();
}

void ConfigEntry::open()
{
    if (m_file.isEmpty() || m_group.isEmpty() || m_key.isEmpty()) {
        return;
    }
    if (!m_config || m_config->name() != m_file) {
        m_config = KSharedConfig::openConfig(m_file, KConfig::NoGlobals);
        m_watcher = KConfigWatcher::create(m_config);
        connect(m_watcher.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group) {
            if (group.name() == m_group) {
                read();
            }
        });
    }
    read();
}

KConfigGroup ConfigEntry::configGroup() const
{
    return KConfigGroup(m_config, m_group);
}

void ConfigEntry::read()
{
    // also what others wrote without telling (kwriteconfig6 without --notify)
    m_config->reparseConfiguration();
    const QVariant value = configGroup().readEntry(m_key.toUtf8().constData(), m_default);
    if (value != m_value) {
        m_value = value;
        Q_EMIT valueChanged();
    }
}

QVariant ConfigEntry::value() const
{
    return m_value.isValid() ? m_value : m_default;
}

bool ConfigEntry::isDefault() const
{
    return value() == m_default;
}

void ConfigEntry::setValue(const QVariant &value)
{
    if (!m_config || value == m_value) {
        return;
    }
    KConfigGroup group = configGroup();
    if (value == m_default) {
        group.deleteEntry(m_key, KConfig::Notify);
    } else {
        group.writeEntry(m_key, value, KConfig::Notify);
    }
    m_config->sync();
    m_value = value;
    Q_EMIT valueChanged();
    if (m_kwin) {
        KWin::reconfigure();
    }
}
