#include "quickopen.h"

#include "carddelegate.h"
#include "thumbnailloader.h"

#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListView>
#include <QScreen>
#include <QVBoxLayout>

namespace sheaf {
namespace gui {

QuickOpen::QuickOpen(DocumentModel *model, ThumbnailLoader *loader, QWidget *parent)
    : QWidget(parent, Qt::Dialog | Qt::FramelessWindowHint)
{
    setWindowTitle(tr("Quick Open"));
    setAttribute(Qt::WA_DeleteOnClose, false);
    setAccessibleName(tr("Quick open"));
    resize(620, 440);

    m_proxy = new DocumentFilterProxy(this);
    m_proxy->setSourceModel(model);
    SortOptions sort;
    sort.mode = SortMode::LastUsed;
    sort.favoritesFirst = true;
    m_proxy->setSortOptions(sort);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    m_edit = new QLineEdit(this);
    m_edit->setPlaceholderText(tr("Type to find a file"));
    m_edit->setAccessibleName(tr("Search files"));
    m_edit->setClearButtonEnabled(true);
    QFont big = m_edit->font();
    big.setPointSizeF(big.pointSizeF() * 1.3);
    m_edit->setFont(big);
    m_edit->installEventFilter(this);
    layout->addWidget(m_edit);

    m_list = new QListView(this);
    m_list->setModel(m_proxy);
    m_delegate = new CardDelegate(loader, this);
    m_delegate->setMode(CardDelegate::Mode::Compact);
    m_list->setItemDelegate(m_delegate);
    m_list->setUniformItemSizes(true);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setAccessibleName(tr("Matching files"));
    m_list->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(m_list, 1);

    connect(m_edit, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_proxy->setQuery(text);
        m_list->setCurrentIndex(m_proxy->index(0, 0));
    });
    connect(m_proxy, &QAbstractItemModel::modelReset, this, [this]() { m_list->setCurrentIndex(m_proxy->index(0, 0)); });
    connect(m_edit, &QLineEdit::returnPressed, this, &QuickOpen::activateCurrent);
    connect(m_list, &QListView::clicked, this, [this](const QModelIndex &) { activateCurrent(); });
    connect(loader, &ThumbnailLoader::thumbnailReady, m_list->viewport(), QOverload<>::of(&QWidget::update));
}

void QuickOpen::popup()
{
    m_edit->clear();
    m_proxy->setQuery(QString());
    m_proxy->invalidate();
    if (QScreen *screen = QGuiApplication::screenAt(QCursor::pos()) ? QGuiApplication::screenAt(QCursor::pos()) : QGuiApplication::primaryScreen()) {
        const QRect area = screen->availableGeometry();
        move(area.center().x() - width() / 2, area.top() + area.height() / 5);
    }
    m_list->setCurrentIndex(m_proxy->index(0, 0));
    show();
    raise();
    activateWindow();
    m_edit->setFocus();
}

void QuickOpen::activateCurrent()
{
    const QModelIndex index = m_list->currentIndex();
    if (!index.isValid()) {
        return;
    }
    const DocumentInfo doc = m_proxy->documentAt(index);
    hide();
    emit openRequested(doc);
}

bool QuickOpen::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_edit && event->type() == QEvent::KeyPress) {
        const auto *key = static_cast<QKeyEvent *>(event);
        const int rows = m_proxy->rowCount();
        int row = m_list->currentIndex().isValid() ? m_list->currentIndex().row() : -1;
        switch (key->key()) {
        case Qt::Key_Down:
            row = qMin(rows - 1, row + 1);
            break;
        case Qt::Key_Up:
            row = qMax(0, row - 1);
            break;
        case Qt::Key_PageDown:
            row = qMin(rows - 1, row + 8);
            break;
        case Qt::Key_PageUp:
            row = qMax(0, row - 8);
            break;
        default:
            return QWidget::eventFilter(watched, event);
        }
        if (rows > 0) {
            m_list->setCurrentIndex(m_proxy->index(row, 0));
        }
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void QuickOpen::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        hide();
        return;
    }
    QWidget::keyPressEvent(event);
}

void QuickOpen::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    emit dismissed();
}

void QuickOpen::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    // Like a spotlight window, it goes away when the user clicks elsewhere
    if (event->type() == QEvent::ActivationChange && !isActiveWindow() && isVisible()) {
        hide();
    }
}

} // namespace gui
} // namespace sheaf
