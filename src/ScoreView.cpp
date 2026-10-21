// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ScoreView.h"

#include <QQuickWindow>
#include <QSGNode>
#include <QSGSimpleRectNode>
#include <QSGSimpleTextureNode>
#include <QtMath>

namespace {
constexpr qreal kMargin = 8.0;
constexpr int kCacheKb = 320 * 1024; // ~320 MB of rendered pages
}

ScoreView::ScoreView(QQuickItem *parent)
    : QQuickItem(parent)
    , m_service(new PageRenderService(this))
{
    setFlag(ItemHasContents, true);
    m_cache.setMaxCost(kCacheKb);
    connect(m_service, &PageRenderService::opened, this, &ScoreView::onOpened);
    connect(m_service, &PageRenderService::openFailed, this, &ScoreView::onOpenFailed);
    connect(m_service, &PageRenderService::rendered, this, &ScoreView::onRendered);
    m_service->start();
}

ScoreView::~ScoreView()
{
    // Join the render thread before this object (the signal receiver) goes away.
    delete m_service;
    m_service = nullptr;
}

qreal ScoreView::dpr() const
{
    return window() ? window()->effectiveDevicePixelRatio() : 1.0;
}

QSize ScoreView::pxFor(const QRectF &r) const
{
    const qreal d = dpr();
    return QSize(qMax(1, qRound(r.width() * d)), qMax(1, qRound(r.height() * d)));
}

quint64 ScoreView::key(int page, QSize px)
{
    return (quint64(page) << 42) | (quint64(px.width() & 0x1FFFFF) << 21) | quint64(px.height() & 0x1FFFFF);
}

void ScoreView::setSource(const QString &path)
{
    if (path == m_source)
        return;
    m_source = path;
    ++m_gen;
    m_sizes.clear();
    m_slots.clear();
    m_cache.clear();
    m_inflight.clear();
    m_current = 0;
    m_title.clear();
    m_author.clear();
    m_sha1.clear();
    m_error.clear();
    m_loading = !path.isEmpty();
    m_dirty = true;
    if (!path.isEmpty())
        m_service->openDocument(m_gen, path);
    emit sourceChanged();
    emit documentChanged();
    emit layoutChanged();
    update();
}

void ScoreView::onOpened(int gen, int, QVector<QSizeF> sizes, QString title, QString author, QString sha1)
{
    if (gen != m_gen)
        return;
    m_sizes = std::move(sizes);
    m_title = title;
    m_author = author;
    m_sha1 = sha1;
    m_loading = false;
    m_current = 0;
    emit documentChanged();
    emit documentOpened(); // QML may call goToPage() here to restore the last position
    relayout();
}

void ScoreView::onOpenFailed(int gen, QString error)
{
    if (gen != m_gen)
        return;
    m_loading = false;
    m_error = error;
    emit documentChanged();
}

void ScoreView::setPagesPerView(int n)
{
    n = qBound(1, n, 2);
    if (n == m_perView)
        return;
    const int page = m_current;
    m_perView = n;
    m_current = page - page % n;
    emit pagesPerViewChanged();
    emit currentPageChanged();
    relayout();
}

void ScoreView::goToPage(int page)
{
    if (m_sizes.isEmpty())
        return;
    page = qBound(0, page, int(m_sizes.size()) - 1);
    page -= page % m_perView;
    if (page == m_current)
        return;
    m_current = page;
    emit currentPageChanged();
    relayout();
}

void ScoreView::next()
{
    if (m_current + m_perView < m_sizes.size())
        goToPage(m_current + m_perView);
}

void ScoreView::prev()
{
    goToPage(m_current - m_perView);
}

void ScoreView::first()
{
    goToPage(0);
}

void ScoreView::last()
{
    goToPage(int(m_sizes.size()) - 1);
}

const ScoreView::Slot *ScoreView::slotAt(const QPointF &p) const
{
    for (const Slot &s : m_slots) {
        if (s.rect.contains(p))
            return &s;
    }
    return nullptr;
}

