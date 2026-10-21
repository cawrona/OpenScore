// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "AnnotationStore.h"
#include "ScoreView.h"

#include <QImage>
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

// Draws and edits ink on top of a ScoreView. Committed strokes live in a
// cached bitmap; only the stroke in progress (plus a short predicted tail) is
// painted per frame, and only its dirty rectangle is invalidated.
class InkLayer : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(AnnotationStore *store READ store WRITE setStore NOTIFY storeChanged)
    Q_PROPERTY(ScoreView *view READ view WRITE setView NOTIFY viewChanged)
    Q_PROPERTY(QString tool READ tool WRITE setTool NOTIFY toolChanged) // "pen" | "highlighter" | "eraser"
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(qreal thickness READ thickness WRITE setThickness NOTIFY thicknessChanged) // 1..10

public:
    explicit InkLayer(QQuickItem *parent = nullptr);

    AnnotationStore *store() const { return m_store; }
    void setStore(AnnotationStore *s);
    ScoreView *view() const { return m_view; }
    void setView(ScoreView *v);
    QString tool() const { return m_tool; }
    void setTool(const QString &t);
    QColor color() const { return m_color; }
    void setColor(const QColor &c);
    qreal thickness() const { return m_thickness; }
    void setThickness(qreal t);

    void paint(QPainter *painter) override;

signals:
    void storeChanged();
    void viewChanged();
    void toolChanged();
    void colorChanged();
    void thicknessChanged();

protected:
    void mousePressEvent(QMouseEvent *ev) override;
    void mouseMoveEvent(QMouseEvent *ev) override;
    void mouseReleaseEvent(QMouseEvent *ev) override;
    void mouseUngrabEvent() override;
    void geometryChange(const QRectF &n, const QRectF &o) override;

private:
    enum class Mode { None, Draw, Erase };

    const ScoreView::Slot *slotForPage(int page) const;
    void rebuildCache();
    qreal penWidth(Stroke::Tool t) const;
    qreal eraserRadius() const;
    void begin(const QPointF &pos, qreal pressure, bool eraserEnd);
    void extend(const QPointF &pos, qreal pressure);
    void finish();
    QPointF toNorm(const QPointF &pos) const;
    void invalidateAll();

    AnnotationStore *m_store = nullptr;
    ScoreView *m_view = nullptr;
    QString m_tool = QStringLiteral("pen");
    QColor m_color = QColor(QStringLiteral("#d62828"));
    qreal m_thickness = 3;

    QImage m_cache;
    bool m_cacheDirty = true;

    Mode m_mode = Mode::None;
    int m_page = 0;
    QRectF m_rect; // slot rect of the page being edited
    Stroke m_live;
    QPointF m_lastPos;
    QPointF m_predicted;
    bool m_hasPredicted = false;
    bool m_selfChange = false;
};
