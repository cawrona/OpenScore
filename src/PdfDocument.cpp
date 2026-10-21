// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PdfDocument.h"

#include <QFile>

extern "C" {
#include <mupdf/fitz.h>
}

PdfDocument::PdfDocument() = default;

PdfDocument::~PdfDocument()
{
    close();
}

void PdfDocument::close()
{
    if (m_ctx) {
        if (m_doc)
            fz_drop_document(m_ctx, m_doc);
        fz_drop_context(m_ctx);
    }
    m_doc = nullptr;
    m_ctx = nullptr;
    m_pages = 0;
    m_bounds.clear();
}

bool PdfDocument::open(const QString &path)
{
    close();
    m_ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    if (!m_ctx)
        return false;

    fz_try(m_ctx)
    {
        fz_register_document_handlers(m_ctx);
    }
    fz_catch(m_ctx)
    {
        close();
        return false;
    }

    const QByteArray p = QFile::encodeName(path);
    fz_try(m_ctx)
    {
        m_doc = fz_open_document(m_ctx, p.constData());
        m_pages = fz_count_pages(m_ctx, m_doc);
    }
    fz_catch(m_ctx)
    {
        close();
        return false;
    }

    // Page boxes up front: layout needs them, and for PDFs this only touches
    // the page tree, not page content, so it is fast even for big scores.
    m_bounds.fill(QRectF(0, 0, 612, 792), m_pages);
    for (int i = 0; i < m_pages; ++i) {
        fz_page *pg = nullptr;
        fz_var(pg);
        fz_try(m_ctx)
        {
            pg = fz_load_page(m_ctx, m_doc, i);
            const fz_rect r = fz_bound_page(m_ctx, pg);
            if (r.x1 > r.x0 && r.y1 > r.y0)
                m_bounds[i] = QRectF(r.x0, r.y0, r.x1 - r.x0, r.y1 - r.y0);
        }
        fz_always(m_ctx)
        {
            fz_drop_page(m_ctx, pg);
        }
        fz_catch(m_ctx)
        {
            // keep default size for unreadable pages
        }
    }
    return m_pages > 0;
}

QVector<QSizeF> PdfDocument::pageSizes() const
{
    QVector<QSizeF> out;
    out.reserve(m_bounds.size());
    for (const QRectF &b : m_bounds)
        out.push_back(b.size());
    return out;
}

QString PdfDocument::metadata(const char *key) const
{
    if (!m_doc)
        return {};
    char buf[512];
    buf[0] = 0;
    int n = -1;
    fz_var(n);
    fz_try(m_ctx)
    {
        n = fz_lookup_metadata(m_ctx, m_doc, key, buf, int(sizeof buf));
    }
    fz_catch(m_ctx)
    {
        n = -1;
    }
    return n > 0 ? QString::fromUtf8(buf).trimmed() : QString();
}

QImage PdfDocument::render(int page, QSize px) const
{
    if (!m_doc || page < 0 || page >= m_pages || px.isEmpty())
        return {};

    const QRectF b = m_bounds[page];
    QImage out(px, QImage::Format_RGBA8888);
    if (out.isNull())
        return {};

    // Map the page box onto (0,0)-(w,h) exactly, so the pixmap origin is 0,0
    // and we can render straight into the QImage's buffer with no copy.
    const fz_matrix ctm = fz_concat(fz_translate(float(-b.left()), float(-b.top())),
                                    fz_scale(float(px.width() / b.width()), float(px.height() / b.height())));

    fz_page *pg = nullptr;
    fz_pixmap *pix = nullptr;
    fz_device *dev = nullptr;
    bool ok = false;
    fz_var(pg);
    fz_var(pix);
    fz_var(dev);
    fz_var(ok);

    fz_try(m_ctx)
    {
        pg = fz_load_page(m_ctx, m_doc, page);
        pix = fz_new_pixmap_with_data(m_ctx, fz_device_rgb(m_ctx), px.width(), px.height(), nullptr, 1,
                                      int(out.bytesPerLine()), out.bits());
        fz_clear_pixmap_with_value(m_ctx, pix, 0xff);
        dev = fz_new_draw_device(m_ctx, fz_identity, pix);
        fz_run_page(m_ctx, pg, dev, ctm, nullptr);
        fz_close_device(m_ctx, dev);
        ok = true;
    }
    fz_always(m_ctx)
    {
        fz_drop_device(m_ctx, dev);
        fz_drop_pixmap(m_ctx, pix);
        fz_drop_page(m_ctx, pg);
    }
    fz_catch(m_ctx)
    {
        ok = false;
    }
    return ok ? out : QImage();
}
