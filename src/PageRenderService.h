// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <QImage>
#include <QMutex>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QThread>
#include <QVector>
#include <QWaitCondition>

#include <deque>

// Dedicated render thread. Owns the MuPDF document so the GUI thread never
// blocks on parsing or rasterizing. Urgent requests (the pages on screen) are
// served newest-first; prefetch requests (neighbouring pages) run in order.
class PageRenderService : public QThread
{
    Q_OBJECT
public:
    explicit PageRenderService(QObject *parent = nullptr);
    ~PageRenderService() override;

    void openDocument(int generation, const QString &path);
    void request(int generation, int page, QSize px, bool urgent);
    void cancelPending();

signals:
    void opened(int generation, int pageCount, QVector<QSizeF> sizes, QString title, QString author, QString sha1);
    void openFailed(int generation, QString error);
    void rendered(int generation, int page, QSize px, QImage image);

protected:
    void run() override;

private:
    struct Job {
        int gen = 0;
        int page = 0;
        QSize px;
    };

    QMutex m_mutex;
    QWaitCondition m_cond;
    bool m_quit = false;
    bool m_openPending = false;
    int m_openGen = 0;
    QString m_openPath;
    std::deque<Job> m_urgent;
    std::deque<Job> m_prefetch;
};
