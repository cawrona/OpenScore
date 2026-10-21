// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PageRenderService.h"

#include "PdfDocument.h"
#include "Util.h"

#include <QFileInfo>
#include <QMutexLocker>

PageRenderService::PageRenderService(QObject *parent)
    : QThread(parent)
{
}

PageRenderService::~PageRenderService()
{
    {
        QMutexLocker l(&m_mutex);
        m_quit = true;
    }
    m_cond.wakeAll();
    wait();
}

void PageRenderService::openDocument(int generation, const QString &path)
{
    {
        QMutexLocker l(&m_mutex);
        m_urgent.clear();
        m_prefetch.clear();
        m_openPending = true;
        m_openGen = generation;
        m_openPath = path;
    }
    m_cond.wakeOne();
}

void PageRenderService::request(int generation, int page, QSize px, bool urgent)
{
    {
        QMutexLocker l(&m_mutex);
        (urgent ? m_urgent : m_prefetch).push_back({generation, page, px});
    }
    m_cond.wakeOne();
}

void PageRenderService::cancelPending()
{
    QMutexLocker l(&m_mutex);
    m_urgent.clear();
    m_prefetch.clear();
}

void PageRenderService::run()
{
    PdfDocument doc;
    int docGen = -1;

    for (;;) {
        Job job;
        bool doOpen = false;
        QString openPath;
        int openGen = 0;
        {
            QMutexLocker l(&m_mutex);
            while (!m_quit && !m_openPending && m_urgent.empty() && m_prefetch.empty())
                m_cond.wait(&m_mutex);
            if (m_quit)
                return;
            if (m_openPending) {
                doOpen = true;
                openPath = m_openPath;
                openGen = m_openGen;
                m_openPending = false;
            } else if (!m_urgent.empty()) {
                job = m_urgent.back(); // newest first: the latest page turn wins
                m_urgent.pop_back();
            } else {
                job = m_prefetch.front();
                m_prefetch.pop_front();
            }
        }

        if (doOpen) {
            docGen = -1;
            if (doc.open(openPath)) {
                docGen = openGen;
                emit opened(openGen, doc.pageCount(), doc.pageSizes(), doc.metadata("info:Title"),
                            doc.metadata("info:Author"), Util::fileSha1(openPath));
            } else {
                emit openFailed(openGen, QStringLiteral("Could not open %1").arg(QFileInfo(openPath).fileName()));
            }
            continue;
        }

        if (job.gen != docGen)
            continue;
        const QImage img = doc.render(job.page, job.px);
        if (!img.isNull())
            emit rendered(job.gen, job.page, job.px, img);
    }
}
