#include "detailspanel.h"

#include "guiutil.h"
#include "thumbnailloader.h"

#include <QEvent>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace sheaf {
namespace gui {

DetailsPanel::DetailsPanel(ThumbnailLoader *loader, QWidget *parent)
    : QWidget(parent)
    , m_loader(loader)
{
    setAccessibleName(tr("File details"));
    setMinimumWidth(260);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    m_preview = new QLabel(this);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setMinimumHeight(220);
    m_preview->setAccessibleName(tr("Preview"));
    layout->addWidget(m_preview);

    m_title = new QLabel(this);
    m_title->setWordWrap(true);
    m_title->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont bold = m_title->font();
    bold.setBold(true);
    bold.setPointSizeF(bold.pointSizeF() * 1.15);
    m_title->setFont(bold);
    layout->addWidget(m_title);

    m_form = new QFormLayout;
    m_form->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_form->setHorizontalSpacing(12);
    m_form->setVerticalSpacing(6);
    m_type = addRow(tr("Type"));
    m_size = addRow(tr("Size"));
    m_modified = addRow(tr("Modified"));
    m_used = addRow(tr("Last used"));
    m_opens = addRow(tr("Opened here"));
    m_progress = addRow(tr("Read"));
    m_tags = addRow(tr("Tags"));
    m_location = addRow(tr("Location"));
    layout->addLayout(m_form);
    layout->addStretch(1);

    m_hint = new QLabel(tr("Right click a file for more options"), this);
    m_hint->setWordWrap(true);
    QColor dim = palette().color(QPalette::WindowText);
    dim.setAlpha(150);
    QPalette hintPalette = m_hint->palette();
    hintPalette.setColor(QPalette::WindowText, dim);
    m_hint->setPalette(hintPalette);
    layout->addWidget(m_hint);
    connect(m_loader, &ThumbnailLoader::thumbnailReady, this, [this]() { refresh(); });
    refresh();
}

QLabel *DetailsPanel::addRow(const QString &caption)
{
    auto *value = new QLabel(this);
    value->setWordWrap(true);
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *name = new QLabel(caption, this);
    QColor dim = palette().color(QPalette::WindowText);
    dim.setAlpha(170);
    QPalette pal = name->palette();
    pal.setColor(QPalette::WindowText, dim);
    name->setPalette(pal);
    name->setBuddy(value);
    m_form->addRow(name, value);
    return value;
}

void DetailsPanel::setDocuments(const QList<DocumentInfo> &docs)
{
    m_docs = docs;
    refresh();
}

void DetailsPanel::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange) {
        refresh();
    }
}

void DetailsPanel::refresh()
{
    const bool single = m_docs.size() == 1;
    const bool any = !m_docs.isEmpty();

    for (QLabel *label : {m_type, m_size, m_modified, m_used, m_opens, m_progress, m_tags, m_location}) {
        label->setVisible(single);
        if (QWidget *caption = m_form->labelForField(label)) {
            caption->setVisible(single);
        }
    }
    m_preview->setVisible(single);
    m_hint->setVisible(any);

    if (!any) {
        m_title->setText(tr("Nothing selected"));
        return;
    }
    if (!single) {
        m_title->setText(tr("%n file(s) selected", nullptr, m_docs.size()));
        return;
    }

    const DocumentInfo &doc = m_docs.first();
    m_title->setText(doc.displayName());

    QPixmap pix = m_loader->thumbnail(doc);
    const qreal ratio = devicePixelRatioF();
    if (pix.isNull()) {
        pix = ThumbnailLoader::placeholder(doc, QSize(150, 195), ratio, palette());
    } else {
        pix = pix.scaled(QSize(220, 220) * ratio, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        pix.setDevicePixelRatio(ratio);
    }
    m_preview->setPixmap(pix);

    m_type->setText(doc.mimeComment.isEmpty() ? doc.mime : doc.mimeComment);
    m_size->setText(doc.exists ? humanSize(doc.size) : tr("Not found, the file was moved or deleted"));
    m_modified->setText(doc.exists ? relativeTime(doc.modified) : tr("Unknown"));
    m_used->setText(relativeTime(doc.lastUsed));
    m_opens->setText(doc.openCount > 0 ? QString::number(doc.openCount) : tr("Never"));
    m_progress->setText(doc.progress >= 0.0 ? tr("%1 percent").arg(qRound(doc.progress * 100)) : QString());
    if (QWidget *caption = m_form->labelForField(m_progress)) {
        caption->setVisible(doc.progress >= 0.0);
    }
    m_progress->setVisible(doc.progress >= 0.0);
    m_tags->setText(doc.tags.isEmpty() ? tr("None") : doc.tags.join(QStringLiteral(", ")));
    m_location->setText(doc.folder());
}

} // namespace gui
} // namespace sheaf
