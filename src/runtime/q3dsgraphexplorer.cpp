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

#include "q3dsgraphexplorer_p.h"
#include "q3dsuippresentation_p.h"
#include "q3dsscenemanager_p.h"
#include "q3dsutils_p.h"
#include <QSplitter>
#include <QVBoxLayout>
#include <QTreeView>
#include <QPlainTextEdit>
#include <QAbstractItemModel>
#include <QMenu>
#include <Qt3DCore/QTransform>

QT_BEGIN_NAMESPACE

class TreeModel : public QAbstractItemModel
{
public:
    TreeModel(Q3DSGraphObject *root);

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

TreeModel::TreeModel(Q3DSGraphObject *root)
    : m_root(root)
{
}

int TreeModel::columnCount(const QModelIndex &) const
{
    return 1;
}

int TreeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        return 1;
    else
        return static_cast<Q3DSGraphObject *>(parent.internalPointer())->childCount();
}

QVariant TreeModel::headerData(int section, Qt::Orientation orientation, int role) const
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

QModelIndex TreeModel::index(int row, int column, const QModelIndex &parent) const
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

QModelIndex TreeModel::parent(const QModelIndex &index) const
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

QVariant TreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role != Qt::DisplayRole)
        return QVariant();

    Q3DSGraphObject *item = static_cast<Q3DSGraphObject* >(index.internalPointer());
    QString typeStr = QString::fromLatin1(item->typeAsString());
    if (item->type() == Q3DSGraphObject::Slide && item == m_root)
        typeStr = QLatin1String("Master Slide");
    QString s = tr("%1 %2").arg(typeStr).arg((quintptr) item, 0, 16);
    return s;
}

Q3DSGraphExplorer::Q3DSGraphExplorer(Q3DSGraphObject *root, QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle(tr("Graph explorer for %1 (%2)").arg((quintptr) root, 0, 16).arg(QString::fromLatin1(root->typeAsString())));
    resize(1024, 768);

    QTreeView *tv = new QTreeView;
    tv->setModel(new TreeModel(root));
    tv->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tv, &QTreeView::customContextMenuRequested, this, [=](const QPoint &point) {
        QModelIndex index = tv->indexAt(point);
        if (index.isValid()) {
            Q3DSGraphObject *obj = static_cast<Q3DSGraphObject *>(index.internalPointer());
            if (obj) {
                if (obj->type() == Q3DSGraphObject::Component) {
                    Q3DSComponentNode *comp = static_cast<Q3DSComponentNode *>(obj);
                    QMenu *tvContextMenu = new QMenu(this);
                    QAction *action = new QAction(tr("Show component slide tree"), this);
                    connect(action, &QAction::triggered, this, [comp]() {
                        if (comp->masterSlide())
                            Q3DSUtils::showObjectGraph(comp->masterSlide());
                        else
                            Q3DSUtils::showMessage(tr("No slides for this component??"));
                    });
                    tvContextMenu->addAction(action);
                    tvContextMenu->exec(tv->mapToGlobal(point));
                }
            }
        }
    });

    QPlainTextEdit *props = new QPlainTextEdit;
    props->setReadOnly(true);
#ifdef Q_OS_WIN
    props->setFont(QFont(QLatin1String("Consolas")));
#else
    props->setFont(QFont(QLatin1String("Monospace")));
