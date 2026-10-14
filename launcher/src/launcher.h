#pragma once

#include <QKeySequence>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>

class QAction;
class QQuickWindow;
class SearchModel;

// The popup's window and when it is shown: by the global shortcut, by running
// kde-rice-launcher again, over D-Bus. Hidden, the window is unmapped and the
// process stays, with the runners loaded, so that opening is instant.
class Launcher : public QObject
{
    Q_OBJECT
    // no hyphens in D-Bus interface names
    Q_CLASSINFO("D-Bus Interface", "io.github.mak1a4.kderice.Launcher")
    QML_ELEMENT
    QML_UNCREATABLE("made by main()")

    Q_PROPERTY(bool shown READ isShown NOTIFY shownChanged)

public:
    // the shortcut it asks for, KRunner's
    static const QKeySequence defaultShortcut;

    explicit Launcher(SearchModel *results, QObject *parent = nullptr);
    ~Launcher() override;

    // The QML window. On Wayland it becomes a layer-shell surface over
    // everything; elsewhere (X11, or offscreen for testing) a frameless
    // window, with a warning.
    void setWindow(QQuickWindow *window);
    bool isShown() const;

    // the global shortcut, with Meta+Space as default; it only gets that if
    // nothing else has it
    void registerShortcut();
    // Meta+Space taken from whatever has it (KRunner) and given to this
    void takeShortcut();

    // Starts a program from the popup, for kde-rice's own entries. On Wayland
    // with an activation token from the popup's window, as KDE's runners start
    // applications, so that its window comes to the front.
    void startProgram(const QString &program, const QStringList &arguments, const QString &appId);

public Q_SLOTS:
    Q_SCRIPTABLE void toggle();
    Q_SCRIPTABLE void open();
    Q_SCRIPTABLE void close();
    // opens with `text` typed in
    Q_SCRIPTABLE void search(const QString &text);

Q_SIGNALS:
    void shownChanged();
    // the view sets its search field to this and takes the keyboard
    void opened(const QString &text);

private:
    void setShown(bool shown);

    SearchModel *const m_results;
    QPointer<QQuickWindow> m_window;
    QAction *m_shortcutAction = nullptr;
    bool m_layerShell = false;
    bool m_shown = false;
    // the window has had the keyboard since it was opened
    bool m_wasActive = false;
};
