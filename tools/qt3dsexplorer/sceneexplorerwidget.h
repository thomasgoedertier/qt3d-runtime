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

#ifndef SCENEEXPLORERWIDGET_H
#define SCENEEXPLORERWIDGET_H

#include <QWidget>
QT_BEGIN_NAMESPACE

class Q3DSUipPresentation;
class Q3DSGraphObject;
class QTreeView;
class QtTreePropertyBrowser;
class QtVariantPropertyManager;
class QtVariantEditorFactory;
class QtVariantProperty;
class QtProperty;
class SceneTreeModel;
class Q3DSComponentNode;
class SceneExplorerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SceneExplorerWidget(QWidget *parent = nullptr);

    void setPresentation(Q3DSUipPresentation *presentation);

    void reset();

private slots:
    void handleSelectionChanged(const QModelIndex &current, const QModelIndex &);
    void handleValueChanged(const QtProperty *property, const QVariant &value);
signals:
    void componentSelected(Q3DSComponentNode *component);
private:
    void init();
    void resetPropertyViewer();

    Q3DSUipPresentation *m_presentation = nullptr;
    QTreeView *m_sceneTreeView = nullptr;
    QtTreePropertyBrowser *m_propertyBrowser = nullptr;
    QtVariantPropertyManager *m_variantManager = nullptr;
    QtVariantEditorFactory *m_variantFactory = nullptr;
    SceneTreeModel *m_sceneModel = nullptr;

    // Updates
    QHash<QString, QtVariantProperty *> m_propertyMap;
    Q3DSGraphObject *m_currentObject = nullptr;
    int m_updateCallbackIndex = -1;
};

QT_END_NAMESPACE

#endif // SCENEEXPLORERWIDGET_H
