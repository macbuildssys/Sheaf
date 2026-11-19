#pragma once

#include <QDialog>
#include <QStringList>

class QLineEdit;

namespace sheaf {
namespace gui {

// Edits a comma separated list of tags with completion from the tags in use
class TagDialog : public QDialog {
    Q_OBJECT
public:
    // With addOnly the dialog adds tags to several documents and starts empty
    TagDialog(const QStringList &current, const QStringList &known, bool addOnly, QWidget *parent = nullptr);

    QStringList tags() const;

private:
    QLineEdit *m_edit;
};

} // namespace gui
} // namespace sheaf
