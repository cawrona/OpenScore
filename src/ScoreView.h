// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "PageRenderService.h"

#include <QCache>
#include <QQuickItem>
#include <QSet>
#include <QtQml/qqmlregistration.h>

// Scene-graph item that shows one or two score pages. A page turn is a texture
// swap: neighbouring pages are rendered ahead of time on the render thread and
// kept in an in-memory cache keyed by (page, pixel size).
class ScoreView : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY documentChanged)
    Q_PROPERTY(int currentPage READ currentPage WRITE goToPage NOTIFY currentPageChanged)
    Q_PROPERTY(int pagesPerView READ pagesPerView WRITE setPagesPerView NOTIFY pagesPerViewChanged)
    Q_PROPERTY(QString title READ title NOTIFY documentChanged)
    Q_PROPERTY(QString author READ author NOTIFY documentChanged)
    Q_PROPERTY(QString sha1 READ sha1 NOTIFY documentChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY documentChanged)
    Q_PROPERTY(QString error READ error NOTIFY documentChanged)

public:
    struct Slot {
        int page = 0;
        QRectF rect; // item coordinates
    };

    explicit ScoreView(QQuickItem *parent = nullptr);
    ~ScoreView() override;

    QString source() const { return m_source; }
    void setSource(const QString &path);
    int pageCount() const { return m_sizes.size(); }
    int currentPage() const { return m_current; }
    int pagesPerView() const { return m_perView; }
    void setPagesPerView(int n);
    QString title() const { return m_title; }
    QString author() const { return m_author; }
    QString sha1() const { return m_sha1; }
    bool loading() const { return m_loading; }
    QString error() const { return m_error; }

    const QVector<Slot> &slots() const { return m_slots; }
    const Slot *slotAt(const QPointF &p) const;

    Q_INVOKABLE void goToPage(int page);
    Q_INVOKABLE void next();
    Q_INVOKABLE void prev();
    Q_INVOKABLE void first();
    Q_INVOKABLE void last();

signals:
    void sourceChanged();
    void documentChanged();
    void documentOpened();
    void currentPageChanged();
    void pagesPerViewChanged();
    void layoutChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void itemChange(ItemChange change, const ItemChangeData &value) override;

private:
    qreal dpr() const;
    QSize pxFor(const QRectF &r) const;
    static quint64 key(int page, QSize px);
    QVector<Slot> layoutFor(int firstPage) const;
    void relayout();
    void prefetch();
    void ensure(const Slot &s, bool urgent);
    void onOpened(int gen, int pageCount, QVector<QSizeF> sizes, QString title, QString author, QString sha1);
    void onOpenFailed(int gen, QString error);
    void onRendered(int gen, int page, QSize px, QImage image);

    PageRenderService *m_service = nullptr;
    QString m_source;
    QString m_title, m_author, m_sha1, m_error;
    bool m_loading = false;
    int m_gen = 0;
    int m_current = 0; // first page of the visible view
    int m_perView = 1;
    QVector<QSizeF> m_sizes;
    QVector<Slot> m_slots;
    QCache<quint64, QImage> m_cache;
    QSet<quint64> m_inflight;
    bool m_dirty = true;
};