QVector<ScoreView::Slot> ScoreView::layoutFor(int firstPage) const
{
    QVector<Slot> out;
    if (m_sizes.isEmpty() || width() <= 0 || height() <= 0)
        return out;
    const int n = m_perView;
    const qreal d = dpr();
    const qreal slotW = (width() - kMargin * (n + 1)) / n;
    const qreal slotH = height() - 2 * kMargin;
    if (slotW <= 0 || slotH <= 0)
        return out;
    for (int i = 0; i < n; ++i) {
        const int p = firstPage + i;
        if (p >= m_sizes.size())
            break;
        const QSizeF ps = m_sizes[p];
        const qreal sc = qMin(slotW / ps.width(), slotH / ps.height());
        // Snap to whole device pixels so the texture maps 1:1 onto the screen.
        const qreal w = qMax(1.0, qRound(ps.width() * sc * d)) / d;
        const qreal h = qMax(1.0, qRound(ps.height() * sc * d)) / d;
        const qreal x = kMargin + i * (slotW + kMargin) + (slotW - w) / 2;
        const qreal y = kMargin + (slotH - h) / 2;
        out.push_back({p, QRectF(qRound(x * d) / d, qRound(y * d) / d, w, h)});
    }
    return out;
}

void ScoreView::ensure(const Slot &s, bool urgent)
{
    const QSize px = pxFor(s.rect);
    const quint64 k = key(s.page, px);
    if (m_cache.contains(k) || m_inflight.contains(k))
        return;
    m_inflight.insert(k);
    m_service->request(m_gen, s.page, px, urgent);
}

void ScoreView::relayout()
{
    m_service->cancelPending();
    m_inflight.clear();
    m_slots = layoutFor(m_current);
    for (const Slot &s : std::as_const(m_slots))
        ensure(s, true);
    prefetch();
    m_dirty = true;
    emit layoutChanged();
    update();
}

void ScoreView::prefetch()
{
    // Next view first (the common direction), then previous, then one more ahead.
    static const int order[] = {1, -1, 2, -2};
    for (int dv : order) {
        const int first = m_current + dv * m_perView;
        if (first < 0 || first >= m_sizes.size())
            continue;
        for (const Slot &s : layoutFor(first))
            ensure(s, false);
    }
}

void ScoreView::onRendered(int gen, int page, QSize px, QImage image)
{
    if (gen != m_gen)
        return;
    const quint64 k = key(page, px);
    m_inflight.remove(k);
    const int cost = qMax(1, int(image.sizeInBytes() / 1024));
    m_cache.insert(k, new QImage(std::move(image)), cost);
    for (const Slot &s : std::as_const(m_slots)) {
        if (s.page == page && pxFor(s.rect) == px) {
            m_dirty = true;
            update();
            break;
        }
    }
}

void ScoreView::geometryChange(const QRectF &n, const QRectF &o)
{
    QQuickItem::geometryChange(n, o);
    if (n.size() != o.size())
        relayout();
}

void ScoreView::itemChange(ItemChange change, const ItemChangeData &value)
{
    QQuickItem::itemChange(change, value);
    if (change == ItemSceneChange && value.window)
        relayout(); // device pixel ratio is known now
}

QSGNode *ScoreView::updatePaintNode(QSGNode *old, UpdatePaintNodeData *)
{
    QSGNode *root = old ? old : new QSGNode;
    if (!m_dirty && old)
        return root;

    while (QSGNode *c = root->firstChild()) {
        root->removeChildNode(c);
        delete c;
    }
    for (const Slot &s : std::as_const(m_slots)) {
        root->appendChildNode(new QSGSimpleRectNode(s.rect, QColor(0xfb, 0xfa, 0xf6)));
        if (QImage *img = m_cache.object(key(s.page, pxFor(s.rect)))) {
            auto *tex = new QSGSimpleTextureNode;
            tex->setRect(s.rect);
            tex->setTexture(window()->createTextureFromImage(*img, QQuickWindow::TextureIsOpaque));
            tex->setOwnsTexture(true);
            tex->setFiltering(QSGTexture::Linear);
            root->appendChildNode(tex);
        }
    }
    m_dirty = false;
    return root;
}
