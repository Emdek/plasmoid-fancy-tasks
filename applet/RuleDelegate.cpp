/***********************************************************************************
* Fancy Tasks: Plasmoid providing fancy visualization of tasks, launchers and jobs.
* Copyright (C) 2009-2013 Michal Dutkiewicz aka Emdek <emdeck@gmail.com>
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*
***********************************************************************************/

#include "RuleDelegate.h"

#include <QtGui/QCheckBox>
#include <QtGui/QHBoxLayout>

#include <KLocale>
#include <KComboBox>
#include <KLineEdit>

namespace FancyTasks
{

RuleDelegate::RuleDelegate(QObject *parent) : QStyledItemDelegate(parent)
{
}

void RuleDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    const QString rule = index.data(Qt::EditRole).toString();

    if (rule.isEmpty())
    {
        return;
    }

    KLineEdit *ruleLineEdit = static_cast<KLineEdit*>(editor->layout()->itemAt(0)->widget());
    ruleLineEdit->setText(rule.mid(rule.indexOf('+', 3) + 1));

    KComboBox *matchComboBox = static_cast<KComboBox*>(editor->layout()->itemAt(1)->widget());
    matchComboBox->setCurrentIndex(rule.mid(2, (rule.indexOf('+', 3) - 2)).toInt());

    QCheckBox *requiredCheckBox = static_cast<QCheckBox*>(editor->layout()->itemAt(2)->widget());
    requiredCheckBox->setChecked(rule.at(0) == '1');
}

void RuleDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    KLineEdit *ruleLineEdit = static_cast<KLineEdit*>(editor->layout()->itemAt(0)->widget());
    KComboBox *matchComboBox = static_cast<KComboBox*>(editor->layout()->itemAt(1)->widget());
    QCheckBox *requiredCheckBox = static_cast<QCheckBox*>(editor->layout()->itemAt(2)->widget());

    model->setData(index, ((matchComboBox->currentIndex() > 0)?(QString("%1+%2+%3").arg(requiredCheckBox->isChecked()?'1':'0').arg(matchComboBox->currentIndex()).arg(ruleLineEdit->text())):QString()), Qt::EditRole);
}

QString RuleDelegate::displayText(const QVariant &value, const QLocale &locale) const
{
    Q_UNUSED(locale)

    const QString rule = value.toString();

    if (!rule.contains('+'))
    {
        return rule;
    }

    return rule.mid(rule.indexOf('+', 3) + 1);
}

QWidget* RuleDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option)

    QWidget *editor = new QWidget(parent);
    KLineEdit *ruleLineEdit = new KLineEdit(editor);
    ruleLineEdit->setToolTip(i18n("Expression"));

    KComboBox *matchComboBox = new KComboBox(editor);
    matchComboBox->setToolTip(i18n("Match Mode"));
    matchComboBox->addItem(i18n("Ignore"));
    matchComboBox->addItem(i18n("Regular expression"));
    matchComboBox->addItem(i18n("Partial match"));
    matchComboBox->addItem(i18n("Exact match"));

    QCheckBox *requiredCheckBox = new QCheckBox(editor);
    requiredCheckBox->setToolTip(i18n("Required"));

    QHBoxLayout *layout = new QHBoxLayout(editor);
    layout->addWidget(ruleLineEdit);
    layout->addWidget(matchComboBox);
    layout->addWidget(requiredCheckBox);
    layout->setMargin(0);

    setEditorData(editor, index);

    return editor;
}

}
