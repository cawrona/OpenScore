// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <QColor>
#include <QHash>
#include <QObject>
#include <QPair>
#include <QPointF>
#include <QTimer>
#include <QVector>
#include <QtQml/qqmlregistration.h>

struct Stroke {
    enum Tool { Pen = 0, Highlighter = 1 };
    Tool tool = Pen;
    QColor color = Qt::black;
    float width = 0.002f;     // as a fraction of the page width
    QVector<QPointF> pts;     // normalized to the page (0..1 in both axes)
    QVector<float> pressure;  // same length as pts
};

// Per-score ink, stored as JSON in the app data folder keyed by the PDF's
// content hash. The PDF itself is never modified.
class AnnotationStore : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)

public:
    explicit AnnotationStore(QObject *parent = nullptr);
    ~AnnotationStore() override;

    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }

    Q_INVOKABLE void load(const QString &sha1);
    Q_INVOKABLE void flush();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void clearPage(int page);

    const QVector<Stroke> &strokes(int page) const;
    void addStroke(int page, const Stroke &s);

    // Eraser: call beginErase(), eraseAt() for each sample, then endErase().
    // The whole gesture is one undo step. `radius` is in page-width units and
    // `aspect` is page height / page width.
    void beginErase();
    int eraseAt(int page, QPointF normalized, qreal radius, qreal aspect);
    void endErase();

signals:
    void changed(int page); // -1: everything
    void historyChanged();

private:
    struct Op {
        enum Kind { Add, Erase } kind = Add;
        int page = 0;
        QVector<QPair<int, Stroke>> items; // (index at time of change, stroke), in order of change
    };

    QString filePath() const;
    void markDirty();
    void applyUndo(const Op &op);
    void applyRedo(const Op &op);

    QString m_sha1;
    QHash<int, QVector<Stroke>> m_pages;
    QVector<Op> m_undo, m_redo;
    Op m_erase;
    bool m_erasing = false;
    bool m_dirty = false;
    QTimer m_saveTimer;
};
