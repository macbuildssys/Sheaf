#include "tagdialog.h"

#include "database.h"

#include <QCompleter>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QStringListModel>
#include <QVBoxLayout>

namespace sheaf {
namespace gui {

namespace {

// Completes the part after the last comma
class CommaCompleter : public QCompleter {
public:
    CommaCompleter(const QStringList &words, QObject *parent)
        : QCompleter(words, parent)
    {
        setCaseSensitivity(Qt::CaseInsensitive);
    }
    QString pathFromIndex(const QModelIndex &index) const override
    {
        QString text = static_cast<QLineEdit *>(widget())->text();
        const int comma = text.lastIndexOf(QLatin1Char(','));
        const QString word = QCompleter::pathFromIndex(index);
        return comma < 0 ? word : text.left(comma + 1) + QLatin1Char(' ') + word;
    }
    QStringList splitPath(const QString &path) const override
    {
        return {path.mid(path.lastIndexOf(QLatin1Char(',')) + 1).trimmed()};
    }
};

} // namespace

TagDialog::TagDialog(const QStringList &current, const QStringList &known, bool addOnly, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(addOnly ? tr("Add Tags") : tr("Edit Tags"));
    setMinimumWidth(420);
    auto *layout = new QVBoxLayout(this);
    auto *label = new QLabel(addOnly ? tr("Tags to add to the selected files, separated by commas:") : tr("Tags, separated by commas:"), this);
    label->setWordWrap(true);
    layout->addWidget(label);
    m_edit = new QLineEdit(addOnly ? QString() : current.join(QStringLiteral(", ")), this);
    m_edit->setAccessibleName(tr("Tags"));
    m_edit->setCompleter(new CommaCompleter(known, m_edit));
    label->setBuddy(m_edit);
    layout->addWidget(m_edit);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    m_edit->setFocus();
}

QStringList TagDialog::tags() const
{
    QStringList result;
    const QStringList parts = m_edit->text().split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const QString tag = Database::normalizeTag(part);
        if (!tag.isEmpty() && !result.contains(tag, Qt::CaseInsensitive)) {
            result << tag;
        }
    }
    return result;
}

} // namespace gui
} // namespace sheaf
