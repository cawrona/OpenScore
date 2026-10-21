// SPDX-License-Identifier: AGPL-3.0-or-later
#include "InkLayer.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointingDevice>
#include <QQuickWindow>
#include <QtMath>

namespace {

void drawStroke(QPainter *p, const Stroke &s, const QRectF &r)
{
    if (s.pts.isEmpty())
        return;
    auto map = [&](const QPointF &n) { return QPointF(r.x() + n.x() * r.width(), r.y() + n.y() * r.height()); };
    const qreal base = qMax<qreal>(0.5, s.width * r.width());

    if (s.tool == Stroke::Highlighter) {
        QColor c = s.color;
        c.setAlpha(96);
        QPen pen(c, base, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin);
        p->setPen(pen);
        QPainterPath path(map(s.pts.first()));
        for (int i = 1; i < s.pts.size(); ++i)
            path.lineTo(map(s.pts[i]));
        if (s.pts.size() == 1)
            path.lineTo(map(s.pts.first()) + QPointF(0.1, 0));
        p->drawPath(path); // one path => no darker overlap where the stroke crosses itself
        return;
    }

    p->setBrush(Qt::NoBrush);
    if (s.pts.size() == 1) {
        p->setPen(Qt::NoPen);
        p->setBrush(s.color);
        const qreal d = base * (0.4 + 0.6 * s.pressure.value(0, 1.0f));
        p->drawEllipse(map(s.pts.first()), d / 2, d / 2);
        return;
    }
    QPen pen(s.color, base, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    for (int i = 1; i < s.pts.size(); ++i) {
        const qreal pr = 0.5 * (s.pressure.value(i - 1, 1.0f) + s.pressure.value(i, 1.0f));
        pen.setWidthF(base * (0.4 + 0.6 * pr));
        p->setPen(pen);
        p->drawLine(map(s.pts[i - 1]), map(s.pts[i]));
    }
}

bool isTouch(const QMouseEvent *ev)
{
    const QPointingDevice *d = ev->pointingDevice();
    return d && d->type() == QInputDevice::DeviceType::TouchScreen;
}

} // namespace

InkLayer::InkLayer(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
    setAntialiasing(true);
    setOpaquePainting(false);
}

void InkLayer::setStore(AnnotationStore *s)
{
    if (s == m_store)
        return;
    if (m_store)
        disconnect(m_store, nullptr, this, nullptr);
    m_store = s;
    if (m_store) {
        connect(m_store, &AnnotationStore::changed, this, [this](int) {
            if (!m_selfChange)
                invalidateAll();
        });
    }
    invalidateAll();
    emit storeChanged();
}

void InkLayer::setView(ScoreView *v)
{
    if (v == m_view)
        return;
    if (m_view)
        disconnect(m_view, nullptr, this, nullptr);
    m_view = v;
    if (m_view)
        connect(m_view, &ScoreView::layoutChanged, this, &InkLayer::invalidateAll);
    invalidateAll();
    emit viewChanged();
}

void InkLayer::setTool(const QString &t)
{
    if (t == m_tool)
        return;
    m_tool = t;
    emit toolChanged();
}

void InkLayer::setColor(const QColor &c)
{
    if (c == m_color)
        return;
    m_color = c;
    emit colorChanged();
}

void InkLayer::setThickness(qreal t)
{
    t = qBound<qreal>(1, t, 10);
    if (qFuzzyCompare(t, m_thickness))
        return;
    m_thickness = t;
    emit thicknessChanged();
}

void InkLayer::invalidateAll()
{
    m_cacheDirty = true;
    update();
}

void InkLayer::geometryChange(const QRectF &n, const QRectF &o)
{
    QQuickPaintedItem::geometryChange(n, o);
    invalidateAll();
}

qreal InkLayer::penWidth(Stroke::Tool t) const
{
    return t == Stroke::Highlighter ? m_thickness * 0.004 : m_thickness * 0.0007;
}

qreal InkLayer::eraserRadius() const
{
    return 0.008 + m_thickness * 0.0035;
}

const ScoreView::Slot *InkLayer::slotForPage(int page) const
{
    if (!m_view)
        return nullptr;
    for (const ScoreView::Slot &s : m_view->pageSlots()) {
        if (s.page == page)
            return &s;
    }
    return nullptr;
}

QPointF InkLayer::toNorm(const QPointF &pos) const
{
    return QPointF(qBound(0.0, (pos.x() - m_rect.x()) / m_rect.width(), 1.0),
                   qBound(0.0, (pos.y() - m_rect.y()) / m_rect.height(), 1.0));
}

void InkLayer::rebuildCache()
{
    const qreal dpr = window() ? window()->effectiveDevicePixelRatio() : 1.0;
    const QSize px(qMax(1, qCeil(width() * dpr)), qMax(1, qCeil(height() * dpr)));
    if (m_cache.size() != px || !qFuzzyCompare(m_cache.devicePixelRatio(), dpr)) {
        m_cache = QImage(px, QImage::Format_ARGB32_Premultiplied);
        m_cache.setDevicePixelRatio(dpr);
    }
    m_cache.fill(Qt::transparent);
    if (m_store && m_view) {
        QPainter p(&m_cache);
        p.setRenderHint(QPainter::Antialiasing);
        for (const ScoreView::Slot &slot : m_view->pageSlots()) {
            for (const Stroke &s : m_store->strokes(slot.page))
                drawStroke(&p, s, slot.rect);
        }
    }
    m_cacheDirty = false;
}

void InkLayer::paint(QPainter *painter)
{
    painter->setRenderHint(QPainter::Antialiasing);
    if (m_cacheDirty)
        rebuildCache();
    painter->drawImage(QPointF(0, 0), m_cache);
    if (m_mode == Mode::Draw) {
        drawStroke(painter, m_live, m_rect);
        if (m_hasPredicted && m_live.tool == Stroke::Pen && !m_live.pts.isEmpty()) {
            // Short extrapolated tail hides a frame or two of latency. Never stored.
            QPen pen(m_live.color, qMax<qreal>(0.5, m_live.width * m_rect.width()) * 0.7, Qt::SolidLine, Qt::RoundCap);
            painter->setPen(pen);
            const QPointF last = m_live.pts.last();
            painter->drawLine(QPointF(m_rect.x() + last.x() * m_rect.width(), m_rect.y() + last.y() * m_rect.height()), m_predicted);
        }
    }
}

void InkLayer::begin(const QPointF &pos, qreal pressure, bool eraserEnd)
{
    if (!m_view || !m_store)
        return;
    const ScoreView::Slot *slot = m_view->slotAt(pos);
    if (!slot)
        return;
    m_page = slot->page;
    m_rect = slot->rect;
    m_lastPos = pos;
    m_hasPredicted = false;

    if (eraserEnd || m_tool == QLatin1String("eraser")) {
        m_mode = Mode::Erase;
        m_store->beginErase();
        m_store->eraseAt(m_page, toNorm(pos), eraserRadius(), m_rect.height() / m_rect.width());
        return;
    }
    m_mode = Mode::Draw;
    m_live = Stroke();
    m_live.tool = m_tool == QLatin1String("highlighter") ? Stroke::Highlighter : Stroke::Pen;
    m_live.color = m_color;
    m_live.width = float(penWidth(m_live.tool));
    m_live.pts.push_back(toNorm(pos));
    m_live.pressure.push_back(float(qBound(0.15, pressure, 1.0)));
    update(QRect(QPoint(qFloor(pos.x()) - 12, qFloor(pos.y()) - 12), QSize(24, 24)));
}

void InkLayer::extend(const QPointF &pos, qreal pressure)
{
    if (m_mode == Mode::Erase) {
        m_store->eraseAt(m_page, toNorm(pos), eraserRadius(), m_rect.height() / m_rect.width());
        m_lastPos = pos;
        return;
    }
    if (m_mode != Mode::Draw)
        return;
    const QPointF d = pos - m_lastPos;
    if (d.x() * d.x() + d.y() * d.y() < 0.25)
        return; // < 0.5 px: not worth a point

    // Predict a little further along the current direction (up to ~1.5x the last step).
    m_predicted = pos + d * 1.5;
    m_hasPredicted = true;

    m_live.pts.push_back(toNorm(pos));
    m_live.pressure.push_back(float(qBound(0.15, pressure, 1.0)));

    const qreal pad = qMax<qreal>(8.0, m_live.width * m_rect.width() * 2 + 6);
    const QRectF dirty = QRectF(m_lastPos, pos).normalized().united(QRectF(pos, m_predicted).normalized()).adjusted(-pad, -pad, pad, pad);
    m_lastPos = pos;
    update(dirty.toAlignedRect());
}

void InkLayer::finish()
{
    if (m_mode == Mode::Erase) {
        m_store->endErase();
    } else if (m_mode == Mode::Draw && !m_live.pts.isEmpty()) {
        const Stroke done = m_live;
        m_selfChange = true; // draw it into the cache ourselves instead of rebuilding everything
        m_store->addStroke(m_page, done);
        m_selfChange = false;
        if (!m_cacheDirty) {
            QPainter p(&m_cache);
            p.setRenderHint(QPainter::Antialiasing);
            drawStroke(&p, done, m_rect);
        }
    }
    m_mode = Mode::None;
    m_live = Stroke();
    m_hasPredicted = false;
    update();
}

void InkLayer::mousePressEvent(QMouseEvent *ev)
{
    // Touch is for turning pages and gestures; only pen and mouse draw.
    if (isTouch(ev)) {
        ev->ignore();
        return;
    }
    const QEventPoint &pt = ev->points().first();
    const QPointingDevice *dev = ev->pointingDevice();
    const bool eraserEnd = dev && dev->pointerType() == QPointingDevice::PointerType::Eraser;
    begin(ev->position(), pt.pressure() > 0 ? pt.pressure() : 1.0, eraserEnd);
    ev->setAccepted(m_mode != Mode::None);
}

void InkLayer::mouseMoveEvent(QMouseEvent *ev)
{
    if (m_mode == Mode::None || isTouch(ev))
        return;
    const QEventPoint &pt = ev->points().first();
    extend(ev->position(), pt.pressure() > 0 ? pt.pressure() : 1.0);
}

void InkLayer::mouseReleaseEvent(QMouseEvent *ev)
{
    Q_UNUSED(ev);
    if (m_mode != Mode::None)
        finish();
}

void InkLayer::mouseUngrabEvent()
{
    if (m_mode != Mode::None)
        finish();
}
