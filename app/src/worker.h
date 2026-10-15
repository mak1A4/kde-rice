#pragma once

#include <QFutureWatcher>
#include <QObject>
#include <QThreadPool>
#include <QtConcurrentRun>

#include <type_traits>
#include <utility>

// Work done off the GUI thread, one job after the other in the order they
// were given, with `busy` while any is waiting or running. `then` runs on
// the GUI thread with the job's result once it is done.
class Worker : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit Worker(QObject *parent = nullptr)
        : QObject(parent)
    {
        m_pool.setMaxThreadCount(1);
    }

    ~Worker() override
    {
        // jobs not begun are dropped; the one running is waited for
        m_pool.clear();
        m_pool.waitForDone();
    }

    bool busy() const
    {
        return m_pending > 0;
    }

    template<typename Work, typename Then>
    void run(Work work, Then then)
    {
        using Result = std::invoke_result_t<Work>;
        if (++m_pending == 1) {
            Q_EMIT busyChanged();
        }
        auto watcher = new QFutureWatcher<Result>(this);
        connect(watcher, &QFutureWatcher<Result>::finished, this, [this, watcher, then = std::move(then)] {
            watcher->deleteLater();
            if (!watcher->isCanceled()) {
                if constexpr (std::is_void_v<Result>) {
                    then();
                } else {
                    then(watcher->result());
                }
            }
            if (--m_pending == 0) {
                Q_EMIT busyChanged();
            }
        });
        watcher->setFuture(QtConcurrent::run(&m_pool, std::move(work)));
    }

Q_SIGNALS:
    void busyChanged();

private:
    QThreadPool m_pool;
    int m_pending = 0;
};
