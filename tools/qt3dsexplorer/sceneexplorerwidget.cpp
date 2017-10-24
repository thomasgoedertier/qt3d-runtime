/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of Qt 3D Studio.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "sceneexplorerwidget.h"

#include <Qt3DStudioRuntime2/Q3DSPresentation>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QSplitter>
#include "qtpropertybrowser/qttreepropertybrowser.h"
#include "qtpropertybrowser/qtvariantproperty.h"

QT_BEGIN_NAMESPACE

class SceneTreeModel : public QAbstractItemModel
{
public:
    SceneTreeModel(Q3DSGraphObject *root);

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    QVariant data(const QModelIndex &index, int role) const override;

private:
    Q3DSGraphObject *m_root;
};

SceneTreeModel::SceneTreeModel(Q3DSGraphObject *root)
    : m_root(root)
{
}

int SceneTreeModel::columnCount(const QModelIndex &) const
{
    return 1;
}

int SceneTreeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        return 1;
    else
        return static_cast<Q3DSGraphObject *>(parent.internalPointer())->childCount();
}

QVariant SceneTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case 0:
            return QLatin1String("Object");
            break;
        default:
            break;
        }
    }

    return QVariant();
}

QModelIndex SceneTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    Q3DSGraphObject *parentItem;

    if (!parent.isValid())
        return createIndex(row, column, m_root);
    else
        parentItem = static_cast<Q3DSGraphObject *>(parent.internalPointer());

    if (row >= parentItem->childCount())
        return QModelIndex();

    return createIndex(row, column, parentItem->childAtIndex(row));
}

QModelIndex SceneTreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    Q3DSGraphObject *childItem = static_cast<Q3DSGraphObject *>(index.internalPointer());
    Q3DSGraphObject *parentItem = childItem->parent();

    if (!parentItem)
        return QModelIndex();

    int row = 0;
    if (parentItem->parent()) {
        for (int i = 0; i < parentItem->parent()->childCount(); ++i) {
            if (parentItem->parent()->childAtIndex(i) == parentItem) {
                row = i;
                break;
            }
        }
    }

    return createIndex(row, 0, parentItem);
}

QVariant SceneTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role != Qt::DisplayRole)
        return QVariant();

    Q3DSGraphObject *item = static_cast<Q3DSGraphObject* >(index.internalPointer());
    QString typeStr = item->gex_typeAsString();
    if (item->type() == Q3DSGraphObject::Slide && item == m_root)
        typeStr = QLatin1String("Master Slide");
    QString s = tr("%1 %2").arg(typeStr).arg((quintptr) item, 0, 16);
    return s;
}

SceneExplorerWidget::SceneExplorerWidget(Q3DSPresentation *presentation, QWidget *parent)
    : QWidget(parent)
    , m_presentation(presentation)
{
    m_sceneModel = new SceneTreeModel(m_presentation->scene());
    init();
    m_sceneTreeView->expandAll();
}

void SceneExplorerWidget::handleSelectionChanged(const QModelIndex &current, const QModelIndex &)
{
    m_propertyBrowser->clear();
    Q3DSGraphObject *obj = static_cast<Q3DSGraphObject *>(current.internalPointer());
    const QStringList pnames = obj->gex_propertyNames();
    const QVariantList pvalues = obj->gex_propertyValues();
    QtProperty *topLevelItem = m_variantManager->addProperty(QtVariantPropertyManager::groupTypeId(), obj->gex_typeAsString());
    Q_ASSERT(pnames.count() == pvalues.count());
    for (int i = 0; i < pnames.count(); ++i) {
        auto item = m_variantManager->addProperty(pvalues[i].type(), pnames[i]);
        if (item) {
            item->setValue(pvalues[i]);
            topLevelItem->addSubProperty(item);
        } else {
            qDebug() << "skipping: " << pnames[i]<< " of type: " << pvalues[i].type();
        }
    }
    m_propertyBrowser->addProperty(topLevelItem);
}

void SceneExplorerWidget::init()
{
    auto mainLayout = new QVBoxLayout(this);

    setLayout(mainLayout);
    auto splitter = new QSplitter(Qt::Vertical, this);
    m_sceneTreeView = new QTreeView(this);
    splitter->addWidget(m_sceneTreeView);
    m_propertyBrowser = new QtTreePropertyBrowser(this);
    m_variantManager = new QtVariantPropertyManager(this);
    m_variantFactory = new QtVariantEditorFactory(this);
    m_propertyBrowser->setFactoryForManager(m_variantManager, m_variantFactory);
    splitter->addWidget(m_propertyBrowser);
    mainLayout->addWidget(splitter);
    m_sceneTreeView->setModel(m_sceneModel);

    connect(m_sceneTreeView->selectionModel(), &QItemSelectionModel::currentChanged, this, &SceneExplorerWidget::handleSelectionChanged);
}

QT_END_NAMESPACE