#endif

    QVBoxLayout *layout = new QVBoxLayout;
    QSplitter *splitter = new QSplitter;
    splitter->addWidget(tv);
    splitter->addWidget(props);
    layout->addWidget(splitter);
    setLayout(layout);

    connect(tv->selectionModel(), &QItemSelectionModel::currentChanged, this, [=](const QModelIndex &current, const QModelIndex &) {
        props->clear();
        Q3DSGraphObject *obj = static_cast<Q3DSGraphObject *>(current.internalPointer());
        QString s = tr("Object %1 of type %2 with %3 children").arg((quintptr) obj, 0, 16).arg(QString::fromLatin1(obj->typeAsString())).arg(obj->childCount());
        const auto pnames = obj->propertyNames();
        const auto pvalues = obj->propertyValues();
        Q_ASSERT(pnames.count() == pvalues.count());
        for (int i = 0; i < pnames.count(); ++i)
            s += tr("\n%1: %2").arg(QString::fromLatin1(pnames[i])).arg(varStr(pvalues[i]));
        auto diProps = obj->dataInputControlledProperties();
        for (auto it = diProps->cbegin(); it != diProps->cend(); ++it)
            s += tr("\nproperty %2 is controlled by data input entry %1").arg(it.key()).arg(it.value());
        if (obj->isNode() && obj->type() != Q3DSGraphObject::Camera) {
            Q3DSNodeAttached *attached = static_cast<Q3DSNodeAttached *>(obj->attached());
            if (attached && attached->transform) {
                s += tr("\n\nAttached generic node data:\nglobalOpacity: %1\nglobalVisibility: %2\nRight-handed local transform: ")
                        .arg(attached->globalOpacity).arg(attached->globalVisibility);
                s += tr("T: %1 R: %2 S: %3")
                        .arg(varStr(attached->transform->translation()))
                        .arg((varStr(attached->transform->rotation().toEulerAngles())))
                        .arg(varStr(attached->transform->scale3D()));
            }
        }

        if (obj->type() == Q3DSGraphObject::Slide) {
            Q3DSSlide *slide = static_cast<Q3DSSlide *>(obj);
            s += tr("\n\nThis slide has %1 objects%2, %3 property changes, and %4 animations")
                    .arg(slide->objects().count()).arg(obj == root ? QString() : QLatin1String(" (excl. master)"))
                    .arg(slide->propertyChanges().count()).arg(slide->animations().count());
            s += tr("\n\nObjects:");
            for (const Q3DSGraphObject *slideObj : slide->objects())
                s += tr("\n%1 %2").arg((quintptr) slideObj, 0, 16).arg(QString::fromUtf8(slideObj->id()));
            if (slide->parent()) {
                s += tr("\n\nProperty changes:");
                const auto &a = slide->propertyChanges();
                for (auto it = a.cbegin(), ite = a.cend(); it != ite; ++it) {
                    s += tr("\nOn object %1 %2").arg((quintptr) it.key(), 0, 16).arg(QString::fromUtf8(it.key()->id()));
                    for (auto pit = it.value()->cbegin(), pite = it.value()->cend(); pit != pite; ++pit)
                        s += tr("\n  %1: %2").arg(pit->nameStr()).arg(pit->valueStr());
                }
            }
            if (!slide->animations().isEmpty()) {
                s += tr("\n\nAnimations:");
                const QVector<Q3DSAnimationTrack> &anims = slide->animations();
                for (const Q3DSAnimationTrack &animTrack : anims) {
                    s += tr("\n  type %1 on object %2 %3 property %4 with %5 keyframes")
                            .arg(animTrack.type()).arg((quintptr) animTrack.target(), 0, 16).arg(QString::fromUtf8(animTrack.target()->id()))
                            .arg(animTrack.property()).arg(animTrack.keyFrames().count());
                    int kfIdx = 0;
                    for (const Q3DSAnimationTrack::KeyFrame &kf : animTrack.keyFrames()) {
                        s += tr("\n    [%1] time %2 value %3").arg(kfIdx).arg(kf.time).arg(kf.value);
                        if (animTrack.type() == Q3DSAnimationTrack::EaseInOut) {
                            s += tr("\n        easeIn %1 easeOut %2").arg(kf.easeIn).arg(kf.easeOut);
                        } else if (animTrack.type() == Q3DSAnimationTrack::Bezier) {
                            s += tr("\n        C2 time %1 C2 value %2 C1 time %3 C1 value %4")
                                    .arg(kf.c2time).arg(kf.c2value).arg(kf.c1time).arg(kf.c1value);
                        }
                        ++kfIdx;
                    }

                }
            }
        } else if (obj->type() == Q3DSGraphObject::Model) {
            Q3DSModelNode *model = static_cast<Q3DSModelNode *>(obj);
            const MeshList m = model->mesh();
            if (!m.isEmpty()) {
                s += tr("\n\nThis model has a mesh with %1 sub-meshes").arg(m.count());
                for (int i = 0; i < m.count(); ++i)
                    s += tr("\nSub-mesh %1 has %2 vertices").arg(i).arg(m.at(i)->vertexCount());
            }
        } else if (obj->type() == Q3DSGraphObject::CustomMaterial) {
            Q3DSCustomMaterialInstance *mat = static_cast<Q3DSCustomMaterialInstance *>(obj);
            auto props = mat->dynamicProperties();
            s += tr("\n\nThis material has %1 custom properties").arg(props.count());
            for (auto it = props.cbegin(), ite = props.cend(); it != ite; ++it)
                s += tr("\n%1: %2").arg(it.key()).arg(varStr(it.value()));
        } else if (obj->type() == Q3DSGraphObject::Effect) {
            Q3DSEffectInstance *mat = static_cast<Q3DSEffectInstance *>(obj);
            auto props = mat->dynamicProperties();
            s += tr("\n\nThis effect has %1 custom properties").arg(props.count());
            for (auto it = props.cbegin(), ite = props.cend(); it != ite; ++it)
                s += tr("\n%1: %2").arg(it.key()).arg(varStr(it.value()));
        } else if (obj->type() == Q3DSGraphObject::Component) {
            Q3DSComponentNode *comp = static_cast<Q3DSComponentNode *>(obj);
            Q3DSSlide *master = comp->masterSlide();
            if (master) {
                s += tr("\n\nThis component has a master slide (%1 %2)\n  with %3 sub-slides.\n  Right-click to examine.")
                     .arg((quintptr) master, 0, 16).arg(QString::fromUtf8(master->id())).arg(master->childCount());
            }
        }

        props->setPlainText(s);
    });
}

QString Q3DSGraphExplorer::varStr(const QVariant &v)
{
    if (v.canConvert<QVector2D>()) {
        QVector3D vv = v.value<QVector2D>();
        return tr("[%1 %2]").arg(vv.x()).arg(vv.y());
    }
    if (v.canConvert<QVector3D>()) {
        QVector3D vv = v.value<QVector3D>();
        return tr("[%1 %2 %3]").arg(vv.x()).arg(vv.y()).arg(vv.z());
    }
    return v.toString();
}

QT_END_NAMESPACE
