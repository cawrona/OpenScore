// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <QImage>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QVector>

struct fz_context;
struct fz_document;

// Thin MuPDF wrapper. Owns its own fz_context, so one instance may be used by
// one thread at a time (the render thread, or a library-scan worker).
class PdfDocument
{
public:
    PdfDocument();
    ~PdfDocument();
    PdfDocument(const PdfDocument &) = delete;
    PdfDocument &operator=(const PdfDocument &) = delete;

    bool open(const QString &path);
    void close();
    bool isOpen() const { return m_doc != nullptr; }

    int pageCount() const { return m_pages; }
    QVector<QSizeF> pageSizes() const; // in PDF points
    QString metadata(const char *key) const; // e.g. "info:Title", "info:Author"

    // Renders the whole page scaled to exactly `px`. Returns RGBA8888, opaque.
    QImage render(int page, QSize px) const;

private:
    fz_context *m_ctx = nullptr;
    fz_document *m_doc = nullptr;
    int m_pages = 0;
    QVector<QRectF> m_bounds;
};
