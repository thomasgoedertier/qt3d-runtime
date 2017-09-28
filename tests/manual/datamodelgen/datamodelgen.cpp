/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QApplication>
#include <QInputDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QSplitter>
#include <QListWidget>
#include <QPushButton>
#include <QPlainTextEdit>
#include "q3dsdatamodelparser.h"

class W : public QWidget
{
public:
    W(const QString &typeName, const QVector<Q3DSDataModelParser::Property> *props);
};

W::W(const QString &typeName, const QVector<Q3DSDataModelParser::Property> *props)
{
    resize(1024, 768);

    QSplitter *splitter = new QSplitter;
    QVBoxLayout *layout = new QVBoxLayout;
    QListWidget *list = new QListWidget;
    for (const Q3DSDataModelParser::Property &prop : *props) {
        QString s;
        if (prop.type != Q3DS::Enum)
            s = tr("%1 %2 [%3]").arg(prop.typeStr).arg(prop.name).arg(prop.defaultValue);
        else
            s = tr("%1(%2) %3 [%4]").arg(prop.typeStr).arg(prop.enumValues.join(", ")).arg(prop.name).arg(prop.defaultValue);
        list->addItem(s);
    }
    list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    layout->addWidget(list);

    QPushButton *btn = new QPushButton(tr("The Button of Doom"));
    connect(btn, &QPushButton::clicked, qApp, &QCoreApplication::quit);
    layout->addWidget(btn);

    QPlainTextEdit *output = new QPlainTextEdit;
    output->setReadOnly(true);
    output->setFont(QFont("Consolas"));

    QWidget *vl = new QWidget;
    vl->setLayout(layout);
    splitter->addWidget(vl);
    splitter->addWidget(output);

    QVBoxLayout *theMainLayout = new QVBoxLayout;
    theMainLayout->addWidget(splitter);
    setLayout(theMainLayout);

    connect(list, &QListWidget::itemSelectionChanged, this, [=]() {
        output->clear();
        const QList<QListWidgetItem *> sel = list->selectedItems();
        if (sel.isEmpty())
            return;

        QString result;
        result += QString(QLatin1String("// setProperties() implementation\n"));
        result += QString(QLatin1String("const QString typeName = QStringLiteral(\"%1\");\n\n")).arg(typeName);

        bool hasEnums = false;
        for (QListWidgetItem *selectedItem : sel) {
            int idx = list->row(selectedItem);
            const Q3DSDataModelParser::Property &prop(props->at(idx));
            if (prop.type == Q3DS::Enum)
                hasEnums = true;
            result += QString(QLatin1String("parseProperty(attrs, flags, typeName, QStringLiteral(\"%1\"), &m_%2);\n")).arg(prop.name).arg(prop.name);
        }

        result += QString(QLatin1String("\n// q3dspresentation.h class members\n"));
        for (QListWidgetItem *selectedItem : sel) {
            int idx = list->row(selectedItem);
            const Q3DSDataModelParser::Property &prop(props->at(idx));
            QString type = prop.type == Q3DS::Enum ? prop.name : prop.typeStr;
            result += QString(QLatin1String("%1 %2() const { return m_%2; }\n")).arg(type).arg(prop.name);
        }
        for (QListWidgetItem *selectedItem : sel) {
            int idx = list->row(selectedItem);
            const Q3DSDataModelParser::Property &prop(props->at(idx));
            QString type = prop.type == Q3DS::Enum ? prop.name : prop.typeStr;
            result += QString(QLatin1String("%1 m_%2;\n")).arg(type).arg(prop.name);
        }

        if (hasEnums)
            result += QString(QLatin1String("\n// enum definitions for q3dspresentation.h\n"));

        for (QListWidgetItem *selectedItem : sel) {
            int idx = list->row(selectedItem);
            const Q3DSDataModelParser::Property &prop(props->at(idx));
            if (prop.type == Q3DS::Enum) {
                result += QString(QLatin1String("\nenum %1 {\n")).arg(prop.name);
                for (int i = 0; i < prop.enumValues.count(); ++i) {
                    result += QString(QLatin1String("    %1%2,\n")).arg(prop.enumValues[i]).arg(i == 0 ? QString(QLatin1String(" = 0")) : QString());
                }
                result += QString(QLatin1String("};\n"));
            }
        }

        if (hasEnums)
            result += QString(QLatin1String("\n// parseProperty overloads for enum types\n"));

        for (QListWidgetItem *selectedItem : sel) {
            int idx = list->row(selectedItem);
            const Q3DSDataModelParser::Property &prop(props->at(idx));
            if (prop.type == Q3DS::Enum) {
                result += QString(QLatin1String("\ntemplate<typename V>\n"
                                                "bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, Q3DS%1Node::%2 *dst)\n"
                                                "{\n"
                                                "    return ::parseProperty<Q3DS%1Node::%2>(attrs, flags, typeName, propName, Q3DS::Enum, dst, [](const QStringRef &s, Q3DS%1Node::%2 *v) { return Q3DSEnumMap::enumFromStr(s, v); });\n"
                                                "}\n")).arg(typeName).arg(prop.name);
            }
        }

        if (hasEnums)
            result += QString(QLatin1String("\n// q3dsenummaps.h specializations for enum types\n"));

        for (QListWidgetItem *selectedItem : sel) {
            int idx = list->row(selectedItem);
            const Q3DSDataModelParser::Property &prop(props->at(idx));
            if (prop.type == Q3DS::Enum) {
                result += QString(QLatin1String("\ntemplate <>\n"
                                                "struct Q3DSEnumParseMap<Q3DS%1Node::%2>\n"
                                                "{\n"
                                                "    static Q3DSEnumNameMap *get();\n"
                                                "};\n")).arg(typeName).arg(prop.name);
            }
        }

        if (hasEnums)
            result += QString(QLatin1String("\n// q3dsenummaps.cpp specializations for enum types\n"));

        for (QListWidgetItem *selectedItem : sel) {
            int idx = list->row(selectedItem);
            const Q3DSDataModelParser::Property &prop(props->at(idx));
            if (prop.type == Q3DS::Enum) {
                result += QString(QLatin1String("\nstatic Q3DSEnumNameMap g_%1Node%2[] = {\n")).arg(typeName).arg(prop.name);
                for (QString v : prop.enumValues) {
                    result += QString(QLatin1String("    { Q3DS%1Node::%2, \"%2\" },\n")).arg(typeName).arg(v);
                }
                result += QString(QLatin1String("    { 0, nullptr }\n"));
                result += QString(QLatin1String("};\n\nQ3DSEnumNameMap *Q3DSEnumParseMap<Q3DS%1Node::%2>::get()\n"
                                                "{\n"
                                                "    return g_%1Node%2;\n"
                                                "}\n")).arg(typeName).arg(prop.name);
            }
        }

        output->setPlainText(result);
    });
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QString typeName = QInputDialog::getText(nullptr, QObject::tr("Type name"), QObject::tr("Type name (e.g. Layer)"));
    if (typeName.isEmpty())
        return 0;

    Q3DSDataModelParser *p = Q3DSDataModelParser::instance();
    const QVector<Q3DSDataModelParser::Property> *props = p->propertiesForType(typeName);

    if (!props) {
        QMessageBox::warning(nullptr, QObject::tr("Error"), QObject::tr("No such type"));
        return 0;
    }

    W w(typeName, props);
    w.show();

    return app.exec();
}
